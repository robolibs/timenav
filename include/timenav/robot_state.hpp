#pragma once

#include "timenav/claim.hpp"
#include "timenav/ids.hpp"
#include "timenav/route.hpp"

namespace timenav {

    enum class RobotProgressState { Idle, FollowingRoute, Waiting, Queued, Blocked, Replanning };

    struct RobotState {
        RobotId robot_id{0};
        MissionId mission_id{0};
        dp::Optional<zoneout::UUID> current_node_id;
        dp::Optional<zoneout::UUID> current_edge_id;
        dp::Optional<RoutePlan> route_plan;
        dp::Vector<ClaimId> pending_claim_ids;
        dp::Vector<LeaseId> active_lease_ids;
        RobotProgressState progress_state = RobotProgressState::Idle;
        dp::u64 next_route_step_index = 0;
        dp::Optional<dp::String> hold_reason;
        dp::Optional<dp::u64> last_claim_tick;
        dp::Optional<dp::u64> scheduled_start_tick;
        dp::Optional<dp::u64> reserved_until_tick;
        dp::u64 wait_ticks = 0;
        bool needs_replan = false;
        dp::u64 horizon = 0;
        dp::u64 updated_at_tick = 0;
    };

} // namespace timenav
