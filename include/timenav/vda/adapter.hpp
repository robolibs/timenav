#pragma once

#include <string>

#include "timenav/robot_state.hpp"
#include "timenav/route.hpp"
#include "timenav/vda/order.hpp"
#include "timenav/vda/state.hpp"

namespace timenav::vda {

    inline dp::String uuid_string(const zoneout::UUID &id) { return dp::String{id.toString()}; }

    class Adapter {
      public:
        Adapter() = default;

        [[nodiscard]] Order order_from_route(const RoutePlan &route_plan) const;
        [[nodiscard]] State state_from_robot(const RobotState &robot_state) const;
    };

    inline Order Adapter::order_from_route(const RoutePlan &route_plan) const {
        Order order{};
        order.order_id = uuid_string(route_plan.goal_node_id);

        dp::u32 sequence = 0;
        for (const auto &node_id : route_plan.traversed_node_ids) {
            order.nodes.push_back(OrderNode{uuid_string(node_id), dp::String{std::to_string(sequence++)}, false});
        }

        for (dp::u64 i = 0; i < route_plan.traversed_edge_ids.size(); ++i) {
            const auto start_index = i;
            const auto end_index = i + 1;
            order.edges.push_back(OrderEdge{uuid_string(route_plan.traversed_edge_ids[i]),
                                            uuid_string(route_plan.traversed_node_ids[start_index]),
                                            uuid_string(route_plan.traversed_node_ids[end_index]), false});
        }

        return order;
    }

} // namespace timenav::vda
