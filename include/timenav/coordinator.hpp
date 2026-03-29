#pragma once

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "timenav/claim_manager.hpp"
#include "timenav/robot_state.hpp"

namespace timenav {

    inline dp::Vector<ClaimTarget> claim_targets_from_route(const RoutePlan &route_plan) {
        dp::Vector<ClaimTarget> targets;

        for (const auto &zone_id : route_plan.traversed_zone_ids) {
            targets.push_back(ClaimTarget{ClaimTargetKind::Zone, zone_id});
        }
        for (const auto &edge_id : route_plan.traversed_edge_ids) {
            targets.push_back(ClaimTarget{ClaimTargetKind::Edge, edge_id});
        }
        for (const auto &node_id : route_plan.traversed_node_ids) {
            targets.push_back(ClaimTarget{ClaimTargetKind::Node, node_id});
        }

        return targets;
    }

    inline ClaimWindow claim_window_from_route(const RoutePlan &route_plan, dp::u64 start_tick = 0,
                                               dp::f64 ticks_per_cost_unit = 1.0) {
        ClaimWindow window{};
        window.start_tick = start_tick;
        const auto duration =
            static_cast<dp::u64>(std::max<dp::f64>(0.0, std::ceil(route_plan.total_cost * ticks_per_cost_unit)));
        window.end_tick = start_tick + duration;
        return window;
    }

    inline ClaimRequest claim_request_from_route(ClaimId claim_id, RobotId robot_id, MissionId mission_id,
                                                 const RoutePlan &route_plan,
                                                 ClaimAccessMode access_mode = ClaimAccessMode::Exclusive) {
        ClaimRequest request{};
        request.id = claim_id;
        request.robot_id = robot_id;
        request.mission_id = mission_id;
        request.access_mode = access_mode;
        request.targets = claim_targets_from_route(route_plan);
        return request;
    }
    inline ClaimRequest claim_request_from_route(ClaimId claim_id, RobotId robot_id, MissionId mission_id,
                                                 const RoutePlan &route_plan, dp::u64 start_tick,
                                                 dp::f64 ticks_per_cost_unit,
                                                 ClaimAccessMode access_mode = ClaimAccessMode::Exclusive) {
        ClaimRequest request = claim_request_from_route(claim_id, robot_id, mission_id, route_plan, access_mode);
        request.requested_at_tick = start_tick;
        request.window = claim_window_from_route(route_plan, start_tick, ticks_per_cost_unit);
        return request;
    }

    inline ClaimRequest rolling_horizon_claim_request(ClaimId claim_id, const RobotState &state,
                                                      ClaimAccessMode access_mode = ClaimAccessMode::Exclusive) {
        ClaimRequest request{};
        request.id = claim_id;
        request.robot_id = state.robot_id;
        request.mission_id = state.mission_id;
        request.access_mode = access_mode;

        if (!state.route_plan.has_value()) {
            return request;
        }

        const auto &route_plan = state.route_plan.value();
        dp::u64 start_node_index = state.next_route_step_index;
        if (state.current_node_id.has_value()) {
            const auto current_node_it = std::find(route_plan.traversed_node_ids.begin(),
                                                   route_plan.traversed_node_ids.end(), state.current_node_id.value());
            if (current_node_it != route_plan.traversed_node_ids.end()) {
                start_node_index =
                    static_cast<dp::u64>(std::distance(route_plan.traversed_node_ids.begin(), current_node_it));
            }
        }

        const auto available_nodes = route_plan.traversed_node_ids.size() > start_node_index
                                         ? route_plan.traversed_node_ids.size() - start_node_index
                                         : 0;
        const auto available_edges = route_plan.traversed_edge_ids.size() > start_node_index
                                         ? route_plan.traversed_edge_ids.size() - start_node_index
                                         : 0;
        const auto available_zones = route_plan.traversed_zone_ids.size() > start_node_index
                                         ? route_plan.traversed_zone_ids.size() - start_node_index
                                         : 0;
        const auto node_limit = std::min<dp::u64>(available_nodes, state.horizon + 1);
        const auto edge_limit = std::min<dp::u64>(available_edges, state.horizon);
        const auto zone_limit = std::min<dp::u64>(available_zones, state.horizon + 1);

        for (dp::u64 i = 0; i < zone_limit; ++i) {
            request.targets.push_back(
                ClaimTarget{ClaimTargetKind::Zone, route_plan.traversed_zone_ids[start_node_index + i]});
        }
        for (dp::u64 i = 0; i < edge_limit; ++i) {
            request.targets.push_back(
                ClaimTarget{ClaimTargetKind::Edge, route_plan.traversed_edge_ids[start_node_index + i]});
        }
        for (dp::u64 i = 0; i < node_limit; ++i) {
            request.targets.push_back(
                ClaimTarget{ClaimTargetKind::Node, route_plan.traversed_node_ids[start_node_index + i]});
        }

        request.requested_at_tick = state.updated_at_tick;
        if (start_node_index < route_plan.steps.size()) {
            const auto remaining_cost = route_plan.total_cost - route_plan.steps[start_node_index].cumulative_cost;
            request.window.start_tick = state.updated_at_tick;
            request.window.end_tick =
                state.updated_at_tick + static_cast<dp::u64>(std::ceil(std::max<dp::f64>(0.0, remaining_cost)));
        }

        return request;
    }

