#pragma once

#include <algorithm>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "timenav/workspace_index.hpp"
#include "timenav/zone_policy.hpp"
#include <datapod/datapod.hpp>
#include <zoneout/zoneout.hpp>

namespace timenav {

    struct RouteStep {
        zoneout::UUID node_id;
        dp::Optional<zoneout::UUID> incoming_edge_id;
        dp::f64 step_cost = 0.0;
        dp::f64 cumulative_cost = 0.0;
    };

    struct RoutePlan {
        zoneout::UUID start_node_id;
        zoneout::UUID goal_node_id;
        dp::Vector<RouteStep> steps;
        dp::Vector<zoneout::UUID> traversed_node_ids;
        dp::Vector<zoneout::UUID> traversed_edge_ids;
        dp::Vector<zoneout::UUID> traversed_zone_ids;
        dp::Vector<dp::Vector<zoneout::UUID>> traversed_node_zone_ids;
        dp::Vector<dp::Vector<zoneout::UUID>> traversed_edge_zone_ids;
        dp::f64 total_cost = 0.0;
    };

    struct RouteSearchState {
        bool found = false;
        dp::f64 distance = std::numeric_limits<dp::f64>::infinity();
        std::unordered_map<zoneout::UUID, dp::f64, zoneout::UUIDHash> distances;
        std::unordered_map<zoneout::UUID, zoneout::UUID, zoneout::UUIDHash> predecessors;
    };

    enum class RouteFailureKind { MissingStartNode, MissingGoalNode, PolicyBlocked, Unreachable };

    struct RouteFailure {
        RouteFailureKind kind = RouteFailureKind::Unreachable;
        dp::String message;
        dp::Vector<zoneout::UUID> blocked_edge_ids;
        dp::Vector<zoneout::UUID> blocked_zone_ids;
        dp::Vector<zoneout::UUID> reachable_node_ids;
    };

    struct RoutePlanningResult {
        RouteSearchState search;
        dp::Optional<RoutePlan> plan;
        dp::Optional<RouteFailure> failure;
    };

    struct TraversalNeighbor {
        zoneout::UUID node_id;
        zoneout::UUID edge_id;
        dp::f64 weight = 0.0;
    };

    enum class RouteCostModel { GraphWeight, Penalized };

    inline dp::Result<dp::f64> accumulate_route_cost(const WorkspaceIndex &index,
                                                     const dp::Vector<zoneout::UUID> &route_nodes);
    inline dp::Result<dp::f64> accumulate_route_cost(const WorkspaceIndex &index,
                                                     const dp::Vector<zoneout::UUID> &route_nodes,
                                                     RouteCostModel cost_model);
    inline dp::Result<dp::Vector<zoneout::UUID>> extract_traversed_node_ids(const RouteSearchState &search,
                                                                            const zoneout::UUID &start_node_id,
                                                                            const zoneout::UUID &goal_node_id);
    inline dp::Result<dp::Vector<zoneout::UUID>> extract_traversed_edge_ids(const WorkspaceIndex &index,
                                                                            const RouteSearchState &search,
                                                                            const zoneout::UUID &start_node_id,
                                                                            const zoneout::UUID &goal_node_id);
    inline dp::Result<dp::u64> validate_route_plan_shape(const RoutePlan &route_plan);
    inline dp::Result<dp::Vector<zoneout::UUID>> extract_traversed_zone_ids(const WorkspaceIndex &index,
                                                                            const RouteSearchState &search,
                                                                            const zoneout::UUID &start_node_id,
                                                                            const zoneout::UUID &goal_node_id);

    inline bool allows_traversal_from_node(const zoneout::EdgeData &edge_data, bool from_source) {
        const auto semantics = parse_edge_traffic_semantics(edge_data.properties);
        if (!semantics.preferred_direction.has_value()) {
            return true;
        }

        const auto direction = std::string(semantics.preferred_direction.value().c_str());
        if (direction == "forward" || direction == "source_to_target") {
            return from_source || semantics.reversible.value_or(false);
        }
        if (direction == "reverse" || direction == "target_to_source") {
            return !from_source || semantics.reversible.value_or(false);
        }
        if (direction == "bidirectional") {
            return true;
        }

        return true;
    }

