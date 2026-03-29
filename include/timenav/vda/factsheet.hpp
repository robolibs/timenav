#pragma once

#include <datapod/datapod.hpp>

namespace timenav::vda {

    struct Factsheet {
        dp::String manufacturer;
        dp::String serial_number;
        dp::String protocol_version = "3.0.0";
        dp::Vector<dp::String> supported_actions;
    };

} // namespace timenav::vda
