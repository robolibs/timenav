#pragma once

#include <memory>
#include <string_view>
#include <unordered_map>

#include <concord/concord.hpp>
#include <zoneout/zoneout.hpp>

namespace timenav {

    struct ValidationIssue {
        enum class Severity { Warning, Error };

        Severity severity = Severity::Error;
        dp::String category;
        dp::String resource_kind;
        dp::Optional<zoneout::UUID> resource_id;
        dp::String message;
    };

    class WorkspaceIndex {
      public:
        WorkspaceIndex() = default;
        explicit WorkspaceIndex(const zoneout::Workspace &workspace) : workspace_(&workspace) { rebuild(); }
        explicit WorkspaceIndex(std::shared_ptr<const zoneout::Workspace> workspace)
            : owned_workspace_(std::move(workspace)), workspace_(owned_workspace_.get()) {
            rebuild();
        }

        [[nodiscard]] bool empty() const noexcept { return workspace_ == nullptr; }
        [[nodiscard]] bool has_workspace() const noexcept { return workspace_ != nullptr; }
        [[nodiscard]] bool owns_workspace() const noexcept { return static_cast<bool>(owned_workspace_); }
        [[nodiscard]] const zoneout::Workspace *workspace() const noexcept { return workspace_; }
        void bind_workspace(const zoneout::Workspace &workspace) {
            owned_workspace_.reset();
            workspace_ = &workspace;
            rebuild();
        }
        void bind_workspace(std::shared_ptr<const zoneout::Workspace> workspace) {
            owned_workspace_ = std::move(workspace);
            workspace_ = owned_workspace_.get();
            rebuild();
        }
        void refresh() { rebuild(); }
        [[nodiscard]] const zoneout::Zone *root_zone() const noexcept {
            return workspace_ == nullptr ? nullptr : &workspace_->root_zone();
        }
        [[nodiscard]] const zoneout::Zone *zone(const zoneout::UUID &zone_id) const noexcept {
            const auto it = zones_.find(zone_id);
            return it == zones_.end() ? nullptr : it->second;
        }
        [[nodiscard]] const zoneout::NodeData *node(const zoneout::UUID &node_id) const noexcept {
            if (workspace_ == nullptr) {
                return nullptr;
            }

            const auto it = nodes_.find(node_id);
            return it == nodes_.end() ? nullptr : &workspace_->graph()[it->second];
        }
        [[nodiscard]] const zoneout::EdgeData *edge(const zoneout::UUID &edge_id) const noexcept {
            if (workspace_ == nullptr) {
                return nullptr;
            }

            const auto it = edges_.find(edge_id);
            return it == edges_.end() ? nullptr : &workspace_->graph().edge_property(it->second);
        }
        [[nodiscard]] const zoneout::Zone *parent_zone(const zoneout::UUID &zone_id) const noexcept {
            const auto it = parents_.find(zone_id);
            return it == parents_.end() ? nullptr : it->second;
        }
        [[nodiscard]] dp::Vector<const zoneout::Zone *> child_zones(const zoneout::UUID &zone_id) const {
            const auto it = children_.find(zone_id);
            return it == children_.end() ? dp::Vector<const zoneout::Zone *>{} : it->second;
        }
        [[nodiscard]] dp::Vector<const zoneout::Zone *> ancestor_zones(const zoneout::UUID &zone_id) const {
            dp::Vector<const zoneout::Zone *> ancestors;

            for (auto *current = parent_zone(zone_id); current != nullptr; current = parent_zone(current->id())) {
                ancestors.push_back(current);
            }

            return ancestors;
        }
        [[nodiscard]] dp::Vector<const zoneout::Zone *> descendant_zones(const zoneout::UUID &zone_id) const {
            dp::Vector<const zoneout::Zone *> descendants;
            collect_descendants(zone_id, descendants);
            return descendants;
        }
        [[nodiscard]] dp::Vector<const zoneout::NodeData *> nodes_in_zone(const zoneout::UUID &zone_id) const {
            dp::Vector<const zoneout::NodeData *> zone_nodes;

            if (workspace_ == nullptr) {
                return zone_nodes;
            }

            const auto it = nodes_by_zone_.find(zone_id);
            if (it == nodes_by_zone_.end()) {
                return zone_nodes;
            }

            for (const auto vertex_id : it->second) {
                zone_nodes.push_back(&workspace_->graph()[vertex_id]);
            }

            return zone_nodes;
        }
        [[nodiscard]] dp::Vector<const zoneout::Zone *> zones_of_node(const zoneout::UUID &node_id) const {
            dp::Vector<const zoneout::Zone *> node_zones;

            if (const auto *node_data = node(node_id); node_data != nullptr) {
                for (const auto &zone_id : node_data->zone_ids) {
                    if (const auto *zone_data = zone(zone_id); zone_data != nullptr) {
                        node_zones.push_back(zone_data);
                    }
                }
            }

            return node_zones;
        }
        [[nodiscard]] dp::Vector<const zoneout::Zone *> zones_of_edge(const zoneout::UUID &edge_id) const {
            dp::Vector<const zoneout::Zone *> edge_zones;

            if (const auto *edge_data = edge(edge_id); edge_data != nullptr) {
                for (const auto &zone_id : edge_data->zone_ids) {
                    if (const auto *zone_data = zone(zone_id); zone_data != nullptr) {
                        edge_zones.push_back(zone_data);
                    }
                }
            }

            return edge_zones;
        }
        [[nodiscard]] const zoneout::EdgeData *edge_between(const zoneout::UUID &node_a_id,
                                                            const zoneout::UUID &node_b_id) const noexcept {
            if (workspace_ == nullptr) {
                return nullptr;
            }

            const auto node_a_it = nodes_.find(node_a_id);
            const auto node_b_it = nodes_.find(node_b_id);
            if (node_a_it == nodes_.end() || node_b_it == nodes_.end()) {
                return nullptr;
            }

            const auto edge_id = workspace_->graph().get_edge(node_a_it->second, node_b_it->second);
            if (edge_id.has_value()) {
                return &workspace_->graph().edge_property(*edge_id);
            }

            const auto reverse_edge_id = workspace_->graph().get_edge(node_b_it->second, node_a_it->second);
            return reverse_edge_id.has_value() ? &workspace_->graph().edge_property(*reverse_edge_id) : nullptr;
        }
        [[nodiscard]] dp::Optional<dp::Geo> ref() const {
            if (workspace_ == nullptr || !workspace_->has_ref()) {
                return dp::nullopt;
            }

            return workspace_->ref();
        }
        [[nodiscard]] dp::Optional<zoneout::CoordMode> coord_mode() const {
            if (workspace_ == nullptr) {
                return dp::nullopt;
            }

            return workspace_->coord_mode();
        }
        // `concord::` conversions are only valid when the workspace is explicitly local and has a reference origin.
        // Global workspaces may carry a reference, but this index treats it as metadata and does not convert through
        // it.
        [[nodiscard]] dp::Result<dp::Geo> local_to_global(const dp::Point &local_point) const {
            if (workspace_ == nullptr) {
                return dp::Result<dp::Geo>::err(dp::Error::invalid_argument("local_to_global requires a workspace"));
            }
            if (workspace_->coord_mode() != zoneout::CoordMode::Local) {
                return dp::Result<dp::Geo>::err(
                    dp::Error::invalid_argument("local_to_global requires local coord_mode and a reference origin"));
            }
            const auto reference = ref();
            if (!reference.has_value()) {
                return dp::Result<dp::Geo>::err(
                    dp::Error::invalid_argument("local_to_global requires local coord_mode and a reference origin"));
            }

            const auto wgs = concord::frame::to_wgs(concord::frame::ENU{local_point, *reference});
            return dp::Result<dp::Geo>::ok(wgs.geo());
        }
        // The inverse conversion follows the same rule: local coord mode plus reference origin are required.
        [[nodiscard]] dp::Result<dp::Point> global_to_local(const dp::Geo &global_point) const {
            if (workspace_ == nullptr) {
                return dp::Result<dp::Point>::err(dp::Error::invalid_argument("global_to_local requires a workspace"));
            }
            if (workspace_->coord_mode() != zoneout::CoordMode::Local) {
                return dp::Result<dp::Point>::err(
                    dp::Error::invalid_argument("global_to_local requires local coord_mode and a reference origin"));
            }
            const auto reference = ref();
            if (!reference.has_value()) {
                return dp::Result<dp::Point>::err(
                    dp::Error::invalid_argument("global_to_local requires local coord_mode and a reference origin"));
            }

            const auto enu = concord::frame::to_enu(*reference, concord::earth::WGS{global_point});
            return dp::Result<dp::Point>::ok(enu.point());
        }
        [[nodiscard]] dp::Optional<dp::String> zone_property(const zoneout::UUID &zone_id, std::string_view key) const {
            const auto *zone_data = zone(zone_id);
            if (zone_data == nullptr) {
                return dp::nullopt;
            }

            const auto property_it = zone_data->properties().find(std::string(key));
            if (property_it == zone_data->properties().end()) {
                return dp::nullopt;
            }

            return dp::String{property_it->second};
        }
        [[nodiscard]] dp::Optional<dp::String> edge_property(const zoneout::UUID &edge_id, std::string_view key) const {
            const auto *edge_data = edge(edge_id);
            if (edge_data == nullptr) {
                return dp::nullopt;
            }

            const auto property_it = edge_data->properties.find(std::string(key));
            if (property_it == edge_data->properties.end()) {
                return dp::nullopt;
            }

            return dp::String{property_it->second};
        }
        [[nodiscard]] dp::Vector<ValidationIssue> validation_issues() const {
            dp::Vector<ValidationIssue> issues;

            if (workspace_ == nullptr) {
                return issues;
            }

            if (workspace_->coord_mode() == zoneout::CoordMode::Local && !workspace_->has_ref()) {
                issues.push_back(ValidationIssue{
                    ValidationIssue::Severity::Error, dp::String{"invalid_reference"}, dp::String{"workspace"},
                    dp::nullopt, dp::String{"workspace uses local coordinates without a reference origin"}});
            }
            if (workspace_->coord_mode() == zoneout::CoordMode::Global && workspace_->has_ref()) {
                issues.push_back(ValidationIssue{ValidationIssue::Severity::Warning, dp::String{"ignored_reference"},
                                                 dp::String{"workspace"}, dp::nullopt,
                                                 dp::String{"workspace reference origin is set but coord_mode is "
                                                            "global; concord conversions are disabled"}});
            }

            for (const auto &[zone_id, zone_data] : zones_) {
                if (zone_id.isNull()) {
                    issues.push_back(ValidationIssue{ValidationIssue::Severity::Error, dp::String{"missing_id"},
                                                     dp::String{"zone"}, dp::nullopt,
                                                     dp::String{"zone is missing a non-null id"}});
                }

                for (const auto &node_id : zone_data->node_ids()) {
                    if (node(node_id) == nullptr) {
                        issues.push_back(ValidationIssue{
                            ValidationIssue::Severity::Error, dp::String{"broken_membership"}, dp::String{"zone"},
                            zone_id, dp::String{"zone references node id that is not present in the workspace graph"}});
                        continue;
                    }
                    const auto *node_data = node(node_id);
                    if (node_data != nullptr && std::find(node_data->zone_ids.begin(), node_data->zone_ids.end(),
                                                          zone_id) == node_data->zone_ids.end()) {
                        issues.push_back(ValidationIssue{
                            ValidationIssue::Severity::Error, dp::String{"inconsistent_membership"}, dp::String{"zone"},
                            zone_id, dp::String{"zone lists node membership but the node does not list the zone"}});
                    }
                }
            }

            for (const auto &[node_id, vertex_id] : nodes_) {
                const auto &node_data = workspace_->graph()[vertex_id];
                if (node_id.isNull()) {
                    issues.push_back(ValidationIssue{ValidationIssue::Severity::Error, dp::String{"missing_id"},
                                                     dp::String{"node"}, dp::nullopt,
                                                     dp::String{"node is missing a non-null id"}});
                }

                for (const auto &zone_id : node_data.zone_ids) {
                    if (zone(zone_id) == nullptr) {
                        issues.push_back(ValidationIssue{
                            ValidationIssue::Severity::Error, dp::String{"broken_membership"}, dp::String{"node"},
                            node_id, dp::String{"node references zone id that is not present in the workspace tree"}});
                    }
                }
            }

            for (const auto &[edge_uuid, edge_id] : edges_) {
                const auto &edge_data = workspace_->graph().edge_property(edge_id);
                if (edge_uuid.isNull()) {
                    issues.push_back(ValidationIssue{ValidationIssue::Severity::Error, dp::String{"missing_id"},
                                                     dp::String{"edge"}, dp::nullopt,
                                                     dp::String{"edge is missing a non-null id"}});
                }

                const auto source_vertex = workspace_->graph().source(edge_id);
                const auto target_vertex = workspace_->graph().target(edge_id);
                const auto &source_node = workspace_->graph()[source_vertex];
                const auto &target_node = workspace_->graph()[target_vertex];
                for (const auto &zone_id : edge_data.zone_ids) {
                    if (zone(zone_id) == nullptr) {
                        issues.push_back(ValidationIssue{
                            ValidationIssue::Severity::Error, dp::String{"broken_membership"}, dp::String{"edge"},
                            edge_uuid,
                            dp::String{"edge references zone id that is not present in the workspace tree"}});
                        continue;
                    }
                    const bool source_has_zone = std::find(source_node.zone_ids.begin(), source_node.zone_ids.end(),
                                                           zone_id) != source_node.zone_ids.end();
                    const bool target_has_zone = std::find(target_node.zone_ids.begin(), target_node.zone_ids.end(),
                                                           zone_id) != target_node.zone_ids.end();
                    if (!source_has_zone && !target_has_zone) {
                        issues.push_back(ValidationIssue{
                            ValidationIssue::Severity::Warning, dp::String{"inconsistent_membership"},
                            dp::String{"edge"}, edge_uuid,
                            dp::String{"edge lists a zone that is not present on either endpoint node"}});
                    }
                }
            }

            return issues;
        }
        [[nodiscard]] bool is_valid() const { return validation_issues().empty(); }
        [[nodiscard]] dp::Optional<zoneout::UUID> root_zone_id() const {
            if (const auto *zone = root_zone(); zone != nullptr) {
                return zone->id();
            }

            return dp::nullopt;
        }

