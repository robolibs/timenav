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

    inline dp::String claim_target_kind_string(ClaimTargetKind kind) {
        switch (kind) {
        case ClaimTargetKind::Zone:
            return "zone";
        case ClaimTargetKind::Node:
            return "node";
        case ClaimTargetKind::Edge:
            return "edge";
        }

        return "target";
    }

    inline ResourceReservation reservation_from_target(const ClaimTarget &target) {
        ResourceReservation reservation{};
        reservation.target_id = uuid_string(target.resource_id);
        reservation.target_kind = claim_target_kind_string(target.kind);
        return reservation;
    }

    inline dp::Result<Order> try_map_route_plan(const RoutePlan &route_plan) {
        const auto route_shape = validate_route_plan_shape(route_plan);
        if (route_shape.is_err()) {
            return dp::Result<Order>::err(route_shape.error());
        }

        Order order{};
        order.header_id = uuid_string(route_plan.start_node_id);
        order.order_id = uuid_string(route_plan.goal_node_id);
        order.order_update_id = static_cast<dp::u32>(route_plan.traversed_edge_ids.size());

        dp::u32 sequence = 0;
        for (dp::u64 i = 0; i < route_plan.traversed_node_ids.size(); ++i) {
            OrderNode node{};
            node.node_id = uuid_string(route_plan.traversed_node_ids[i]);
            node.sequence_id = dp::String{std::to_string(sequence++)};
            node.node_position_hint = node.sequence_id;
            node.released = false;
            if (i < route_plan.traversed_node_zone_ids.size()) {
                for (const auto &zone_id : route_plan.traversed_node_zone_ids[i]) {
                    node.reservations.push_back(
                        ResourceReservation{uuid_string(zone_id), "zone", false, dp::nullopt, dp::nullopt});
                }
            }
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
            if (i < route_plan.traversed_edge_zone_ids.size()) {
                for (const auto &zone_id : route_plan.traversed_edge_zone_ids[i]) {
                    edge.reservations.push_back(
                        ResourceReservation{uuid_string(zone_id), "zone", false, dp::nullopt, dp::nullopt});
                }
            }
            order.edges.push_back(edge);
        }

        return dp::Result<Order>::ok(order);
    }
    inline Order map_route_plan(const RoutePlan &route_plan) {
        const auto order = try_map_route_plan(route_plan);
        return order.is_ok() ? order.value() : Order{};
    }
    inline dp::Result<Order> try_map_route_plan(const WorkspaceIndex &index, const RoutePlan &route_plan) {
        const auto base_order = try_map_route_plan(route_plan);
        if (base_order.is_err()) {
            return dp::Result<Order>::err(base_order.error());
        }

        Order order = base_order.value();

        for (dp::u64 i = 0; i < route_plan.traversed_node_ids.size() && i < order.nodes.size(); ++i) {
            const auto node_zones = index.zones_of_node(route_plan.traversed_node_ids[i]);
            if (!node_zones.empty() && node_zones.front() != nullptr) {
                order.nodes[i].zone_id = uuid_string(node_zones.front()->id());
            }
            if (const auto *node = index.node(route_plan.traversed_node_ids[i]); node != nullptr) {
                order.nodes[i].node_position_hint =
                    dp::String{std::to_string(node->position.x)} + "," + dp::String{std::to_string(node->position.y)};
            }
        }

        for (dp::u64 i = 0; i < route_plan.traversed_edge_ids.size() && i < order.edges.size(); ++i) {
            const auto edge_zones = index.zones_of_edge(route_plan.traversed_edge_ids[i]);
            if (!edge_zones.empty() && edge_zones.front() != nullptr) {
                order.edges[i].zone_id = uuid_string(edge_zones.front()->id());
            }
            if (const auto *edge = index.edge(route_plan.traversed_edge_ids[i]); edge != nullptr) {
                const auto *workspace = index.workspace();
                bool directed = false;
                if (workspace != nullptr) {
                    const auto edge_vertex_id = workspace->find_edge(route_plan.traversed_edge_ids[i]);
                    if (edge_vertex_id.has_value()) {
                        directed =
                            workspace->graph().get_edge_type(*edge_vertex_id) == graphix::vertex::EdgeType::Directed;
                    }
                }
                dp::Vector<ZonePolicy> zone_policies;
                for (const auto *zone : edge_zones) {
                    if (zone != nullptr) {
                        zone_policies.push_back(parse_zone_policy(zone->properties()));
                    }
                }
                const auto semantics = derive_effective_edge_semantics(edge->properties, directed, zone_policies);
                if (semantics.speed_limit.has_value()) {
                    order.edges[i].max_speed = semantics.speed_limit.value();
                }
                order.edges[i].bidirectional = !semantics.directed || semantics.reversible.value_or(false);
                if (semantics.requires_claim.value_or(false)) {
                    order.edges[i].reservations.push_back(ResourceReservation{
                        uuid_string(edge->id), "edge", true, semantics.access_group, semantics.schedule_window});
                }
                if (semantics.access_group.has_value() && !order.edges[i].reservations.empty()) {
                    order.edges[i].reservations.back().access_group = semantics.access_group;
                }
                if (semantics.schedule_window.has_value() && !order.edges[i].reservations.empty()) {
                    order.edges[i].reservations.back().schedule_window = semantics.schedule_window;
                }
            }
        }

        for (dp::u64 i = 0; i < route_plan.traversed_node_ids.size() && i < order.nodes.size(); ++i) {
            const auto node_zones = index.zones_of_node(route_plan.traversed_node_ids[i]);
            for (const auto *zone : node_zones) {
                if (zone == nullptr) {
                    continue;
                }
                const auto policy = parse_zone_policy(zone->properties());
                if (policy.requires_claim || policy.access_group.has_value() || policy.schedule_window.has_value()) {
                    order.nodes[i].reservations.push_back(
                        ResourceReservation{uuid_string(zone->id()), "zone", policy.requires_claim, policy.access_group,
                                            policy.schedule_window});
                }
            }
        }

        return dp::Result<Order>::ok(order);
    }
    inline Order map_route_plan(const WorkspaceIndex &index, const RoutePlan &route_plan) {
        const auto order = try_map_route_plan(index, route_plan);
        return order.is_ok() ? order.value() : Order{};
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
            state.order_update_id = static_cast<dp::u32>(robot_state.route_plan->traversed_edge_ids.size());
        }
        state.driving_state = robot_state.progress_state == RobotProgressState::FollowingRoute ? dp::String{"DRIVING"}
                                                                                               : dp::String{"STOPPED"};
        state.paused = robot_state.progress_state == RobotProgressState::Waiting ||
                       robot_state.progress_state == RobotProgressState::Blocked;
        if (!robot_state.pending_claim_ids.empty()) {
            state.errors.push_back(dp::String{"pending_claims"});
            state.information.push_back(dp::String{"claims awaiting arbitration"});
        }
        return state;
    }
    inline State map_robot_state(const RobotState &robot_state, const ClaimManager &claim_manager) {
        State state = map_robot_state(robot_state);
        if (!robot_state.active_lease_ids.empty()) {
            state.action_states.push_back(dp::String{"holding_leases"});
        }
        for (const auto lease_id : robot_state.active_lease_ids) {
            const auto *lease = claim_manager.find_lease(lease_id);
            if (lease == nullptr) {
                state.errors.push_back(dp::String{"missing_lease"});
                continue;
            }
            for (const auto &target : lease->targets) {
                state.reservation_states.push_back(
                    ReservationState{uuid_string(target.resource_id), claim_target_kind_string(target.kind), "ACTIVE"});
            }
        }
        for (const auto claim_id : robot_state.pending_claim_ids) {
            if (const auto *request = claim_manager.find_request(claim_id); request != nullptr) {
                for (const auto &target : request->targets) {
                    state.reservation_states.push_back(ReservationState{
                        uuid_string(target.resource_id), claim_target_kind_string(target.kind), "PENDING"});
                }
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
