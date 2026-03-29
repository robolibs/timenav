#pragma once

#include <datapod/datapod.hpp>

namespace timenav::vda {

    enum class ConnectionStatus { Online, Offline, ConnectionBroken };

    struct Connection {
        dp::String interface_name;
        dp::String manufacturer;
        dp::String serial_number;
        dp::String version = "3.0.0";
        dp::Optional<dp::String> connection_id;
        ConnectionStatus status = ConnectionStatus::Offline;
        dp::Optional<dp::u64> timestamp_ms;
    };

} // namespace timenav::vda
