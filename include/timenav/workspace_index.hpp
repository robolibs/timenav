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
        [[nodiscard]] dp::Optional<zoneout::UUID> root_zone_id() const {
            if (const auto *zone = root_zone(); zone != nullptr) {
                return zone->id();
            }

            return dp::nullopt;
        }

      private:
        void rebuild() {
            zones_.clear();
            nodes_.clear();

            if (workspace_ == nullptr) {
                return;
            }

            workspace_->root_zone().visit(
                [this](const zoneout::Zone &zone, std::size_t) { zones_[zone.id()] = &zone; });
            for (const auto vertex_id : workspace_->graph().vertices()) {
                nodes_[workspace_->graph()[vertex_id].id] = vertex_id;
            }
        }

        std::shared_ptr<const zoneout::Workspace> owned_workspace_{};
        const zoneout::Workspace *workspace_ = nullptr;
        std::unordered_map<zoneout::UUID, const zoneout::Zone *, zoneout::UUIDHash> zones_{};
        std::unordered_map<zoneout::UUID, zoneout::Graph::VertexId, zoneout::UUIDHash> nodes_{};
    };

} // namespace timenav