    inline dp::Vector<zoneout::UUID> blocked_zones_for_edge(const WorkspaceIndex &index, const zoneout::UUID &edge_id) {
        dp::Vector<zoneout::UUID> blocked_zone_ids;
        for (const auto *zone : index.zones_of_edge(edge_id)) {
            if (zone == nullptr) {
                continue;
            }

            const auto zone_policy = parse_zone_policy(zone->properties());
            if (zone_policy.blocked.value_or(false) || zone_policy.kind == ZonePolicyKind::Restricted ||
                zone_policy.blocks_entry_without_grant || zone_policy.blocks_traversal_without_grant) {
                blocked_zone_ids.push_back(zone->id());
            }
        }
        return blocked_zone_ids;
    }

    inline bool is_edge_hard_blocked(const WorkspaceIndex &index, const zoneout::UUID &edge_id) {
        const auto *edge_data = index.edge(edge_id);
        if (edge_data == nullptr) {
            return true;
        }

        const auto edge_semantics = parse_edge_traffic_semantics(edge_data->properties);
        if (edge_semantics.blocked.value_or(false) || edge_semantics.no_stop.value_or(false)) {
            return true;
        }

        if (!blocked_zones_for_edge(index, edge_id).empty()) {
            return true;
        }

        return false;
    }

    inline dp::f64 edge_traversal_penalty(const WorkspaceIndex &index, const zoneout::UUID &edge_id) {
        const auto *edge_data = index.edge(edge_id);
        if (edge_data == nullptr) {
            return std::numeric_limits<dp::f64>::infinity();
        }

        dp::Vector<ZonePolicy> zone_policies;
        for (const auto *zone : index.zones_of_edge(edge_id)) {
            if (zone != nullptr) {
                zone_policies.push_back(parse_zone_policy(zone->properties()));
            }
        }

        const auto semantics = derive_effective_edge_semantics(edge_data->properties, false, zone_policies);

        dp::f64 penalty = 0.0;
        if (semantics.cost_bias.has_value()) {
            penalty += std::max(0.0, semantics.cost_bias.value());
        }
        if (semantics.speed_limit.has_value() && semantics.speed_limit.value() > 0.0) {
            penalty += 1.0 / semantics.speed_limit.value();
        }
        if (semantics.priority.has_value()) {
            penalty += std::max(0.0, 10.0 - semantics.priority.value()) * 0.1;
        }
        if (semantics.capacity.has_value() && semantics.capacity.value() > 0) {
            penalty += 1.0 / static_cast<dp::f64>(semantics.capacity.value());
        }
        if (semantics.clearance_width.has_value() && semantics.clearance_width.value() > 0.0) {
            penalty += 1.0 / semantics.clearance_width.value();
        }
        if (semantics.clearance_height.has_value() && semantics.clearance_height.value() > 0.0) {
            penalty += 1.0 / semantics.clearance_height.value();
        }

        for (const auto &zone_policy : zone_policies) {
            if (zone_policy.requires_claim || zone_policy.blocks_entry_without_grant ||
                zone_policy.blocks_traversal_without_grant) {
                penalty += 100.0;
            }
        }

        return penalty;
    }

    class GraphTraversalAdapter {
      public:
        explicit GraphTraversalAdapter(const WorkspaceIndex &index) : index_(index) {}

        [[nodiscard]] dp::Vector<TraversalNeighbor> neighbors(const zoneout::UUID &node_id) const {
            dp::Vector<TraversalNeighbor> result;

            const auto *workspace = index_.workspace();
            if (workspace == nullptr) {
                return result;
            }

            const auto vertex_id = workspace->find_node(node_id);
            if (!vertex_id.has_value()) {
                return result;
            }

            for (const auto &edge : workspace->graph().edges()) {
                const auto source = workspace->graph().source(edge.id);
                const auto target = workspace->graph().target(edge.id);
                const auto touches_vertex = source == *vertex_id || target == *vertex_id;
                if (!touches_vertex) {
                    continue;
                }

                const bool from_source = source == *vertex_id;
                if (!allows_traversal_from_node(workspace->graph().edge_property(edge.id), from_source)) {
                    continue;
                }

                const auto other_vertex = from_source ? target : source;
                result.push_back(TraversalNeighbor{workspace->graph()[other_vertex].id,
                                                   workspace->graph().edge_property(edge.id).id,
                                                   workspace->graph().get_weight(edge.id)});
            }

            return result;
        }

      private:
        const WorkspaceIndex &index_;
    };

