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
        [[nodiscard]] dp::u64 robot_count() const noexcept { return robot_states_.size(); }
        [[nodiscard]] const dp::Vector<RobotState> &robot_states() const noexcept { return robot_states_; }

        void register_robot(const RobotState &state) {
            if (auto *existing = find_robot_state(state.robot_id); existing != nullptr) {
                *existing = state;
                return;
            }

            robot_states_.push_back(state);
        }

        [[nodiscard]] RobotState *find_robot_state(RobotId robot_id) noexcept {
            for (auto &state : robot_states_) {
                if (state.robot_id == robot_id) {
                    return &state;
                }
            }

            return nullptr;
        }

        [[nodiscard]] const RobotState *find_robot_state(RobotId robot_id) const noexcept {
            for (const auto &state : robot_states_) {
                if (state.robot_id == robot_id) {
                    return &state;
                }
            }

            return nullptr;
        }

      private:
        const WorkspaceIndex *index_ = nullptr;
        ClaimManager claim_manager_{};
        dp::Vector<RobotState> robot_states_{};
    };

} // namespace timenav
