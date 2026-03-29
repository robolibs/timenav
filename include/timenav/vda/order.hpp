#pragma once

#include <datapod/datapod.hpp>

namespace timenav::vda {

    // These transport structs are intentionally partial VDA5050 3.0.0-shaped mappings.
    // They are compatibility helpers for this library's internal model, not a full schema clone.

    struct ActionReference {
        dp::String action_id;
        dp::String action_type;
        bool blocking = false;
    };

    struct ResourceReservation {
        dp::String target_id;
        dp::String target_kind;
        bool requires_claim = false;
        dp::Optional<dp::String> access_group;
        dp::Optional<dp::String> schedule_window;
    };

    struct OrderNode {
        dp::String node_id;
        dp::String sequence_id;
        bool released = false;
        dp::Optional<dp::String> zone_id;
        dp::Optional<dp::String> node_position_hint;
        dp::Vector<ResourceReservation> reservations;
        dp::Vector<ActionReference> actions;
    };

    struct OrderEdge {
        dp::String edge_id;
        dp::String start_node_id;
        dp::String end_node_id;
        bool released = false;
        dp::Optional<dp::String> zone_id;
        dp::Optional<dp::f64> max_speed;
        bool bidirectional = true;
        dp::Vector<ResourceReservation> reservations;
        dp::Vector<ActionReference> actions;
    };

    struct Order {
        dp::String header_id;
        dp::String order_id;
        dp::u32 order_update_id = 0;
        dp::String version = "3.0.0";
        dp::Optional<dp::u64> timestamp_ms;
        dp::Vector<OrderNode> nodes;
        dp::Vector<OrderEdge> edges;
    };

} // namespace timenav::vda
