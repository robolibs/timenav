#pragma once

#include <datapod/datapod.hpp>

namespace timenav::vda {

    struct Response {
        dp::String action_id;
        bool accepted = false;
        dp::Optional<dp::String> description;
    };

} // namespace timenav::vda
