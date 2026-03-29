#pragma once

#include <memory>
#include <unordered_map>

#include <zoneout/zoneout.hpp>

namespace timenav {

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
