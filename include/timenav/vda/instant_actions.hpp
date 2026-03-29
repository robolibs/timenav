#pragma once

#include <datapod/datapod.hpp>

namespace timenav::vda {

    enum class ActionBlockingType { None, Soft, Hard };

    struct InstantAction {
        dp::String action_id;
        dp::String action_type;
        ActionBlockingType blocking_type = ActionBlockingType::None;
        dp::Map<dp::String, dp::String> parameters;
    };

    struct InstantActions {
        dp::String header_id;
        dp::Vector<InstantAction> actions;
    };

} // namespace timenav::vda
