#pragma once

#include <datapod/datapod.hpp>

namespace timenav::vda {

    struct ActionReference {
        dp::String action_id;
        dp::String action_type;
        bool blocking = false;
    };

    struct OrderNode {
        dp::String node_id;
        dp::String sequence_id;
        bool released = false;
        dp::Optional<dp::String> zone_id;
        dp::Vector<ActionReference> actions;
    };

    struct OrderEdge {
        dp::String edge_id;
        dp::String start_node_id;
        dp::String end_node_id;
        bool released = false;
        dp::Optional<dp::String> zone_id;
        dp::Optional<dp::f64> max_speed;
        dp::Vector<ActionReference> actions;
    };

    struct Order {
        dp::String order_id;
        dp::u32 order_update_id = 0;
        dp::String version = "3.0.0";
        dp::Vector<OrderNode> nodes;
        dp::Vector<OrderEdge> edges;
    };

} // namespace timenav::vda
