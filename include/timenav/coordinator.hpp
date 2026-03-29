#pragma once

#include "timenav/claim_manager.hpp"
#include "timenav/robot_state.hpp"

namespace timenav {

    class Coordinator {
      public:
        Coordinator() = default;
        explicit Coordinator(const WorkspaceIndex &index) : index_(&index), claim_manager_(index) {}

        [[nodiscard]] const WorkspaceIndex *index() const noexcept { return index_; }
        [[nodiscard]] const ClaimManager &claim_manager() const noexcept { return claim_manager_; }
        [[nodiscard]] ClaimManager &claim_manager() noexcept { return claim_manager_; }
        [[nodiscard]] bool empty() const noexcept { return robot_count() == 0; }
        [[nodiscard]] dp::u64 robot_count() const noexcept { return 0; }

      private:
        const WorkspaceIndex *index_ = nullptr;
        ClaimManager claim_manager_{};
    };

} // namespace timenav
