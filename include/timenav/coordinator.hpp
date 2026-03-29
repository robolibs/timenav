#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

#include "timenav/claim_manager.hpp"
#include "timenav/robot_state.hpp"

namespace timenav {

    struct ClaimTargetSemantics {
        ClaimTarget target{};
        bool requires_claim = false;
        bool waiting_allowed = true;
        bool stop_allowed = true;
        bool blocked = false;
        bool corridor = false;
        bool slowdown = false;
        dp::Optional<dp::String> schedule_window;
        dp::Optional<dp::String> access_group;
    };

    struct ScheduledTargetWindow {
        ClaimTargetSemantics semantics{};
        dp::u64 start_tick = 0;
        dp::u64 end_tick = 0;
    };

    struct ScheduleConflict {
        ClaimTarget target{};
        dp::u64 blocking_until_tick = 0;
        dp::Optional<ClaimId> conflicting_claim_id;
        dp::Optional<LeaseId> conflicting_lease_id;
        dp::Vector<dp::String> diagnostics;
    };

    enum class ScheduleDecisionKind { Proceed, Queue, Replan };

    struct ScheduleDecision {
        ScheduleDecisionKind kind = ScheduleDecisionKind::Proceed;
        dp::u64 start_tick = 0;
        dp::u64 queue_position = 0;
        dp::Vector<ScheduleConflict> conflicts;
        dp::Vector<dp::String> diagnostics;
    };

    inline ClaimTargetSemantics claim_target_semantics(const WorkspaceIndex &index, const ClaimTarget &target) {
        ClaimTargetSemantics semantics{};
        semantics.target = target;

        if (target.kind == ClaimTargetKind::Zone) {
            const auto *zone = index.zone(target.resource_id);
            if (zone == nullptr) {
                return semantics;
            }

            const auto policy = parse_zone_policy(zone->properties());
            semantics.requires_claim = policy.requires_claim;
            semantics.waiting_allowed = policy.waiting_allowed.value_or(true);
            semantics.stop_allowed = policy.stop_allowed.value_or(!policy.blocked.value_or(false));
            semantics.blocked = policy.blocked.value_or(false) || policy.blocks_entry_without_grant ||
                                policy.blocks_traversal_without_grant;
            semantics.corridor = policy.kind == ZonePolicyKind::Corridor;
            semantics.slowdown = policy.kind == ZonePolicyKind::Slowdown ||
                                 (policy.speed_limit.has_value() && policy.speed_limit.value() < 1.0);
            semantics.schedule_window = policy.schedule_window;
            semantics.access_group = policy.access_group;
            return semantics;
        }

        if (target.kind == ClaimTargetKind::Edge) {
            const auto *edge = index.edge(target.resource_id);
            if (edge == nullptr) {
                return semantics;
            }

            dp::Vector<ZonePolicy> zone_policies;
            for (const auto *zone : index.zones_of_edge(target.resource_id)) {
                if (zone != nullptr) {
                    zone_policies.push_back(parse_zone_policy(zone->properties()));
                }
            }
            const auto edge_semantics = derive_effective_edge_semantics(edge->properties, false, zone_policies);
            semantics.requires_claim = edge_semantics.requires_claim.value_or(false);
            semantics.waiting_allowed = edge_semantics.waiting_allowed.value_or(true);
            semantics.stop_allowed = edge_semantics.stop_allowed.value_or(!edge_semantics.blocked.value_or(false));
            semantics.blocked = edge_semantics.blocked.value_or(false);
            semantics.corridor = edge_semantics.lane_type.has_value() && edge_semantics.lane_type.value() == "corridor";
            semantics.slowdown = edge_semantics.speed_limit.has_value() && edge_semantics.speed_limit.value() < 1.0;
            semantics.schedule_window = edge_semantics.schedule_window;
            semantics.access_group = edge_semantics.access_group;
            return semantics;
        }

        if (target.kind == ClaimTargetKind::Node) {
            for (const auto *zone : index.zones_of_node(target.resource_id)) {
                if (zone == nullptr) {
                    continue;
                }
                const auto policy = parse_zone_policy(zone->properties());
                semantics.requires_claim = semantics.requires_claim || policy.requires_claim;
                semantics.waiting_allowed = semantics.waiting_allowed && policy.waiting_allowed.value_or(true);
                semantics.stop_allowed = semantics.stop_allowed && policy.stop_allowed.value_or(true);
                semantics.blocked = semantics.blocked || policy.blocked.value_or(false) ||
                                    policy.blocks_entry_without_grant || policy.blocks_traversal_without_grant;
                semantics.corridor = semantics.corridor || policy.kind == ZonePolicyKind::Corridor;
                semantics.slowdown = semantics.slowdown || policy.kind == ZonePolicyKind::Slowdown ||
                                     (policy.speed_limit.has_value() && policy.speed_limit.value() < 1.0);
                if (!semantics.schedule_window.has_value() && policy.schedule_window.has_value()) {
                    semantics.schedule_window = policy.schedule_window;
                }
                if (!semantics.access_group.has_value() && policy.access_group.has_value()) {
                    semantics.access_group = policy.access_group;
                }
            }
        }

        return semantics;
    }

