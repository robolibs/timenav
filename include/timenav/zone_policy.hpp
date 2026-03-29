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

} // namespace timenav
