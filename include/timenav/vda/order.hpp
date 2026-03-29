#pragma once

#include <datapod/datapod.hpp>

namespace timenav::vda {

    struct OrderNode {
        dp::String node_id;
        dp::String sequence_id;
        bool released = false;
    };

    struct OrderEdge {
        dp::String edge_id;
        dp::String start_node_id;
        dp::String end_node_id;
        bool released = false;
    };

    struct Order {
        dp::String order_id;
        dp::u32 order_update_id = 0;
        dp::Vector<OrderNode> nodes;
        dp::Vector<OrderEdge> edges;
    };

} // namespace timenav::vda