    inline dp::Vector<ScheduledTargetWindow> scheduled_target_windows_from_route(const WorkspaceIndex &index,
                                                                                 const RoutePlan &route_plan,
                                                                                 dp::u64 start_tick = 0,
                                                                                 dp::f64 ticks_per_cost_unit = 1.0) {
        dp::Vector<ScheduledTargetWindow> windows;
        if (validate_route_plan_shape(route_plan).is_err()) {
            return windows;
        }

        auto tick_at_step = [&](dp::u64 step_index) {
            if (step_index >= route_plan.steps.size()) {
                return start_tick;
            }
            const auto offset = static_cast<dp::u64>(
                std::max<dp::f64>(0.0, std::ceil(route_plan.steps[step_index].cumulative_cost * ticks_per_cost_unit)));
            return start_tick + offset;
        };

        for (dp::u64 i = 0; i < route_plan.traversed_node_ids.size(); ++i) {
            const auto window_start = tick_at_step(i);
            const auto next_tick = i + 1 < route_plan.steps.size() ? tick_at_step(i + 1) : window_start;
            const auto window_end = std::max(window_start, next_tick);

            ScheduledTargetWindow node_window{};
            node_window.semantics =
                claim_target_semantics(index, ClaimTarget{ClaimTargetKind::Node, route_plan.traversed_node_ids[i]});
            node_window.start_tick = window_start;
            node_window.end_tick = window_end;
            windows.push_back(node_window);

            if (i < route_plan.traversed_node_zone_ids.size()) {
                for (const auto &zone_id : route_plan.traversed_node_zone_ids[i]) {
                    ScheduledTargetWindow zone_window{};
                    zone_window.semantics = claim_target_semantics(index, ClaimTarget{ClaimTargetKind::Zone, zone_id});
                    zone_window.start_tick = window_start;
                    zone_window.end_tick = window_end;
                    windows.push_back(zone_window);
                }
            }

            if (i < route_plan.traversed_edge_ids.size()) {
                ScheduledTargetWindow edge_window{};
                edge_window.semantics =
                    claim_target_semantics(index, ClaimTarget{ClaimTargetKind::Edge, route_plan.traversed_edge_ids[i]});
                edge_window.start_tick = window_start;
                edge_window.end_tick = std::max(window_start, tick_at_step(i + 1));
                windows.push_back(edge_window);

                if (i < route_plan.traversed_edge_zone_ids.size()) {
                    for (const auto &zone_id : route_plan.traversed_edge_zone_ids[i]) {
                        ScheduledTargetWindow zone_window{};
                        zone_window.semantics =
                            claim_target_semantics(index, ClaimTarget{ClaimTargetKind::Zone, zone_id});
                        zone_window.start_tick = edge_window.start_tick;
                        zone_window.end_tick = edge_window.end_tick;
                        windows.push_back(zone_window);
                    }
                }
            }
        }

        return windows;
    }