    inline dp::u64 release_targets_behind_progress(RobotState &state, ClaimManager &claim_manager) {
        if (!state.route_plan.has_value() || !state.current_node_id.has_value()) {
            return 0;
        }

        const auto &route_plan = state.route_plan.value();
        const auto current_node_it = std::find(route_plan.traversed_node_ids.begin(),
                                               route_plan.traversed_node_ids.end(), *state.current_node_id);
        if (current_node_it == route_plan.traversed_node_ids.end()) {
            return 0;
        }

        const auto current_index =
            static_cast<dp::u64>(std::distance(route_plan.traversed_node_ids.begin(), current_node_it));
        std::unordered_set<zoneout::UUID, zoneout::UUIDHash> remaining_node_ids;
        std::unordered_set<zoneout::UUID, zoneout::UUIDHash> remaining_edge_ids;

        for (dp::u64 i = current_index; i < route_plan.traversed_node_ids.size(); ++i) {
            remaining_node_ids.insert(route_plan.traversed_node_ids[i]);
        }
        for (dp::u64 i = current_index; i < route_plan.traversed_edge_ids.size(); ++i) {
            remaining_edge_ids.insert(route_plan.traversed_edge_ids[i]);
        }

        dp::Vector<LeaseId> retained_lease_ids;
        dp::u64 released = 0;

        for (const auto lease_id : state.active_lease_ids) {
            const auto *lease = claim_manager.find_lease(lease_id);
            if (lease == nullptr) {
                continue;
            }

            bool keep = false;
            for (const auto &target : lease->targets) {
                if ((target.kind == ClaimTargetKind::Node && remaining_node_ids.count(target.resource_id) > 0) ||
                    (target.kind == ClaimTargetKind::Edge && remaining_edge_ids.count(target.resource_id) > 0) ||
                    target.kind == ClaimTargetKind::Zone) {
                    keep = true;
                    break;
                }
            }

            if (keep) {
                retained_lease_ids.push_back(lease_id);
            } else if (claim_manager.release_lease(lease_id)) {
                ++released;
            }
        }

        state.active_lease_ids = retained_lease_ids;
        return released;
    }

    inline dp::Vector<zoneout::UUID> route_schedule_window_conflicts(const WorkspaceIndex &index,
                                                                     const RoutePlan &route_plan,
                                                                     std::string_view active_window) {
        dp::Vector<zoneout::UUID> conflicting_zone_ids;

        for (const auto &zone_id : route_plan.traversed_zone_ids) {
            const auto schedule_window = index.zone_property(zone_id, "traffic.schedule_window");
            if (schedule_window.has_value() && schedule_window.value() != active_window) {
                conflicting_zone_ids.push_back(zone_id);
            }
        }

        return conflicting_zone_ids;
    }

    inline bool route_matches_schedule_window(const WorkspaceIndex &index, const RoutePlan &route_plan,
                                              std::string_view active_window) {
        return route_schedule_window_conflicts(index, route_plan, active_window).empty();
    }

    enum class ArbitrationDecision { Proceed, Yield, Replan };

    struct ArbitrationContext {
        dp::f64 self_priority = 0.0;
        dp::f64 other_priority = 0.0;
        bool self_holds_lease = false;
        bool other_holds_lease = false;
    };

    inline ArbitrationDecision arbitrate_right_of_way(const ArbitrationContext &context) {
        if (context.other_holds_lease && !context.self_holds_lease) {
            return ArbitrationDecision::Yield;
        }
        if (context.self_holds_lease && !context.other_holds_lease) {
            return ArbitrationDecision::Proceed;
        }
        if (context.self_priority > context.other_priority) {
            return ArbitrationDecision::Proceed;
        }
        if (context.self_priority < context.other_priority) {
            return ArbitrationDecision::Yield;
        }
        return ArbitrationDecision::Replan;
    }

    class Coordinator {
      public:
        Coordinator() = default;
        explicit Coordinator(const WorkspaceIndex &index) : index_(&index), claim_manager_(index) {}