      private:
        void collect_descendants(const zoneout::UUID &zone_id, dp::Vector<const zoneout::Zone *> &descendants) const {
            const auto direct_children = child_zones(zone_id);
            for (const auto *child : direct_children) {
                descendants.push_back(child);
                collect_descendants(child->id(), descendants);
            }
        }

        void index_zone_tree(const zoneout::Zone &zone, const zoneout::Zone *parent) {
            zones_[zone.id()] = &zone;
            if (parent != nullptr) {
                parents_[zone.id()] = parent;
                children_[parent->id()].push_back(&zone);
            }

            for (const auto &child : zone.children()) {
                index_zone_tree(child, &zone);
            }
        }

        void rebuild() {
            zones_.clear();
            nodes_.clear();
            edges_.clear();
            parents_.clear();
            children_.clear();
            nodes_by_zone_.clear();

            if (workspace_ == nullptr) {
                return;
            }

            index_zone_tree(workspace_->root_zone(), nullptr);
            for (const auto vertex_id : workspace_->graph().vertices()) {
                nodes_[workspace_->graph()[vertex_id].id] = vertex_id;
                for (const auto &zone_id : workspace_->graph()[vertex_id].zone_ids) {
                    nodes_by_zone_[zone_id].push_back(vertex_id);
                }
            }
            for (const auto &edge : workspace_->graph().edges()) {
                edges_[workspace_->graph().edge_property(edge.id).id] = edge.id;
            }
        }

        std::shared_ptr<const zoneout::Workspace> owned_workspace_{};
        const zoneout::Workspace *workspace_ = nullptr;
        std::unordered_map<zoneout::UUID, const zoneout::Zone *, zoneout::UUIDHash> zones_{};
        std::unordered_map<zoneout::UUID, zoneout::Graph::VertexId, zoneout::UUIDHash> nodes_{};
        std::unordered_map<zoneout::UUID, graphix::vertex::EdgeId, zoneout::UUIDHash> edges_{};
        std::unordered_map<zoneout::UUID, const zoneout::Zone *, zoneout::UUIDHash> parents_{};
        std::unordered_map<zoneout::UUID, dp::Vector<const zoneout::Zone *>, zoneout::UUIDHash> children_{};
        std::unordered_map<zoneout::UUID, dp::Vector<zoneout::Graph::VertexId>, zoneout::UUIDHash> nodes_by_zone_{};
    };

} // namespace timenav
