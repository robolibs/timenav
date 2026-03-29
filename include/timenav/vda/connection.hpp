#pragma once

#include <datapod/datapod.hpp>

namespace timenav::vda {

    struct Connection {
        dp::String interface_name;
        dp::String manufacturer;
        dp::String serial_number;
        dp::String version = "3.0.0";
    };

} // namespace timenav::vda
