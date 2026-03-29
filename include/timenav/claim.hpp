#pragma once

#include "timenav/ids.hpp"
#include <datapod/datapod.hpp>
#include <zoneout/zoneout.hpp>

namespace timenav {

    enum class ClaimTargetKind { Zone, Node, Edge };

    enum class ClaimAccessMode { Shared, Exclusive };

    enum class ClaimDecision { Grant, Deny };

    struct ClaimTarget {
        ClaimTargetKind kind = ClaimTargetKind::Zone;
        zoneout::UUID resource_id = zoneout::UUID::null();
    };

    struct ClaimRequest {
        ClaimId id{0};
        RobotId robot_id{0};
        MissionId mission_id{0};
        ClaimAccessMode access_mode = ClaimAccessMode::Exclusive;
        dp::u32 priority = 0;
        dp::Vector<ClaimTarget> targets;
    };

    struct Lease {
        LeaseId id{0};
        ClaimId claim_id{0};
        RobotId robot_id{0};
        ClaimAccessMode access_mode = ClaimAccessMode::Exclusive;
        dp::Vector<ClaimTarget> targets;
        dp::Optional<dp::u64> expires_at_tick;
        bool active = true;
    };

    struct ClaimEvaluation {
        ClaimDecision decision = ClaimDecision::Grant;
        dp::String reason;
        dp::Optional<ClaimId> conflicting_claim_id;
        dp::Optional<LeaseId> conflicting_lease_id;
    };

} // namespace timenav