    inline bool claim_target_windows_overlap(const ClaimTarget &lhs, const ClaimWindow &lhs_window,
                                             const ClaimTarget &rhs, const ClaimWindow &rhs_window,
                                             const WorkspaceIndex *index = nullptr) {
        const auto lhs_start = lhs_window.start_tick.value_or(0);
        const auto rhs_start = rhs_window.start_tick.value_or(0);
        const auto lhs_end = lhs_window.end_tick.value_or(std::numeric_limits<dp::u64>::max());
        const auto rhs_end = rhs_window.end_tick.value_or(std::numeric_limits<dp::u64>::max());
        if (!(lhs_start <= rhs_end && rhs_start <= lhs_end)) {
            return false;
        }

        if (lhs.kind != rhs.kind) {
            return false;
        }
        if (lhs.resource_id == rhs.resource_id) {
            return true;
        }
        if (lhs.kind != ClaimTargetKind::Zone || index == nullptr) {
            return false;
        }

        if (const auto *lhs_zone = index->zone(lhs.resource_id); lhs_zone != nullptr) {
            for (const auto *ancestor : index->ancestor_zones(lhs.resource_id)) {
                if (ancestor != nullptr && ancestor->id() == rhs.resource_id) {
                    return true;
                }
            }
        }
        if (const auto *rhs_zone = index->zone(rhs.resource_id); rhs_zone != nullptr) {
            for (const auto *ancestor : index->ancestor_zones(rhs.resource_id)) {
                if (ancestor != nullptr && ancestor->id() == lhs.resource_id) {
                    return true;
                }
            }
        }

        return false;
    }

    inline ScheduleDecision schedule_route_request(const WorkspaceIndex &index, const ClaimManager &claim_manager,
                                                   const ClaimRequest &request, const RoutePlan &route_plan,
                                                   dp::u64 start_tick = 0, dp::f64 ticks_per_cost_unit = 1.0) {
        ScheduleDecision decision{};
        decision.start_tick = start_tick;

        const auto windows = scheduled_target_windows_from_route(index, route_plan, start_tick, ticks_per_cost_unit);
        dp::u64 latest_blocking_tick = start_tick;

        auto add_conflict = [&](const ClaimTarget &target, dp::u64 blocking_until_tick,
                                dp::Optional<ClaimId> conflicting_claim_id, dp::Optional<LeaseId> conflicting_lease_id,
                                const ClaimTargetSemantics &semantics, dp::String source) {
            ScheduleConflict conflict{};
            conflict.target = target;
            conflict.blocking_until_tick = blocking_until_tick;
            conflict.conflicting_claim_id = conflicting_claim_id;
            conflict.conflicting_lease_id = conflicting_lease_id;
            conflict.diagnostics.push_back(dp::String{"schedule conflict with "} + source);
            if (semantics.corridor) {
                conflict.diagnostics.push_back("corridor resources cannot be used for side waiting");
            }
            if (!semantics.waiting_allowed) {
                conflict.diagnostics.push_back("waiting is not allowed on the blocking resource");
            }
            if (!semantics.stop_allowed) {
                conflict.diagnostics.push_back("stopping is not allowed on the blocking resource");
            }
            if (semantics.blocked) {
                conflict.diagnostics.push_back("blocking resource is hard-restricted");
            }
            if (semantics.slowdown) {
                conflict.diagnostics.push_back("blocking resource applies slowdown semantics");
            }
            if (semantics.schedule_window.has_value()) {
                conflict.diagnostics.push_back(dp::String{"blocking schedule window="} +
                                               semantics.schedule_window.value());
            }
            decision.conflicts.push_back(conflict);
            latest_blocking_tick = std::max(latest_blocking_tick, blocking_until_tick);
        };

        for (const auto &window : windows) {
            ClaimWindow requested_window{};
            requested_window.start_tick = window.start_tick;
            requested_window.end_tick = window.end_tick;

            for (const auto &active_request : claim_manager.requests()) {
                if (active_request.id == request.id) {
                    continue;
                }
                for (const auto &target : active_request.targets) {
                    if (!claim_target_windows_overlap(window.semantics.target, requested_window, target,
                                                      active_request.window, claim_manager.index())) {
                        continue;
                    }
                    add_conflict(window.semantics.target, active_request.window.end_tick.value_or(window.end_tick),
                                 active_request.id, dp::nullopt, window.semantics, "active request");
                    break;
                }
            }

            for (const auto &lease : claim_manager.leases()) {
                if (!lease.active) {
                    continue;
                }
                ClaimWindow lease_window{};
                lease_window.start_tick = lease.granted_at_tick;
                lease_window.end_tick = lease.expires_at_tick;
                for (const auto &target : lease.targets) {
                    if (!claim_target_windows_overlap(window.semantics.target, requested_window, target, lease_window,
                                                      claim_manager.index())) {
                        continue;
                    }
                    add_conflict(window.semantics.target, lease.expires_at_tick.value_or(window.end_tick), dp::nullopt,
                                 lease.id, window.semantics, "active lease");
                    break;
                }
            }
        }

        if (decision.conflicts.empty()) {
            decision.kind = ScheduleDecisionKind::Proceed;
            decision.diagnostics.push_back("route can proceed within the requested reservation window");
            return decision;
        }

        bool queueable = true;
        for (const auto &conflict : decision.conflicts) {
            const auto semantics = claim_target_semantics(index, conflict.target);
            if (semantics.blocked || semantics.corridor || !semantics.waiting_allowed || !semantics.stop_allowed) {
                queueable = false;
                break;
            }
        }

        if (!queueable) {
            decision.kind = ScheduleDecisionKind::Replan;
            decision.diagnostics.push_back("schedule conflicts require replanning instead of queuing");
            return decision;
        }

        decision.kind = ScheduleDecisionKind::Queue;
        decision.start_tick = latest_blocking_tick + 1;
        decision.queue_position = decision.conflicts.size() + 1;
        decision.diagnostics.push_back("route should wait for an available reservation window");
        return decision;
    }

