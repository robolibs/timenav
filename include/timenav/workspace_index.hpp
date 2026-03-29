#pragma once

#include <memory>

#include <zoneout/zoneout.hpp>

namespace timenav {

    class WorkspaceIndex {
      public:
        WorkspaceIndex() = default;
        explicit WorkspaceIndex(const zoneout::Workspace &workspace) : workspace_(&workspace) {}
        explicit WorkspaceIndex(std::shared_ptr<const zoneout::Workspace> workspace)
            : owned_workspace_(std::move(workspace)), workspace_(owned_workspace_.get()) {}

        [[nodiscard]] bool empty() const noexcept { return workspace_ == nullptr; }
        [[nodiscard]] bool has_workspace() const noexcept { return workspace_ != nullptr; }
        [[nodiscard]] bool owns_workspace() const noexcept { return static_cast<bool>(owned_workspace_); }
        [[nodiscard]] const zoneout::Workspace *workspace() const noexcept { return workspace_; }
        [[nodiscard]] const zoneout::Zone *root_zone() const noexcept {
            return workspace_ == nullptr ? nullptr : &workspace_->root_zone();
        }
        [[nodiscard]] dp::Optional<zoneout::UUID> root_zone_id() const {
            if (const auto *zone = root_zone(); zone != nullptr) {
                return zone->id();
            }

            return dp::nullopt;
        }

      private:
        std::shared_ptr<const zoneout::Workspace> owned_workspace_{};
        const zoneout::Workspace *workspace_ = nullptr;
    };

} // namespace timenav
