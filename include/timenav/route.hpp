#pragma once

#include <algorithm>
#include <limits>
#include <queue>
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
    };

    struct RoutePlan {
        zoneout::UUID start_node_id;
        zoneout::UUID goal_node_id;
        dp::Vector<RouteStep> steps;
        dp::Vector<zoneout::UUID> traversed_node_ids;
        dp::Vector<zoneout::UUID> traversed_edge_ids;
        dp::Vector<zoneout::UUID> traversed_zone_ids;
        dp::f64 total_cost = 0.0;
    };

    struct RouteSearchState {
        bool found = false;
        dp::f64 distance = std::numeric_limits<dp::f64>::infinity();
        std::unordered_map<zoneout::UUID, dp::f64, zoneout::UUIDHash> distances;
        std::unordered_map<zoneout::UUID, zoneout::UUID, zoneout::UUIDHash> predecessors;
    };

    struct TraversalNeighbor {
        zoneout::UUID node_id;
        zoneout::UUID edge_id;
        dp::f64 weight = 0.0;
    };

    inline bool is_edge_hard_blocked(const WorkspaceIndex &index, const zoneout::UUID &edge_id) {
        const auto *edge_data = index.edge(edge_id);
        if (edge_data == nullptr) {
            return true;
        }

        const auto edge_semantics = parse_edge_traffic_semantics(edge_data->properties);
        if (edge_semantics.blocked.value_or(false)) {
            return true;
        }

        for (const auto *zone : index.zones_of_edge(edge_id)) {
            if (zone == nullptr) {
                continue;
            }

            const auto zone_policy = parse_zone_policy(zone->properties());
            if (zone_policy.blocked.value_or(false) || zone_policy.kind == ZonePolicyKind::Restricted) {
                return true;
            }
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

            for (const auto edge_id : workspace->graph().out_edges(*vertex_id)) {
                const auto other_vertex = workspace->graph().source(edge_id) == *vertex_id
                                              ? workspace->graph().target(edge_id)
                                              : workspace->graph().source(edge_id);
                result.push_back(TraversalNeighbor{workspace->graph()[other_vertex].id,
                                                   workspace->graph().edge_property(edge_id).id,
                                                   workspace->graph().get_weight(edge_id)});
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

    inline dp::Result<dp::f64> accumulate_route_cost(const WorkspaceIndex &index,
                                                     const dp::Vector<zoneout::UUID> &route_nodes) {
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

    inline dp::Result<RoutePlan> build_route_plan(const WorkspaceIndex &index, const zoneout::UUID &start_node_id,
                                                  const zoneout::UUID &goal_node_id,
                                                  const dp::Vector<zoneout::UUID> &route_nodes) {
        RoutePlan plan{};
        plan.start_node_id = start_node_id;
        plan.goal_node_id = goal_node_id;
        plan.traversed_node_ids = route_nodes;

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

        const auto total_cost = accumulate_route_cost(index, route_nodes);
        if (total_cost.is_err()) {
            return dp::Result<RoutePlan>::err(total_cost.error());
        }
        plan.total_cost = total_cost.value();

        for (dp::u64 i = 0; i < route_nodes.size(); ++i) {
            RouteStep step{};
            step.node_id = route_nodes[i];
            if (i > 0) {
                step.incoming_edge_id = plan.traversed_edge_ids[i - 1];
            }
            plan.steps.push_back(step);
        }

        return dp::Result<RoutePlan>::ok(plan);
    }

} // namespace timenav