    inline bool robot_missed_schedule_slot(const RobotState &state, dp::u64 current_tick, dp::u64 grace_ticks = 0) {
        return state.scheduled_start_tick.has_value() &&
               current_tick > state.scheduled_start_tick.value() + grace_ticks &&
               state.progress_state != RobotProgressState::FollowingRoute &&
               state.progress_state != RobotProgressState::Idle;
    }

    inline void apply_schedule_decision(RobotState &state, const ScheduleDecision &decision, dp::u64 updated_at_tick) {
        state.updated_at_tick = updated_at_tick;
        state.scheduled_start_tick = decision.start_tick;
        state.wait_ticks = decision.start_tick > updated_at_tick ? decision.start_tick - updated_at_tick : 0;
        state.needs_replan = decision.kind == ScheduleDecisionKind::Replan;
        if (decision.kind == ScheduleDecisionKind::Proceed) {
            state.progress_state =
                state.route_plan.has_value() ? RobotProgressState::FollowingRoute : RobotProgressState::Idle;
            state.hold_reason = dp::nullopt;
            return;
        }
        if (decision.kind == ScheduleDecisionKind::Queue) {
            state.progress_state = RobotProgressState::Queued;
            state.hold_reason = "queued_for_reservation_window";
            return;
        }

        state.progress_state = RobotProgressState::Replanning;
        state.hold_reason = "schedule_conflict_requires_replan";
    }

