#pragma once

#include <datapod/datapod.hpp>

namespace timenav::vda {

    enum class ActionStatus { Accepted, Rejected, Running, Finished, Failed };

    struct Response {
        dp::String action_id;
        ActionStatus status = ActionStatus::Rejected;
        dp::Optional<dp::String> description;
        dp::Optional<dp::String> result_code;
    };

} // namespace timenav::vda