        [[nodiscard]] const WorkspaceIndex *index() const noexcept { return index_; }
        [[nodiscard]] bool has_index() const noexcept { return index_ != nullptr; }
        [[nodiscard]] const ClaimManager &claim_manager() const noexcept { return claim_manager_; }
        [[nodiscard]] ClaimManager &claim_manager() noexcept { return claim_manager_; }
        [[nodiscard]] bool empty() const noexcept { return robot_count() == 0; }
        [[nodiscard]] dp::u64 robot_count() const noexcept { return robot_states_.size(); }
        [[nodiscard]] const dp::Vector<RobotState> &robot_states() const noexcept { return robot_states_; }
        void bind_index(const WorkspaceIndex &index) {
            index_ = &index;
            claim_manager_.bind_index(index);
        }
        void clear() {
            robot_states_.clear();
            claim_manager_.clear();
        }

        void register_robot(const RobotState &state) {
            if (auto *existing = find_robot_state(state.robot_id); existing != nullptr) {
                *existing = state;
                return;
            }

            robot_states_.push_back(state);
        }
        [[nodiscard]] bool unregister_robot(RobotId robot_id) {
            for (auto it = robot_states_.begin(); it != robot_states_.end(); ++it) {
                if (it->robot_id == robot_id) {
                    robot_states_.erase(it);
                    return true;
                }
            }

            return false;
        }

        [[nodiscard]] RobotState *find_robot_state(RobotId robot_id) noexcept {
            for (auto &state : robot_states_) {
                if (state.robot_id == robot_id) {
                    return &state;
                }
            }

            return nullptr;
        }

        [[nodiscard]] const RobotState *find_robot_state(RobotId robot_id) const noexcept {
            for (const auto &state : robot_states_) {
                if (state.robot_id == robot_id) {
                    return &state;
                }
            }

            return nullptr;
        }

        [[nodiscard]] ClaimRequest
        claim_request_for_robot(RobotId robot_id, ClaimId claim_id,
                                ClaimAccessMode access_mode = ClaimAccessMode::Exclusive) const {
            const auto *state = find_robot_state(robot_id);
            if (state == nullptr) {
                return ClaimRequest{};
            }

            return rolling_horizon_claim_request(claim_id, *state, access_mode);
        }

        [[nodiscard]] bool update_robot_progress(RobotId robot_id, const dp::Optional<zoneout::UUID> &current_node_id,
                                                 const dp::Optional<zoneout::UUID> &current_edge_id,
                                                 dp::u64 updated_at_tick) {
            auto *state = find_robot_state(robot_id);
            if (state == nullptr) {
                return false;
            }

            state->current_node_id = current_node_id;
            state->current_edge_id = current_edge_id;
            if (state->route_plan.has_value() && current_node_id.has_value()) {
                const auto node_it = std::find(state->route_plan->traversed_node_ids.begin(),
                                               state->route_plan->traversed_node_ids.end(), current_node_id.value());
                if (node_it != state->route_plan->traversed_node_ids.end()) {
                    state->next_route_step_index =
                        static_cast<dp::u64>(std::distance(state->route_plan->traversed_node_ids.begin(), node_it));
                    state->progress_state =
                        state->next_route_step_index + 1 >= state->route_plan->traversed_node_ids.size()
                            ? RobotProgressState::Idle
                            : RobotProgressState::FollowingRoute;
                    state->hold_reason = dp::nullopt;
                }
            } else if (current_edge_id.has_value()) {
                state->progress_state = RobotProgressState::FollowingRoute;
            } else {
                state->progress_state = RobotProgressState::Waiting;
            }
            state->updated_at_tick = updated_at_tick;
            return true;
        }
        [[nodiscard]] bool assign_route_plan(RobotId robot_id, const RoutePlan &route_plan, dp::u64 horizon,
                                             dp::u64 updated_at_tick) {
            auto *state = find_robot_state(robot_id);
            if (state == nullptr) {
                return false;
            }

            state->route_plan = route_plan;
            state->horizon = horizon;
            state->next_route_step_index = 0;
            state->progress_state =
                route_plan.traversed_node_ids.empty() ? RobotProgressState::Idle : RobotProgressState::FollowingRoute;
            state->updated_at_tick = updated_at_tick;
            return true;
        }
        [[nodiscard]] bool update_robot_claim_state(RobotId robot_id, const dp::Vector<ClaimId> &pending_claim_ids,
                                                    const dp::Vector<LeaseId> &active_lease_ids,
                                                    dp::Optional<dp::u64> last_claim_tick = dp::nullopt) {
            auto *state = find_robot_state(robot_id);
            if (state == nullptr) {
                return false;
            }

            state->pending_claim_ids = pending_claim_ids;
            state->active_lease_ids = active_lease_ids;
            state->last_claim_tick = last_claim_tick;
            return true;
        }

        [[nodiscard]] dp::u64 release_behind_progress(RobotId robot_id) {
            auto *state = find_robot_state(robot_id);
            if (state == nullptr) {
                return 0;
            }

            return release_targets_behind_progress(*state, claim_manager_);
        }

      private:
        const WorkspaceIndex *index_ = nullptr;
        ClaimManager claim_manager_{};
        dp::Vector<RobotState> robot_states_{};
    };

} // namespace timenav
