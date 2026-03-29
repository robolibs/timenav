#pragma once

#include "timenav/ids.hpp"
#include <datapod/datapod.hpp>
#include <zoneout/zoneout.hpp>

namespace timenav {

    enum class ClaimTargetKind { Zone, Node, Edge };

    enum class ClaimAccessMode { Shared, Exclusive };

    enum class ClaimDecision { Grant, Deny };

    enum class LeaseDisposition { Active, Released, Expired, Revoked };

    struct ClaimWindow {
        dp::Optional<dp::u64> start_tick;
        dp::Optional<dp::u64> end_tick;
    };

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
        dp::Optional<dp::u64> requested_at_tick;
        ClaimWindow window{};
        dp::Vector<ClaimTarget> targets;
    };

    struct Lease {
        LeaseId id{0};
        ClaimId claim_id{0};
        RobotId robot_id{0};
        ClaimAccessMode access_mode = ClaimAccessMode::Exclusive;
        dp::Vector<ClaimTarget> targets;
        dp::Optional<dp::u64> granted_at_tick;
        dp::Optional<dp::u64> expires_at_tick;
        dp::Optional<dp::u64> refreshed_at_tick;
        dp::Optional<dp::u64> released_at_tick;
        dp::Optional<dp::u64> revoked_at_tick;
        dp::Optional<dp::String> revoke_reason;
        LeaseDisposition disposition = LeaseDisposition::Active;
        bool active = true;
    };

    struct ClaimEvaluation {
        ClaimDecision decision = ClaimDecision::Grant;
        dp::String reason;
        dp::Optional<ClaimId> conflicting_claim_id;
        dp::Optional<LeaseId> conflicting_lease_id;
        dp::Vector<ClaimTarget> conflicting_targets;
        dp::Optional<ClaimTarget> blocking_target;
        dp::Vector<dp::String> diagnostics;
    };

} // namespace timenav
