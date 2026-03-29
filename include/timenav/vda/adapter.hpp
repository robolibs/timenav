#pragma once

#include <string>

#include "timenav/robot_state.hpp"
#include "timenav/route.hpp"
#include "timenav/vda/order.hpp"
#include "timenav/vda/state.hpp"

namespace timenav::vda {

    inline dp::String uuid_string(const zoneout::UUID &id) { return dp::String{id.toString()}; }

    inline Order map_route_plan(const RoutePlan &route_plan) {
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

    class Adapter {
      public:
        Adapter() = default;

        [[nodiscard]] Order order_from_route(const RoutePlan &route_plan) const;
        [[nodiscard]] State state_from_robot(const RobotState &robot_state) const;
    };

    inline Order Adapter::order_from_route(const RoutePlan &route_plan) const { return map_route_plan(route_plan); }

    inline State map_robot_state(const RobotState &robot_state) {
        State state{};
        state.agv_id = dp::String{std::to_string(robot_state.robot_id.raw())};
        state.operating_mode = robot_state.route_plan.has_value() ? dp::String{"AUTOMATIC"} : dp::String{"IDLE"};
        if (robot_state.current_node_id.has_value()) {
            state.last_node_id = uuid_string(*robot_state.current_node_id);
        }
        if (robot_state.current_edge_id.has_value()) {
            state.last_edge_id = uuid_string(*robot_state.current_edge_id);
        }
        if (!robot_state.pending_claim_ids.empty()) {
            state.errors.push_back(dp::String{"pending_claims"});
        }
        return state;
    }

    inline State Adapter::state_from_robot(const RobotState &robot_state) const { return map_robot_state(robot_state); }

} // namespace timenav::vda
