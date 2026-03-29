#pragma once

#include <datapod/datapod.hpp>

namespace timenav::vda {

    struct State {
        dp::String agv_id;
        dp::String operating_mode;
        dp::Optional<dp::String> last_node_id;
        dp::Optional<dp::String> last_edge_id;
        dp::Vector<dp::String> errors;
    };

} // namespace timenav::vda
