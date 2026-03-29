#pragma once

#include "timenav/claim.hpp"
#include "timenav/ids.hpp"
#include "timenav/route.hpp"

namespace timenav {

    struct RobotState {
        RobotId robot_id{0};
        MissionId mission_id{0};
        dp::Optional<zoneout::UUID> current_node_id;
        dp::Optional<zoneout::UUID> current_edge_id;
        dp::Optional<RoutePlan> route_plan;
        dp::Vector<ClaimId> pending_claim_ids;
        dp::Vector<LeaseId> active_lease_ids;
        dp::u64 horizon = 0;
        dp::u64 updated_at_tick = 0;
    };

} // namespace timenav