    inline dp::u64 route_progress_index(const RobotState &state) {
        if (!state.route_plan.has_value()) {
            return state.next_route_step_index;
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

        return start_node_index;
    }

    inline dp::Vector<zoneout::UUID> route_zone_targets_from_progress(const RoutePlan &route_plan,
                                                                      dp::u64 start_node_index, dp::u64 horizon) {
        std::unordered_set<zoneout::UUID, zoneout::UUIDHash> seen_zone_ids;
        dp::Vector<zoneout::UUID> zone_ids;

        const auto node_limit =
            std::min<dp::u64>(route_plan.traversed_node_zone_ids.size(), start_node_index + horizon + 1);
        for (dp::u64 i = start_node_index; i < node_limit; ++i) {
            for (const auto &zone_id : route_plan.traversed_node_zone_ids[i]) {
                if (seen_zone_ids.insert(zone_id).second) {
                    zone_ids.push_back(zone_id);
                }
            }
        }

        const auto edge_limit =
            std::min<dp::u64>(route_plan.traversed_edge_zone_ids.size(), start_node_index + horizon);
        for (dp::u64 i = start_node_index; i < edge_limit; ++i) {
            for (const auto &zone_id : route_plan.traversed_edge_zone_ids[i]) {
                if (seen_zone_ids.insert(zone_id).second) {
                    zone_ids.push_back(zone_id);
                }
            }
        }

        return zone_ids;
    }

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
        const auto route_shape = validate_route_plan_shape(route_plan);
        if (route_shape.is_err()) {
            return request;
        }

        const dp::u64 start_node_index = route_progress_index(state);
        request.requested_at_tick = state.updated_at_tick;
        request.window.start_tick = state.updated_at_tick;
        if (start_node_index >= route_plan.traversed_node_ids.size()) {
            request.window.end_tick = state.updated_at_tick;
            return request;
        }

        const auto available_nodes = route_plan.traversed_node_ids.size() - start_node_index;
        const auto available_edges = route_plan.traversed_edge_ids.size() > start_node_index
                                         ? route_plan.traversed_edge_ids.size() - start_node_index
                                         : 0;
        const auto node_limit = std::min<dp::u64>(available_nodes, state.horizon + 1);
        const auto edge_limit = std::min<dp::u64>(available_edges, state.horizon);
        const auto zone_targets = route_zone_targets_from_progress(route_plan, start_node_index, state.horizon);

        for (const auto &zone_id : zone_targets) {
            request.targets.push_back(ClaimTarget{ClaimTargetKind::Zone, zone_id});
        }
        for (dp::u64 i = 0; i < edge_limit; ++i) {
            request.targets.push_back(
                ClaimTarget{ClaimTargetKind::Edge, route_plan.traversed_edge_ids[start_node_index + i]});
        }
        for (dp::u64 i = 0; i < node_limit; ++i) {
            request.targets.push_back(
                ClaimTarget{ClaimTargetKind::Node, route_plan.traversed_node_ids[start_node_index + i]});
        }

        if (start_node_index < route_plan.steps.size()) {
            const auto traversed_cost =
                start_node_index == 0 ? 0.0 : route_plan.steps[start_node_index].cumulative_cost;
            const auto remaining_cost = std::max<dp::f64>(0.0, route_plan.total_cost - traversed_cost);
            request.window.end_tick = state.updated_at_tick + static_cast<dp::u64>(std::ceil(remaining_cost));
        } else {
            request.window.end_tick = state.updated_at_tick;
        }

        return request;
    }