    inline RouteSearchState shortest_path_search(const WorkspaceIndex &index, const zoneout::UUID &start_node_id,
                                                 const zoneout::UUID &goal_node_id) {
        struct QueueEntry {
            zoneout::UUID node_id;
            dp::f64 distance;

            bool operator>(const QueueEntry &other) const { return distance > other.distance; }
        };

        RouteSearchState state{};
        if (index.node(start_node_id) == nullptr || index.node(goal_node_id) == nullptr) {
            return state;
        }
        if (start_node_id == goal_node_id) {
            state.found = true;
            state.distance = 0.0;
            state.distances[start_node_id] = 0.0;
            return state;
        }

        GraphTraversalAdapter adapter{index};
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> frontier;
        std::unordered_set<zoneout::UUID, zoneout::UUIDHash> visited;

        state.distances[start_node_id] = 0.0;
        frontier.push(QueueEntry{start_node_id, 0.0});

        while (!frontier.empty()) {
            const auto current = frontier.top();
            frontier.pop();

            if (!visited.insert(current.node_id).second) {
                continue;
            }

            if (current.node_id == goal_node_id) {
                state.found = true;
                state.distance = current.distance;
                return state;
            }

            for (const auto &neighbor : adapter.neighbors(current.node_id)) {
                if (visited.count(neighbor.node_id) > 0) {
                    continue;
                }

                const auto new_distance = current.distance + neighbor.weight;
                const auto best_it = state.distances.find(neighbor.node_id);
                if (best_it == state.distances.end() || new_distance < best_it->second) {
                    state.distances[neighbor.node_id] = new_distance;
                    state.predecessors[neighbor.node_id] = current.node_id;
                    frontier.push(QueueEntry{neighbor.node_id, new_distance});
                }
            }
        }

        return state;
    }

    inline RouteSearchState shortest_path_search_with_blocking(const WorkspaceIndex &index,
                                                               const zoneout::UUID &start_node_id,
                                                               const zoneout::UUID &goal_node_id) {
        struct QueueEntry {
            zoneout::UUID node_id;
            dp::f64 distance;

            bool operator>(const QueueEntry &other) const { return distance > other.distance; }
        };

        RouteSearchState state{};
        if (index.node(start_node_id) == nullptr || index.node(goal_node_id) == nullptr) {
            return state;
        }
        if (start_node_id == goal_node_id) {
            state.found = true;
            state.distance = 0.0;
            state.distances[start_node_id] = 0.0;
            return state;
        }

        GraphTraversalAdapter adapter{index};
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> frontier;
        std::unordered_set<zoneout::UUID, zoneout::UUIDHash> visited;

        state.distances[start_node_id] = 0.0;
        frontier.push(QueueEntry{start_node_id, 0.0});

        while (!frontier.empty()) {
            const auto current = frontier.top();
            frontier.pop();

            if (!visited.insert(current.node_id).second) {
                continue;
            }

            if (current.node_id == goal_node_id) {
                state.found = true;
                state.distance = current.distance;
                return state;
            }

            for (const auto &neighbor : adapter.neighbors(current.node_id)) {
                if (visited.count(neighbor.node_id) > 0 || is_edge_hard_blocked(index, neighbor.edge_id)) {
                    continue;
                }

                const auto new_distance = current.distance + neighbor.weight;
                const auto best_it = state.distances.find(neighbor.node_id);
                if (best_it == state.distances.end() || new_distance < best_it->second) {
                    state.distances[neighbor.node_id] = new_distance;
                    state.predecessors[neighbor.node_id] = current.node_id;
                    frontier.push(QueueEntry{neighbor.node_id, new_distance});
                }
            }
        }

        return state;
    }

    inline RouteSearchState shortest_path_search_with_penalties(const WorkspaceIndex &index,
                                                                const zoneout::UUID &start_node_id,
                                                                const zoneout::UUID &goal_node_id) {
        struct QueueEntry {
            zoneout::UUID node_id;
            dp::f64 distance;

            bool operator>(const QueueEntry &other) const { return distance > other.distance; }
        };

        RouteSearchState state{};
        if (index.node(start_node_id) == nullptr || index.node(goal_node_id) == nullptr) {
            return state;
        }
        if (start_node_id == goal_node_id) {
            state.found = true;
            state.distance = 0.0;
            state.distances[start_node_id] = 0.0;
            return state;
        }

        GraphTraversalAdapter adapter{index};
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> frontier;
        std::unordered_set<zoneout::UUID, zoneout::UUIDHash> visited;

        state.distances[start_node_id] = 0.0;
        frontier.push(QueueEntry{start_node_id, 0.0});

        while (!frontier.empty()) {
            const auto current = frontier.top();
            frontier.pop();

            if (!visited.insert(current.node_id).second) {
                continue;
            }

            if (current.node_id == goal_node_id) {
                state.found = true;
                state.distance = current.distance;
                return state;
            }

            for (const auto &neighbor : adapter.neighbors(current.node_id)) {
                if (visited.count(neighbor.node_id) > 0 || is_edge_hard_blocked(index, neighbor.edge_id)) {
                    continue;
                }

                const auto new_distance =
                    current.distance + neighbor.weight + edge_traversal_penalty(index, neighbor.edge_id);
                const auto best_it = state.distances.find(neighbor.node_id);
                if (best_it == state.distances.end() || new_distance < best_it->second) {
                    state.distances[neighbor.node_id] = new_distance;
                    state.predecessors[neighbor.node_id] = current.node_id;
                    frontier.push(QueueEntry{neighbor.node_id, new_distance});
                }
            }
        }

        return state;
    }

