#pragma once

#include <datapod/datapod.hpp>

namespace timenav::vda {

    // These state structs remain deliberately partial. They model a stable subset of VDA5050-like runtime state
    // needed by timenav rather than claiming full protocol parity.

    enum class OperatingMode { Manual, Automatic, Semiautomatic };
    enum class ConnectionState { Online, Offline, ConnectionBroken };

    struct BatteryState {
        dp::Optional<dp::f64> battery_charge;
        bool charging = false;
    };

    struct ReservationState {
        dp::String target_id;
        dp::String target_kind;
        dp::String state;
    };

    struct State {
        dp::String agv_id;
        OperatingMode operating_mode = OperatingMode::Manual;
        ConnectionState connection_state = ConnectionState::Offline;
        dp::u32 order_update_id = 0;
        dp::Optional<dp::String> last_node_id;
        dp::Optional<dp::String> last_edge_id;
        dp::Optional<dp::String> order_id;
        dp::Optional<dp::String> driving_state;
        bool paused = false;
        BatteryState battery_state{};
        dp::Vector<ReservationState> reservation_states;
        dp::Vector<dp::String> action_states;
        dp::Vector<dp::String> errors;
        dp::Vector<dp::String> information;
    };

} // namespace timenav::vda
