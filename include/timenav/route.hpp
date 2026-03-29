#pragma once

#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "timenav/workspace_index.hpp"
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

} // namespace timenav