    inline dp::Vector<zoneout::UUID> reconstruct_route_nodes(const RouteSearchState &search,
                                                             const zoneout::UUID &start_node_id,
                                                             const zoneout::UUID &goal_node_id) {
        dp::Vector<zoneout::UUID> nodes;
        if (!search.found) {
            return nodes;
        }

        auto current = goal_node_id;
        nodes.push_back(current);

        while (current != start_node_id) {
            const auto predecessor_it = search.predecessors.find(current);
            if (predecessor_it == search.predecessors.end()) {
                return dp::Vector<zoneout::UUID>{};
            }
            current = predecessor_it->second;
            nodes.push_back(current);
        }

        std::reverse(nodes.begin(), nodes.end());
        return nodes;
    }

    inline dp::Result<dp::Vector<zoneout::UUID>> extract_traversed_node_ids(const RouteSearchState &search,
                                                                            const zoneout::UUID &start_node_id,
                                                                            const zoneout::UUID &goal_node_id) {
        if (!search.found) {
            return dp::Result<dp::Vector<zoneout::UUID>>::ok({});
        }

        const auto route_nodes = reconstruct_route_nodes(search, start_node_id, goal_node_id);
        if (route_nodes.empty() && !(start_node_id == goal_node_id)) {
            return dp::Result<dp::Vector<zoneout::UUID>>::err(
                dp::Error::not_found("route reconstruction could not recover a predecessor chain"));
        }

        return dp::Result<dp::Vector<zoneout::UUID>>::ok(route_nodes);
    }

    inline dp::Result<dp::Vector<RouteStep>>
    reconstruct_route_steps(const WorkspaceIndex &index, const RouteSearchState &search,
                            const zoneout::UUID &start_node_id, const zoneout::UUID &goal_node_id,
                            RouteCostModel cost_model = RouteCostModel::GraphWeight) {
        const auto route_nodes = reconstruct_route_nodes(search, start_node_id, goal_node_id);
        dp::Vector<RouteStep> steps;
        if (route_nodes.empty() && !(search.found && start_node_id == goal_node_id)) {
            return dp::Result<dp::Vector<RouteStep>>::ok(steps);
        }

        dp::f64 cumulative_cost = 0.0;
        for (dp::u64 i = 0; i < route_nodes.size(); ++i) {
            RouteStep step{};
            step.node_id = route_nodes[i];
            if (i > 0) {
                const auto *edge_data = index.edge_between(route_nodes[i - 1], route_nodes[i]);
                if (edge_data == nullptr) {
                    return dp::Result<dp::Vector<RouteStep>>::err(
                        dp::Error::not_found("route reconstruction references adjacent nodes without a graph edge"));
                }
                step.incoming_edge_id = edge_data->id;
                const auto partial_route = dp::Vector<zoneout::UUID>(route_nodes.begin() + static_cast<dp::i64>(i - 1),
                                                                     route_nodes.begin() + static_cast<dp::i64>(i + 1));
                const auto step_cost = accumulate_route_cost(index, partial_route, cost_model);
                if (step_cost.is_err()) {
                    return dp::Result<dp::Vector<RouteStep>>::err(step_cost.error());
                }
                step.step_cost = step_cost.value();
                cumulative_cost += step.step_cost;
                step.cumulative_cost = cumulative_cost;
            }
            steps.push_back(step);
        }

        return dp::Result<dp::Vector<RouteStep>>::ok(steps);
    }

