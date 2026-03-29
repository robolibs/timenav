#pragma once

#include <datapod/datapod.hpp>

namespace timenav::vda {

    struct TypeSpecification {
        dp::Optional<dp::f64> max_speed;
        dp::Optional<dp::f64> max_payload;
    };

    struct Factsheet {
        dp::String manufacturer;
        dp::String serial_number;
        dp::String protocol_version = "3.0.0";
        dp::Optional<dp::String> agv_class;
        dp::Optional<dp::String> software_version;
        TypeSpecification type_specification{};
        dp::Vector<dp::String> supported_actions;
    };

} // namespace timenav::vda
