#pragma once

#include <datapod/datapod.hpp>

namespace timenav::vda {

    enum class OperatingMode { Manual, Automatic, Semiautomatic };
    enum class ConnectionState { Online, Offline, ConnectionBroken };

    struct BatteryState {
        dp::Optional<dp::f64> battery_charge;
        bool charging = false;
    };

    struct State {
        dp::String agv_id;
        OperatingMode operating_mode = OperatingMode::Manual;
        ConnectionState connection_state = ConnectionState::Offline;
        dp::Optional<dp::String> last_node_id;
        dp::Optional<dp::String> last_edge_id;
        dp::Optional<dp::String> order_id;
        dp::Optional<dp::String> driving_state;
        BatteryState battery_state{};
        dp::Vector<dp::String> action_states;
        dp::Vector<dp::String> errors;
    };

} // namespace timenav::vda
