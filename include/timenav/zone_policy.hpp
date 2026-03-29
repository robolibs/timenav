#pragma once

#include <datapod/datapod.hpp>

namespace timenav {

    enum class ZonePolicyKind {
        Informational,
        ExclusiveAccess,
        SharedAccess,
        CapacityLimited,
        Corridor,
        Replanning,
        Restricted,
        NoStop,
        Slowdown
    };

    struct ZonePolicy {
        ZonePolicyKind kind = ZonePolicyKind::Informational;
        dp::u64 capacity = 1;
        bool requires_claim = false;
        bool blocks_traversal_without_grant = false;
        bool blocks_entry_without_grant = false;
        dp::Map<dp::String, dp::String> properties;
    };

    struct EdgeTrafficSemantics {
        bool directed = false;
        dp::Optional<dp::f64> speed_limit;
        dp::Optional<dp::String> lane_type;
        dp::Optional<bool> reversible;
        dp::Optional<bool> passing_allowed;
        dp::Optional<dp::f64> priority;
        dp::Optional<dp::u64> capacity;
        dp::Optional<dp::f64> clearance_width;
        dp::Optional<dp::f64> clearance_height;
        dp::Optional<dp::String> surface_type;
        dp::Optional<dp::String> robot_class;
        dp::Optional<dp::String> allowed_payload;
        dp::Optional<dp::f64> cost_bias;
        dp::Optional<bool> no_stop;
        dp::Optional<dp::String> preferred_direction;
        dp::Map<dp::String, dp::String> properties;
    };

} // namespace timenav