    inline dp::Result<dp::f64> accumulate_route_cost(const WorkspaceIndex &index,
                                                     const dp::Vector<zoneout::UUID> &route_nodes) {
        return accumulate_route_cost(index, route_nodes, RouteCostModel::GraphWeight);
    }

    inline dp::Result<dp::f64> accumulate_route_cost(const WorkspaceIndex &index,
                                                     const dp::Vector<zoneout::UUID> &route_nodes,
                                                     RouteCostModel cost_model) {
        if (route_nodes.empty()) {
            return dp::Result<dp::f64>::ok(0.0);
        }

        const auto *workspace = index.workspace();
        if (workspace == nullptr) {
            return dp::Result<dp::f64>::err(dp::Error::invalid_argument("route cost accumulation requires workspace"));
        }

        dp::f64 total_cost = 0.0;
        for (dp::u64 i = 1; i < route_nodes.size(); ++i) {
            const auto from_vertex = workspace->find_node(route_nodes[i - 1]);
            const auto to_vertex = workspace->find_node(route_nodes[i]);
            if (!from_vertex.has_value() || !to_vertex.has_value()) {
                return dp::Result<dp::f64>::err(
                    dp::Error::not_found("route references node that is not present in the workspace"));
            }

            auto edge_id = workspace->graph().get_edge(*from_vertex, *to_vertex);
            if (!edge_id.has_value()) {
                edge_id = workspace->graph().get_edge(*to_vertex, *from_vertex);
            }
            if (!edge_id.has_value()) {
                return dp::Result<dp::f64>::err(
                    dp::Error::not_found("route references adjacent nodes without a graph edge"));
            }

            total_cost += workspace->graph().get_weight(*edge_id);
            if (cost_model == RouteCostModel::Penalized) {
                total_cost += edge_traversal_penalty(index, workspace->graph().edge_property(*edge_id).id);
            }
        }

        return dp::Result<dp::f64>::ok(total_cost);
    }

    inline dp::Result<dp::Vector<zoneout::UUID>>
    extract_traversed_edge_ids(const WorkspaceIndex &index, const dp::Vector<zoneout::UUID> &route_nodes) {
        dp::Vector<zoneout::UUID> traversed_edge_ids;

        if (route_nodes.size() < 2) {
            return dp::Result<dp::Vector<zoneout::UUID>>::ok(traversed_edge_ids);
        }

        for (dp::u64 i = 1; i < route_nodes.size(); ++i) {
            const auto *edge_data = index.edge_between(route_nodes[i - 1], route_nodes[i]);
            if (edge_data == nullptr) {
                return dp::Result<dp::Vector<zoneout::UUID>>::err(
                    dp::Error::not_found("route references adjacent nodes without a graph edge"));
            }
            traversed_edge_ids.push_back(edge_data->id);
        }

        return dp::Result<dp::Vector<zoneout::UUID>>::ok(traversed_edge_ids);
    }

    inline dp::Result<dp::Vector<zoneout::UUID>> extract_traversed_edge_ids(const WorkspaceIndex &index,
                                                                            const RouteSearchState &search,
                                                                            const zoneout::UUID &start_node_id,
                                                                            const zoneout::UUID &goal_node_id) {
        const auto route_nodes = extract_traversed_node_ids(search, start_node_id, goal_node_id);
        if (route_nodes.is_err()) {
            return dp::Result<dp::Vector<zoneout::UUID>>::err(route_nodes.error());
        }

        return extract_traversed_edge_ids(index, route_nodes.value());
    }

    inline dp::Result<dp::Vector<zoneout::UUID>>
    extract_traversed_zone_ids(const WorkspaceIndex &index, const dp::Vector<zoneout::UUID> &route_nodes) {
        dp::Vector<zoneout::UUID> traversed_zone_ids;
        std::unordered_set<zoneout::UUID, zoneout::UUIDHash> seen_zone_ids;

        for (const auto &node_id : route_nodes) {
            for (const auto *zone : index.zones_of_node(node_id)) {
                if (zone != nullptr && seen_zone_ids.insert(zone->id()).second) {
                    traversed_zone_ids.push_back(zone->id());
                }
            }
        }

        const auto traversed_edges = extract_traversed_edge_ids(index, route_nodes);
        if (traversed_edges.is_err()) {
            return dp::Result<dp::Vector<zoneout::UUID>>::err(traversed_edges.error());
        }

        for (const auto &edge_id : traversed_edges.value()) {
            for (const auto *zone : index.zones_of_edge(edge_id)) {
                if (zone != nullptr && seen_zone_ids.insert(zone->id()).second) {
                    traversed_zone_ids.push_back(zone->id());
                }
            }
        }

        return dp::Result<dp::Vector<zoneout::UUID>>::ok(traversed_zone_ids);
    }

