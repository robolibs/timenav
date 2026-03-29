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

      private:
        std::shared_ptr<const zoneout::Workspace> owned_workspace_{};
        const zoneout::Workspace *workspace_ = nullptr;
    };

} // namespace timenav
