#pragma once

#include <string>

#include "timenav/claim_manager.hpp"
#include "timenav/robot_state.hpp"
#include "timenav/route.hpp"
#include "timenav/vda/connection.hpp"
#include "timenav/vda/factsheet.hpp"
#include "timenav/vda/instant_actions.hpp"
#include "timenav/vda/order.hpp"
#include "timenav/vda/responses.hpp"
#include "timenav/vda/state.hpp"

namespace timenav::vda {

    inline dp::String uuid_string(const zoneout::UUID &id) { return dp::String{id.toString()}; }

    inline Order map_route_plan(const RoutePlan &route_plan) {
        Order order{};
        order.order_id = uuid_string(route_plan.goal_node_id);

        dp::u32 sequence = 0;
        for (const auto &node_id : route_plan.traversed_node_ids) {
            OrderNode node{};
            node.node_id = uuid_string(node_id);
            node.sequence_id = dp::String{std::to_string(sequence++)};
            node.released = false;
            order.nodes.push_back(node);
        }

        for (dp::u64 i = 0; i < route_plan.traversed_edge_ids.size(); ++i) {
            const auto start_index = i;
            const auto end_index = i + 1;
            OrderEdge edge{};
            edge.edge_id = uuid_string(route_plan.traversed_edge_ids[i]);
            edge.start_node_id = uuid_string(route_plan.traversed_node_ids[start_index]);
            edge.end_node_id = uuid_string(route_plan.traversed_node_ids[end_index]);
            edge.released = false;
            order.edges.push_back(edge);
        }

        return order;
    }
    inline Order map_route_plan(const WorkspaceIndex &index, const RoutePlan &route_plan) {
        Order order = map_route_plan(route_plan);

        for (dp::u64 i = 0; i < route_plan.traversed_node_ids.size() && i < order.nodes.size(); ++i) {
            const auto node_zones = index.zones_of_node(route_plan.traversed_node_ids[i]);
            if (!node_zones.empty() && node_zones.front() != nullptr) {
                order.nodes[i].zone_id = uuid_string(node_zones.front()->id());
            }
        }

        for (dp::u64 i = 0; i < route_plan.traversed_edge_ids.size() && i < order.edges.size(); ++i) {
            const auto edge_zones = index.zones_of_edge(route_plan.traversed_edge_ids[i]);
            if (!edge_zones.empty() && edge_zones.front() != nullptr) {
                order.edges[i].zone_id = uuid_string(edge_zones.front()->id());
            }
            if (const auto *edge = index.edge(route_plan.traversed_edge_ids[i]); edge != nullptr) {
                const auto semantics = parse_edge_traffic_semantics(edge->properties);
                if (semantics.speed_limit.has_value()) {
                    order.edges[i].max_speed = semantics.speed_limit.value();
                }
            }
        }

        return order;
    }

    class Adapter {
      public:
        Adapter() = default;

        [[nodiscard]] Order order_from_route(const RoutePlan &route_plan) const;
        [[nodiscard]] Order order_from_route(const WorkspaceIndex &index, const RoutePlan &route_plan) const;
        [[nodiscard]] State state_from_robot(const RobotState &robot_state) const;
        [[nodiscard]] State state_from_robot(const RobotState &robot_state, const ClaimManager &claim_manager) const;
        [[nodiscard]] Connection connection_from_factsheet(const Factsheet &factsheet) const;
        [[nodiscard]] Response response_for_action(const InstantAction &action, ActionStatus status,
                                                   dp::Optional<dp::String> description = dp::nullopt,
                                                   dp::Optional<dp::String> result_code = dp::nullopt) const;
    };

    inline Order Adapter::order_from_route(const RoutePlan &route_plan) const { return map_route_plan(route_plan); }
    inline Order Adapter::order_from_route(const WorkspaceIndex &index, const RoutePlan &route_plan) const {
        return map_route_plan(index, route_plan);
    }

    inline State map_robot_state(const RobotState &robot_state) {
        State state{};
        state.agv_id = dp::String{std::to_string(robot_state.robot_id.raw())};
        state.operating_mode = robot_state.route_plan.has_value() ? OperatingMode::Automatic : OperatingMode::Manual;
        state.connection_state = ConnectionState::Online;
        if (robot_state.current_node_id.has_value()) {
            state.last_node_id = uuid_string(*robot_state.current_node_id);
        }
        if (robot_state.current_edge_id.has_value()) {
            state.last_edge_id = uuid_string(*robot_state.current_edge_id);
        }
        if (robot_state.route_plan.has_value()) {
            state.order_id = uuid_string(robot_state.route_plan->goal_node_id);
        }
        state.driving_state = robot_state.progress_state == RobotProgressState::FollowingRoute ? dp::String{"DRIVING"}
                                                                                               : dp::String{"STOPPED"};
        if (!robot_state.pending_claim_ids.empty()) {
            state.errors.push_back(dp::String{"pending_claims"});
        }
        return state;
    }
    inline State map_robot_state(const RobotState &robot_state, const ClaimManager &claim_manager) {
        State state = map_robot_state(robot_state);
        if (!robot_state.active_lease_ids.empty()) {
            state.action_states.push_back(dp::String{"holding_leases"});
        }
        for (const auto lease_id : robot_state.active_lease_ids) {
            if (claim_manager.find_lease(lease_id) == nullptr) {
                state.errors.push_back(dp::String{"missing_lease"});
            }
        }
        if (robot_state.hold_reason.has_value()) {
            state.action_states.push_back(robot_state.hold_reason.value());
        }
        return state;
    }

    inline State Adapter::state_from_robot(const RobotState &robot_state) const { return map_robot_state(robot_state); }
    inline State Adapter::state_from_robot(const RobotState &robot_state, const ClaimManager &claim_manager) const {
        return map_robot_state(robot_state, claim_manager);
    }

    inline Connection Adapter::connection_from_factsheet(const Factsheet &factsheet) const {
        Connection connection{};
        connection.manufacturer = factsheet.manufacturer;
        connection.serial_number = factsheet.serial_number;
        connection.version = factsheet.protocol_version;
        connection.status = ConnectionStatus::Online;
        return connection;
    }

    inline Response Adapter::response_for_action(const InstantAction &action, ActionStatus status,
                                                 dp::Optional<dp::String> description,
                                                 dp::Optional<dp::String> result_code) const {
        Response response{};
        response.action_id = action.action_id;
        response.status = status;
        response.description = description;
        response.result_code = result_code;
        return response;
    }

} // namespace timenav::vda