    inline dp::u64 release_targets_behind_progress(RobotState &state, ClaimManager &claim_manager) {
        if (!state.route_plan.has_value() || !state.current_node_id.has_value()) {
            return 0;
        }

        const auto &route_plan = state.route_plan.value();
        if (validate_route_plan_shape(route_plan).is_err()) {
            return 0;
        }
        const auto current_node_it = std::find(route_plan.traversed_node_ids.begin(),
                                               route_plan.traversed_node_ids.end(), *state.current_node_id);
        if (current_node_it == route_plan.traversed_node_ids.end()) {
            return 0;
        }

        const auto current_index =
            static_cast<dp::u64>(std::distance(route_plan.traversed_node_ids.begin(), current_node_it));
        std::unordered_set<zoneout::UUID, zoneout::UUIDHash> remaining_node_ids;
        std::unordered_set<zoneout::UUID, zoneout::UUIDHash> remaining_edge_ids;
        std::unordered_set<zoneout::UUID, zoneout::UUIDHash> remaining_zone_ids;

        for (dp::u64 i = current_index; i < route_plan.traversed_node_ids.size(); ++i) {
            remaining_node_ids.insert(route_plan.traversed_node_ids[i]);
        }
        for (dp::u64 i = current_index; i < route_plan.traversed_edge_ids.size(); ++i) {
            remaining_edge_ids.insert(route_plan.traversed_edge_ids[i]);
        }
        for (dp::u64 i = current_index; i < route_plan.traversed_node_zone_ids.size(); ++i) {
            for (const auto &zone_id : route_plan.traversed_node_zone_ids[i]) {
                remaining_zone_ids.insert(zone_id);
            }
        }
        for (dp::u64 i = current_index; i < route_plan.traversed_edge_zone_ids.size(); ++i) {
            for (const auto &zone_id : route_plan.traversed_edge_zone_ids[i]) {
                remaining_zone_ids.insert(zone_id);
            }
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
                    (target.kind == ClaimTargetKind::Zone && remaining_zone_ids.count(target.resource_id) > 0)) {
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
            if (!schedule_window.has_value()) {
                continue;
            }

            bool matches_window = false;
            std::string raw = schedule_window.value().c_str();
            std::size_t token_start = 0;
            while (token_start <= raw.size()) {
                const auto token_end = raw.find(',', token_start);
                auto token = raw.substr(token_start,
                                        token_end == std::string::npos ? std::string::npos : token_end - token_start);
                const auto first = token.find_first_not_of(" \t");
                const auto last = token.find_last_not_of(" \t");
                token = first == std::string::npos ? "" : token.substr(first, last - first + 1);
                if (token == active_window) {
                    matches_window = true;
                    break;
                }
                if (token_end == std::string::npos) {
                    break;
                }
                token_start = token_end + 1;
            }

            if (!matches_window) {
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
        bool self_is_emergency = false;
        bool other_is_emergency = false;
        RobotProgressState self_state = RobotProgressState::Idle;
        RobotProgressState other_state = RobotProgressState::Idle;
        dp::u64 self_wait_ticks = 0;
        dp::u64 other_wait_ticks = 0;
        dp::u64 self_remaining_steps = 0;
        dp::u64 other_remaining_steps = 0;
    };

    inline ArbitrationDecision arbitrate_right_of_way(const ArbitrationContext &context) {
        if (context.self_is_emergency && !context.other_is_emergency) {
            return ArbitrationDecision::Proceed;
        }
        if (context.other_is_emergency && !context.self_is_emergency) {
            return ArbitrationDecision::Yield;
        }
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
        if (context.self_state == RobotProgressState::Blocked && context.other_state != RobotProgressState::Blocked) {
            return ArbitrationDecision::Yield;
        }
        if (context.other_state == RobotProgressState::Blocked && context.self_state != RobotProgressState::Blocked) {
            return ArbitrationDecision::Proceed;
        }
        if (context.self_state == RobotProgressState::FollowingRoute &&
            context.other_state == RobotProgressState::Waiting) {
            return ArbitrationDecision::Proceed;
        }
        if (context.self_state == RobotProgressState::Waiting &&
            context.other_state == RobotProgressState::FollowingRoute) {
            return ArbitrationDecision::Yield;
        }
        if (context.self_state == RobotProgressState::Waiting && context.other_state == RobotProgressState::Waiting) {
            if (context.self_wait_ticks > context.other_wait_ticks) {
                return ArbitrationDecision::Proceed;
            }
            if (context.self_wait_ticks < context.other_wait_ticks) {
                return ArbitrationDecision::Yield;
            }
        }
        if (context.self_remaining_steps > 0 || context.other_remaining_steps > 0) {
            if (context.self_remaining_steps < context.other_remaining_steps) {
                return ArbitrationDecision::Proceed;
            }
            if (context.self_remaining_steps > context.other_remaining_steps) {
                return ArbitrationDecision::Yield;
            }
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
                    claim_manager_.remove_requests_for_robot(robot_id);
                    claim_manager_.release_leases_for_robot(robot_id, it->updated_at_tick);
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
                    state->needs_replan = false;
                    state->wait_ticks = 0;
                }
            } else if (current_edge_id.has_value()) {
                state->progress_state = RobotProgressState::FollowingRoute;
                state->needs_replan = false;
                state->wait_ticks = 0;
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
            state->scheduled_start_tick = updated_at_tick;
            state->reserved_until_tick =
                updated_at_tick + static_cast<dp::u64>(std::max<dp::f64>(0.0, std::ceil(route_plan.total_cost)));
            state->wait_ticks = 0;
            state->needs_replan = false;
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

            const auto released = release_targets_behind_progress(*state, claim_manager_);
            if (released > 0 && state->progress_state == RobotProgressState::FollowingRoute) {
                state->last_claim_tick = state->updated_at_tick;
            }
            return released;
        }
        [[nodiscard]] ScheduleDecision schedule_robot_route(RobotId robot_id, ClaimId claim_id, dp::u64 start_tick,
                                                            dp::f64 ticks_per_cost_unit = 1.0,
                                                            ClaimAccessMode access_mode = ClaimAccessMode::Exclusive) {
            const auto *state = find_robot_state(robot_id);
            if (state == nullptr || !state->route_plan.has_value() || index_ == nullptr) {
                ScheduleDecision decision{};
                decision.kind = ScheduleDecisionKind::Replan;
                decision.start_tick = start_tick;
                decision.diagnostics.push_back("robot does not have a schedulable route");
                return decision;
            }

            auto request = claim_request_from_route(claim_id, robot_id, state->mission_id, *state->route_plan,
                                                    start_tick, ticks_per_cost_unit, access_mode);
            const auto decision = schedule_route_request(*index_, claim_manager_, request, *state->route_plan,
                                                         start_tick, ticks_per_cost_unit);
            if (auto *mutable_state = find_robot_state(robot_id); mutable_state != nullptr) {
                mutable_state->reserved_until_tick = request.window.end_tick;
                apply_schedule_decision(*mutable_state, decision, start_tick);
            }
            return decision;
        }
        [[nodiscard]] dp::u64 refresh_robot_leases(RobotId robot_id, dp::u64 refreshed_at_tick,
                                                   dp::u64 extension_ticks = 0) {
            auto *state = find_robot_state(robot_id);
            if (state == nullptr) {
                return 0;
            }

            dp::u64 refreshed = 0;
            for (const auto lease_id : state->active_lease_ids) {
                const auto *lease = claim_manager_.find_lease(lease_id);
                if (lease == nullptr) {
                    continue;
                }
                const auto new_expiry = lease->expires_at_tick.has_value()
                                            ? dp::Optional<dp::u64>{lease->expires_at_tick.value() + extension_ticks}
                                            : dp::nullopt;
                if (claim_manager_.refresh_lease(lease_id, refreshed_at_tick, new_expiry)) {
                    ++refreshed;
                }
            }
            state->last_claim_tick = refreshed_at_tick;
            return refreshed;
        }
        [[nodiscard]] dp::u64 revoke_robot_leases(RobotId robot_id, dp::String reason, dp::u64 revoked_at_tick) {
            auto *state = find_robot_state(robot_id);
            if (state == nullptr) {
                return 0;
            }

            dp::u64 revoked = 0;
            for (const auto lease_id : state->active_lease_ids) {
                if (claim_manager_.revoke_lease(lease_id, reason, revoked_at_tick)) {
                    ++revoked;
                }
            }
            state->active_lease_ids.clear();
            state->progress_state = RobotProgressState::Replanning;
            state->hold_reason = reason;
            state->needs_replan = revoked > 0;
            state->updated_at_tick = revoked_at_tick;
            return revoked;
        }
        [[nodiscard]] bool handle_missed_schedule_slot(RobotId robot_id, dp::u64 current_tick,
                                                       dp::u64 grace_ticks = 0) {
            auto *state = find_robot_state(robot_id);
            if (state == nullptr || !robot_missed_schedule_slot(*state, current_tick, grace_ticks)) {
                return false;
            }

            state->progress_state = RobotProgressState::Replanning;
            state->hold_reason = "missed_reservation_window";
            state->needs_replan = true;
            state->updated_at_tick = current_tick;
            return true;
        }

      private:
        const WorkspaceIndex *index_ = nullptr;
        ClaimManager claim_manager_{};
        dp::Vector<RobotState> robot_states_{};
    };

} // namespace timenav
