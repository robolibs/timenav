#pragma once

#include "timenav/claim.hpp"
#include "timenav/workspace_index.hpp"

namespace timenav {

    class ClaimManager {
      public:
        ClaimManager() = default;
        explicit ClaimManager(const WorkspaceIndex &index) : index_(&index) {}

        [[nodiscard]] bool empty() const noexcept { return request_count() == 0 && lease_count() == 0; }
        [[nodiscard]] const WorkspaceIndex *index() const noexcept { return index_; }
        [[nodiscard]] dp::u64 request_count() const noexcept { return 0; }
        [[nodiscard]] dp::u64 lease_count() const noexcept { return 0; }

      private:
        const WorkspaceIndex *index_ = nullptr;
    };

} // namespace timenav