    inline dp::Result<dp::Vector<zoneout::UUID>> extract_traversed_zone_ids(const WorkspaceIndex &index,
                                                                            const RouteSearchState &search,
                                                                            const zoneout::UUID &start_node_id,
                                                                            const zoneout::UUID &goal_node_id) {
        const auto route_nodes = extract_traversed_node_ids(search, start_node_id, goal_node_id);
        if (route_nodes.is_err()) {
            return dp::Result<dp::Vector<zoneout::UUID>>::err(route_nodes.error());
        }

        return extract_traversed_zone_ids(index, route_nodes.value());
    }

    inline dp::Result<dp::u64> validate_route_plan_shape(const RoutePlan &route_plan) {
        if (route_plan.traversed_node_ids.empty()) {
            if (!route_plan.traversed_edge_ids.empty() || !route_plan.steps.empty()) {
                return dp::Result<dp::u64>::err(
                    dp::Error::invalid_argument("route plan contains edges or steps without any traversed nodes"));
            }
            return dp::Result<dp::u64>::ok(0);
        }

        if (route_plan.start_node_id != route_plan.traversed_node_ids.front()) {
            return dp::Result<dp::u64>::err(
                dp::Error::invalid_argument("route plan start node does not match the first traversed node"));
        }
        if (route_plan.goal_node_id != route_plan.traversed_node_ids.back()) {
            return dp::Result<dp::u64>::err(
                dp::Error::invalid_argument("route plan goal node does not match the last traversed node"));
        }
        if (route_plan.traversed_edge_ids.size() + 1 != route_plan.traversed_node_ids.size()) {
            return dp::Result<dp::u64>::err(
                dp::Error::invalid_argument("route plan edge count must equal node count minus one"));
        }
        if (!route_plan.steps.empty() && route_plan.steps.size() != route_plan.traversed_node_ids.size()) {
            return dp::Result<dp::u64>::err(
                dp::Error::invalid_argument("route plan step count must equal traversed node count"));
        }
        if (!route_plan.traversed_node_zone_ids.empty() &&
            route_plan.traversed_node_zone_ids.size() != route_plan.traversed_node_ids.size()) {
            return dp::Result<dp::u64>::err(
                dp::Error::invalid_argument("route plan node-zone coverage must align with traversed nodes"));
        }
        if (!route_plan.traversed_edge_zone_ids.empty() &&
            route_plan.traversed_edge_zone_ids.size() != route_plan.traversed_edge_ids.size()) {
            return dp::Result<dp::u64>::err(
                dp::Error::invalid_argument("route plan edge-zone coverage must align with traversed edges"));
        }

        for (dp::u64 i = 0; i < route_plan.steps.size(); ++i) {
            if (route_plan.steps[i].node_id != route_plan.traversed_node_ids[i]) {
                return dp::Result<dp::u64>::err(
                    dp::Error::invalid_argument("route plan step nodes must match traversed node sequence"));
            }
            if (i == 0) {
                if (route_plan.steps[i].incoming_edge_id.has_value()) {
                    return dp::Result<dp::u64>::err(
                        dp::Error::invalid_argument("route plan first step cannot have an incoming edge"));
                }
                continue;
            }
            if (!route_plan.steps[i].incoming_edge_id.has_value() ||
                route_plan.steps[i].incoming_edge_id.value() != route_plan.traversed_edge_ids[i - 1]) {
                return dp::Result<dp::u64>::err(
                    dp::Error::invalid_argument("route plan step incoming edges must match traversed edges"));
            }
        }

        return dp::Result<dp::u64>::ok(route_plan.traversed_edge_ids.size());
    }

