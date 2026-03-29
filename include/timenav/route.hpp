#pragma once

#include <limits>
#include <unordered_map>

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

} // namespace timenav