    inline dp::Result<RoutePlan> build_route_plan(const WorkspaceIndex &index, const zoneout::UUID &start_node_id,
                                                  const zoneout::UUID &goal_node_id,
                                                  const dp::Vector<zoneout::UUID> &route_nodes,
                                                  RouteCostModel cost_model = RouteCostModel::GraphWeight) {
        RoutePlan plan{};
        plan.start_node_id = start_node_id;
        plan.goal_node_id = goal_node_id;
        plan.traversed_node_ids = route_nodes;

        if (!route_nodes.empty()) {
            if (route_nodes.front() != start_node_id) {
                return dp::Result<RoutePlan>::err(
                    dp::Error::invalid_argument("route node sequence does not begin at the requested start node"));
            }
            if (route_nodes.back() != goal_node_id) {
                return dp::Result<RoutePlan>::err(
                    dp::Error::invalid_argument("route node sequence does not end at the requested goal node"));
            }
        }

        const auto traversed_edges = extract_traversed_edge_ids(index, route_nodes);
        if (traversed_edges.is_err()) {
            return dp::Result<RoutePlan>::err(traversed_edges.error());
        }
        plan.traversed_edge_ids = traversed_edges.value();

        const auto traversed_zones = extract_traversed_zone_ids(index, route_nodes);
        if (traversed_zones.is_err()) {
            return dp::Result<RoutePlan>::err(traversed_zones.error());
        }
        plan.traversed_zone_ids = traversed_zones.value();

        for (const auto &node_id : route_nodes) {
            dp::Vector<zoneout::UUID> step_zone_ids;
            for (const auto *zone : index.zones_of_node(node_id)) {
                if (zone != nullptr) {
                    step_zone_ids.push_back(zone->id());
                }
            }
            plan.traversed_node_zone_ids.push_back(step_zone_ids);
        }
        for (const auto &edge_id : plan.traversed_edge_ids) {
            dp::Vector<zoneout::UUID> step_zone_ids;
            for (const auto *zone : index.zones_of_edge(edge_id)) {
                if (zone != nullptr) {
                    step_zone_ids.push_back(zone->id());
                }
            }
            plan.traversed_edge_zone_ids.push_back(step_zone_ids);
        }

        const auto total_cost = accumulate_route_cost(index, route_nodes, cost_model);
        if (total_cost.is_err()) {
            return dp::Result<RoutePlan>::err(total_cost.error());
        }
        plan.total_cost = total_cost.value();

        for (dp::u64 i = 0; i < route_nodes.size(); ++i) {
            RouteStep step{};
            step.node_id = route_nodes[i];
            if (i > 0) {
                step.incoming_edge_id = plan.traversed_edge_ids[i - 1];
                const auto partial_route = dp::Vector<zoneout::UUID>(route_nodes.begin() + static_cast<dp::i64>(i - 1),
                                                                     route_nodes.begin() + static_cast<dp::i64>(i + 1));
                const auto step_cost = accumulate_route_cost(index, partial_route, cost_model);
                if (step_cost.is_err()) {
                    return dp::Result<RoutePlan>::err(step_cost.error());
                }
                step.step_cost = step_cost.value();
                step.cumulative_cost =
                    plan.steps.empty() ? step.step_cost : plan.steps.back().cumulative_cost + step.step_cost;
            }
            plan.steps.push_back(step);
        }

        const auto valid = validate_route_plan_shape(plan);
        if (valid.is_err()) {
            return dp::Result<RoutePlan>::err(valid.error());
        }

        return dp::Result<RoutePlan>::ok(plan);
    }

    inline dp::Result<RoutePlan> build_route_plan(const WorkspaceIndex &index, const RouteSearchState &search,
                                                  const zoneout::UUID &start_node_id, const zoneout::UUID &goal_node_id,
                                                  RouteCostModel cost_model = RouteCostModel::GraphWeight) {
        const auto route_nodes = extract_traversed_node_ids(search, start_node_id, goal_node_id);
        if (route_nodes.is_err()) {
            return dp::Result<RoutePlan>::err(route_nodes.error());
        }

        return build_route_plan(index, start_node_id, goal_node_id, route_nodes.value(), cost_model);
    }

    inline RouteFailure diagnose_route_failure(const WorkspaceIndex &index, const zoneout::UUID &start_node_id,
                                               const zoneout::UUID &goal_node_id) {
        if (index.node(start_node_id) == nullptr) {
            return RouteFailure{RouteFailureKind::MissingStartNode,
                                dp::String{"start node is not present in workspace graph"},
                                {},
                                {},
                                {}};
        }
        if (index.node(goal_node_id) == nullptr) {
            return RouteFailure{RouteFailureKind::MissingGoalNode,
                                dp::String{"goal node is not present in workspace graph"},
                                {},
                                {},
                                {}};
        }

        const auto unconstrained = shortest_path_search(index, start_node_id, goal_node_id);
        if (unconstrained.found) {
            const auto blocked_search = shortest_path_search_with_blocking(index, start_node_id, goal_node_id);
            dp::Vector<zoneout::UUID> blocked_edge_ids;
            dp::Vector<zoneout::UUID> blocked_zone_ids;
            std::unordered_set<zoneout::UUID, zoneout::UUIDHash> seen_blocked_edge_ids;
            std::unordered_set<zoneout::UUID, zoneout::UUIDHash> seen_blocked_zone_ids;

            dp::Vector<zoneout::UUID> reachable_node_ids;
            reachable_node_ids.reserve(blocked_search.distances.size());
            for (const auto &[node_id, _] : blocked_search.distances) {
                reachable_node_ids.push_back(node_id);
            }
            if (reachable_node_ids.empty()) {
                reachable_node_ids.push_back(start_node_id);
            }

            GraphTraversalAdapter adapter{index};
            for (const auto &node_id : reachable_node_ids) {
                for (const auto &neighbor : adapter.neighbors(node_id)) {
                    if (!is_edge_hard_blocked(index, neighbor.edge_id)) {
                        continue;
                    }

                    if (seen_blocked_edge_ids.insert(neighbor.edge_id).second) {
                        blocked_edge_ids.push_back(neighbor.edge_id);
                    }

                    for (const auto &zone_id : blocked_zones_for_edge(index, neighbor.edge_id)) {
                        if (seen_blocked_zone_ids.insert(zone_id).second) {
                            blocked_zone_ids.push_back(zone_id);
                        }
                    }
                }
            }

            if (blocked_edge_ids.empty()) {
                const auto unconstrained_nodes = reconstruct_route_nodes(unconstrained, start_node_id, goal_node_id);
                const auto unconstrained_edges = extract_traversed_edge_ids(index, unconstrained_nodes);
                if (unconstrained_edges.is_ok()) {
                    for (const auto &edge_id : unconstrained_edges.value()) {
                        if (!is_edge_hard_blocked(index, edge_id)) {
                            continue;
                        }
                        if (seen_blocked_edge_ids.insert(edge_id).second) {
                            blocked_edge_ids.push_back(edge_id);
                        }
                        for (const auto &zone_id : blocked_zones_for_edge(index, edge_id)) {
                            if (seen_blocked_zone_ids.insert(zone_id).second) {
                                blocked_zone_ids.push_back(zone_id);
                            }
                        }
                    }
                }
            }

            dp::String message = "route is blocked by traffic policy";
            if (!blocked_edge_ids.empty() || !blocked_zone_ids.empty()) {
                message += " (";
                message += std::to_string(blocked_edge_ids.size());
                message += " blocked edge(s), ";
                message += std::to_string(blocked_zone_ids.size());
                message += " blocked zone(s))";
            }

            return RouteFailure{RouteFailureKind::PolicyBlocked, message, blocked_edge_ids, blocked_zone_ids,
                                reachable_node_ids};
        }

        dp::Vector<zoneout::UUID> reachable_node_ids;
        reachable_node_ids.reserve(unconstrained.distances.size());
        for (const auto &[node_id, _] : unconstrained.distances) {
            reachable_node_ids.push_back(node_id);
        }
        if (reachable_node_ids.empty() && index.node(start_node_id) != nullptr) {
            reachable_node_ids.push_back(start_node_id);
        }

        dp::String message = "goal is unreachable from start";
        message += " after reaching ";
        message += std::to_string(reachable_node_ids.size());
        message += " node(s)";

        return RouteFailure{RouteFailureKind::Unreachable, message, {}, {}, reachable_node_ids};
    }

    inline RoutePlanningResult plan_route(const WorkspaceIndex &index, const zoneout::UUID &start_node_id,
                                          const zoneout::UUID &goal_node_id, bool use_penalties = true) {
        RoutePlanningResult result{};
        result.search = use_penalties ? shortest_path_search_with_penalties(index, start_node_id, goal_node_id)
                                      : shortest_path_search_with_blocking(index, start_node_id, goal_node_id);

        if (!result.search.found) {
            result.failure = diagnose_route_failure(index, start_node_id, goal_node_id);
            return result;
        }

        const auto cost_model = use_penalties ? RouteCostModel::Penalized : RouteCostModel::GraphWeight;
        const auto route_plan = build_route_plan(index, result.search, start_node_id, goal_node_id, cost_model);
        if (route_plan.is_err()) {
            result.failure = RouteFailure{RouteFailureKind::Unreachable, route_plan.error().message, {}, {}, {}};
            return result;
        }

        result.plan = route_plan.value();
        return result;
    }

} // namespace timenav
