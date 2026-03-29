#pragma once

#include <algorithm>
#include <limits>
#include <string>

#include "timenav/claim.hpp"
#include "timenav/workspace_index.hpp"
#include "timenav/zone_policy.hpp"

namespace timenav {

    class ClaimManager {
      public:
        ClaimManager() = default;
        explicit ClaimManager(const WorkspaceIndex &index) : index_(&index) {}

        [[nodiscard]] bool empty() const noexcept { return request_count() == 0 && lease_count() == 0; }
        [[nodiscard]] const WorkspaceIndex *index() const noexcept { return index_; }
        [[nodiscard]] bool has_index() const noexcept { return index_ != nullptr; }
        [[nodiscard]] dp::u64 request_count() const noexcept { return active_requests_.size(); }
        [[nodiscard]] dp::u64 lease_count() const noexcept { return active_leases_.size(); }
        [[nodiscard]] const dp::Vector<ClaimRequest> &requests() const noexcept { return active_requests_; }
        [[nodiscard]] const dp::Vector<Lease> &leases() const noexcept { return active_leases_; }
        [[nodiscard]] const dp::Vector<Lease> &released_leases() const noexcept { return released_leases_; }
        void bind_index(const WorkspaceIndex &index) noexcept { index_ = &index; }
        void clear() {
            active_requests_.clear();
            active_leases_.clear();
            released_leases_.clear();
        }

        void add_request(const ClaimRequest &request) { upsert_request(request); }
        void add_lease(const Lease &lease) { upsert_lease(lease); }
        void upsert_request(const ClaimRequest &request) {
            for (auto &active_request : active_requests_) {
                if (active_request.id == request.id) {
                    active_request = request;
                    return;
                }
            }

            active_requests_.push_back(request);
        }
        [[nodiscard]] bool remove_request(ClaimId id) {
            for (auto it = active_requests_.begin(); it != active_requests_.end(); ++it) {
                if (it->id == id) {
                    active_requests_.erase(it);
                    return true;
                }
            }

            return false;
        }
        [[nodiscard]] dp::u64 remove_requests_for_robot(RobotId robot_id) {
            dp::u64 removed = 0;
            auto it = active_requests_.begin();
            while (it != active_requests_.end()) {
                if (it->robot_id != robot_id) {
                    ++it;
                    continue;
                }
                it = active_requests_.erase(it);
                ++removed;
            }

            return removed;
        }
        void upsert_lease(const Lease &lease) {
            for (auto &active_lease : active_leases_) {
                if (active_lease.id == lease.id) {
                    active_lease = lease;
                    return;
                }
            }

            active_leases_.push_back(lease);
        }
        [[nodiscard]] bool remove_lease(LeaseId id) {
            for (auto it = active_leases_.begin(); it != active_leases_.end(); ++it) {
                if (it->id == id) {
                    active_leases_.erase(it);
                    return true;
                }
            }

            return false;
        }
        [[nodiscard]] dp::u64 release_leases_for_robot(RobotId robot_id,
                                                       dp::Optional<dp::u64> released_at_tick = dp::nullopt) {
            dp::u64 released = 0;
            auto it = active_leases_.begin();
            while (it != active_leases_.end()) {
                if (it->robot_id != robot_id) {
                    ++it;
                    continue;
                }

                Lease archived = *it;
                archived.active = false;
                archived.released_at_tick = released_at_tick;
                released_leases_.push_back(archived);
                it = active_leases_.erase(it);
                ++released;
            }

            return released;
        }
        [[nodiscard]] bool release_lease(LeaseId id, dp::Optional<dp::u64> released_at_tick = dp::nullopt) {
            for (auto it = active_leases_.begin(); it != active_leases_.end(); ++it) {
                if (it->id == id) {
                    Lease released = *it;
                    released.active = false;
                    released.released_at_tick = released_at_tick;
                    released_leases_.push_back(released);
                    active_leases_.erase(it);
                    return true;
                }
            }

            return false;
        }
        [[nodiscard]] dp::u64 expire_leases(dp::u64 current_tick) {
            dp::u64 expired = 0;
            auto it = active_leases_.begin();
            while (it != active_leases_.end()) {
                if (!(it->expires_at_tick.has_value() && it->expires_at_tick.value() <= current_tick)) {
                    ++it;
                    continue;
                }

                Lease released = *it;
                released.active = false;
                released.released_at_tick = current_tick;
                released_leases_.push_back(released);
                it = active_leases_.erase(it);
                ++expired;
            }

            return expired;
        }

        [[nodiscard]] const ClaimRequest *find_request(ClaimId id) const noexcept {
            for (const auto &request : active_requests_) {
                if (request.id == id) {
                    return &request;
                }
            }

            return nullptr;
        }
        [[nodiscard]] bool has_request(ClaimId id) const noexcept { return find_request(id) != nullptr; }

        [[nodiscard]] const Lease *find_lease(LeaseId id) const noexcept {
            for (const auto &lease : active_leases_) {
                if (lease.id == id) {
                    return &lease;
                }
            }

            return nullptr;
        }
        [[nodiscard]] bool has_lease(LeaseId id) const noexcept { return find_lease(id) != nullptr; }
        [[nodiscard]] const Lease *find_released_lease(LeaseId id) const noexcept {
            for (const auto &lease : released_leases_) {
                if (lease.id == id) {
                    return &lease;
                }
            }

            return nullptr;
        }
        [[nodiscard]] dp::Vector<const Lease *> leases_for_robot(RobotId robot_id) const {
            dp::Vector<const Lease *> leases;
            for (const auto &lease : active_leases_) {
                if (lease.robot_id == robot_id) {
                    leases.push_back(&lease);
                }
            }

            return leases;
        }
        [[nodiscard]] const Lease *lease_for_claim(ClaimId claim_id) const noexcept {
            for (const auto &lease : active_leases_) {
                if (lease.claim_id == claim_id) {
                    return &lease;
                }
            }

            return nullptr;
        }

        [[nodiscard]] static bool zone_claims_compatible(const ClaimRequest &lhs, const ClaimRequest &rhs) noexcept {
            return target_kind_compatible(lhs, rhs, ClaimTargetKind::Zone);
        }

        [[nodiscard]] static bool node_claims_compatible(const ClaimRequest &lhs, const ClaimRequest &rhs) noexcept {
            return target_kind_compatible(lhs, rhs, ClaimTargetKind::Node);
        }

        [[nodiscard]] static bool edge_claims_compatible(const ClaimRequest &lhs, const ClaimRequest &rhs) noexcept {
            return target_kind_compatible(lhs, rhs, ClaimTargetKind::Edge);
        }

        [[nodiscard]] static bool claims_compatible(const ClaimRequest &lhs, const ClaimRequest &rhs) noexcept {
            return zone_claims_compatible(lhs, rhs) && node_claims_compatible(lhs, rhs) &&
                   edge_claims_compatible(lhs, rhs);
        }

        [[nodiscard]] static bool claims_compatible(const ClaimRequest &request, const Lease &lease) noexcept {
            ClaimRequest lease_view{};
            lease_view.access_mode = lease.access_mode;
            lease_view.window.end_tick = lease.expires_at_tick;
            lease_view.targets = lease.targets;
            return claims_compatible(request, lease_view);
        }

        [[nodiscard]] ClaimEvaluation evaluate_request(const ClaimRequest &request) const {
            if (request.targets.empty()) {
                return ClaimEvaluation{ClaimDecision::Deny,
                                       dp::String{"claim request does not contain any targets"},
                                       dp::nullopt,
                                       dp::nullopt,
                                       {},
                                       dp::nullopt,
                                       {dp::String{"request has no claim targets"}}};
            }

            if (const auto invalid_target = first_invalid_target(request); invalid_target.has_value()) {
                return ClaimEvaluation{ClaimDecision::Deny,
                                       dp::String{"claim request references a missing workspace resource"},
                                       dp::nullopt,
                                       dp::nullopt,
                                       {invalid_target.value()},
                                       invalid_target,
                                       {dp::String{"request target does not exist in current workspace index"}}};
            }

            for (const auto &active_request : active_requests_) {
                if (active_request.id == request.id) {
                    continue;
                }

                if (!claims_compatible_for_current_index(request, active_request)) {
                    const auto conflicts = conflicting_targets(request, active_request);
                    return ClaimEvaluation{ClaimDecision::Deny,
                                           describe_conflict_reason(dp::String{"active request"}, conflicts),
                                           active_request.id,
                                           dp::nullopt,
                                           conflicts,
                                           conflicts.empty() ? dp::nullopt
                                                             : dp::Optional<ClaimTarget>{conflicts.front()},
                                           build_conflict_diagnostics(dp::String{"active request"}, conflicts)};
                }
            }

            for (const auto &active_lease : active_leases_) {
                if (!active_lease.active) {
                    continue;
                }

                if (!claims_compatible_for_current_index(request, active_lease)) {
                    const auto conflicts = conflicting_targets(request, active_lease);
                    return ClaimEvaluation{ClaimDecision::Deny,
                                           describe_conflict_reason(dp::String{"granted lease"}, conflicts),
                                           dp::nullopt,
                                           active_lease.id,
                                           conflicts,
                                           conflicts.empty() ? dp::nullopt
                                                             : dp::Optional<ClaimTarget>{conflicts.front()},
                                           build_conflict_diagnostics(dp::String{"granted lease"}, conflicts)};
                }
            }

            if (const auto saturated = first_capacity_violation(request); saturated.has_value()) {
                ClaimEvaluation evaluation{};
                evaluation.decision = ClaimDecision::Deny;
                evaluation.reason = saturated->reason;
                evaluation.conflicting_claim_id = saturated->conflicting_claim_id;
                evaluation.conflicting_lease_id = saturated->conflicting_lease_id;
                evaluation.conflicting_targets = {saturated->target};
                evaluation.blocking_target = saturated->target;
                evaluation.diagnostics = saturated->diagnostics;
                return evaluation;
            }

            if (const auto saturated = first_edge_capacity_violation(request); saturated.has_value()) {
                ClaimEvaluation evaluation{};
                evaluation.decision = ClaimDecision::Deny;
                evaluation.reason = saturated->reason;
                evaluation.conflicting_claim_id = saturated->conflicting_claim_id;
                evaluation.conflicting_lease_id = saturated->conflicting_lease_id;
                evaluation.conflicting_targets = {saturated->target};
                evaluation.blocking_target = saturated->target;
                evaluation.diagnostics = saturated->diagnostics;
                return evaluation;
            }

            return ClaimEvaluation{ClaimDecision::Grant,
                                   dp::String{"claim is compatible with current state"},
                                   dp::nullopt,
                                   dp::nullopt,
                                   {},
                                   dp::nullopt,
                                   {dp::String{"request passed conflict and capacity checks"}}};
        }

      private:
        struct CapacityViolation {
            ClaimTarget target;
            dp::String reason;
            dp::Optional<ClaimId> conflicting_claim_id;
            dp::Optional<LeaseId> conflicting_lease_id;
            dp::Vector<dp::String> diagnostics;
        };

        [[nodiscard]] dp::Optional<CapacityViolation> first_capacity_violation(const ClaimRequest &request) const {
            if (index_ == nullptr || request.access_mode != ClaimAccessMode::Shared) {
                return dp::nullopt;
            }

            for (const auto &target : request.targets) {
                if (target.kind != ClaimTargetKind::Zone) {
                    continue;
                }

                const auto *zone = index_->zone(target.resource_id);
                if (zone == nullptr) {
                    continue;
                }

                const auto policy = parse_zone_policy(zone->properties());
                if (!policy.capacity_is_explicit || policy.capacity <= 1) {
                    continue;
                }

                dp::u32 occupant_count = 1;
                dp::Optional<ClaimId> blocking_claim_id;
                dp::Optional<LeaseId> blocking_lease_id;

                for (const auto &active_request : active_requests_) {
                    if (active_request.id == request.id || active_request.access_mode != ClaimAccessMode::Shared ||
                        !claim_windows_overlap(request.window, active_request.window) ||
                        !request_overlaps_zone(active_request, target.resource_id)) {
                        continue;
                    }
                    ++occupant_count;
                    if (!blocking_claim_id.has_value()) {
                        blocking_claim_id = active_request.id;
                    }
                }

                for (const auto &active_lease : active_leases_) {
                    if (!active_lease.active || active_lease.access_mode != ClaimAccessMode::Shared ||
                        !claim_windows_overlap(request.window, lease_window(active_lease)) ||
                        !lease_overlaps_zone(active_lease, target.resource_id)) {
                        continue;
                    }
                    ++occupant_count;
                    if (!blocking_lease_id.has_value()) {
                        blocking_lease_id = active_lease.id;
                    }
                }

                if (occupant_count > policy.capacity) {
                    CapacityViolation violation{};
                    violation.target = target;
                    violation.reason = "shared zone capacity exceeded";
                    violation.conflicting_claim_id = blocking_claim_id;
                    violation.conflicting_lease_id = blocking_lease_id;
                    violation.diagnostics.push_back(dp::String{"zone capacity limit reached"});
                    violation.diagnostics.push_back(dp::String{"configured capacity="} +
                                                    dp::String{std::to_string(policy.capacity)});
                    violation.diagnostics.push_back(dp::String{"observed occupancy="} +
                                                    dp::String{std::to_string(occupant_count)});
                    return violation;
                }
            }

            return dp::nullopt;
        }
        [[nodiscard]] dp::Optional<CapacityViolation> first_edge_capacity_violation(const ClaimRequest &request) const {
            if (index_ == nullptr || request.access_mode != ClaimAccessMode::Shared) {
                return dp::nullopt;
            }

            for (const auto &target : request.targets) {
                if (target.kind != ClaimTargetKind::Edge) {
                    continue;
                }

                const auto *edge = index_->edge(target.resource_id);
                if (edge == nullptr) {
                    continue;
                }

                const auto semantics = parse_edge_traffic_semantics(edge->properties);
                if (!semantics.capacity_is_explicit || !semantics.capacity.has_value() ||
                    semantics.capacity.value() <= 1) {
                    continue;
                }

                dp::u32 occupant_count = 1;
                dp::Optional<ClaimId> blocking_claim_id;
                dp::Optional<LeaseId> blocking_lease_id;

                for (const auto &active_request : active_requests_) {
                    if (active_request.id == request.id || active_request.access_mode != ClaimAccessMode::Shared ||
                        !claim_windows_overlap(request.window, active_request.window) ||
                        !request_contains_target(active_request, target)) {
                        continue;
                    }
                    ++occupant_count;
                    if (!blocking_claim_id.has_value()) {
                        blocking_claim_id = active_request.id;
                    }
                }

                for (const auto &active_lease : active_leases_) {
                    if (!active_lease.active || active_lease.access_mode != ClaimAccessMode::Shared ||
                        !claim_windows_overlap(request.window, lease_window(active_lease)) ||
                        !lease_contains_target(active_lease, target)) {
                        continue;
                    }
                    ++occupant_count;
                    if (!blocking_lease_id.has_value()) {
                        blocking_lease_id = active_lease.id;
                    }
                }

                if (occupant_count > semantics.capacity.value()) {
                    CapacityViolation violation{};
                    violation.target = target;
                    violation.reason = "shared edge capacity exceeded";
                    violation.conflicting_claim_id = blocking_claim_id;
                    violation.conflicting_lease_id = blocking_lease_id;
                    violation.diagnostics.push_back(dp::String{"edge capacity limit reached"});
                    violation.diagnostics.push_back(dp::String{"configured capacity="} +
                                                    dp::String{std::to_string(semantics.capacity.value())});
                    violation.diagnostics.push_back(dp::String{"observed occupancy="} +
                                                    dp::String{std::to_string(occupant_count)});
                    return violation;
                }
            }

            return dp::nullopt;
        }
        [[nodiscard]] dp::Optional<ClaimTarget> first_invalid_target(const ClaimRequest &request) const noexcept {
            if (index_ == nullptr) {
                return dp::nullopt;
            }

            for (const auto &target : request.targets) {
                if ((target.kind == ClaimTargetKind::Zone && index_->zone(target.resource_id) == nullptr) ||
                    (target.kind == ClaimTargetKind::Node && index_->node(target.resource_id) == nullptr) ||
                    (target.kind == ClaimTargetKind::Edge && index_->edge(target.resource_id) == nullptr)) {
                    return target;
                }
            }

            return dp::nullopt;
        }
        [[nodiscard]] static dp::Vector<ClaimTarget> conflicting_targets(const ClaimRequest &lhs,
                                                                         const ClaimRequest &rhs) {
            dp::Vector<ClaimTarget> conflicts;
            for (const auto &lhs_target : lhs.targets) {
                for (const auto &rhs_target : rhs.targets) {
                    if (lhs_target.kind == rhs_target.kind && lhs_target.resource_id == rhs_target.resource_id) {
                        conflicts.push_back(lhs_target);
                    }
                }
            }
            return conflicts;
        }
        [[nodiscard]] static dp::Vector<ClaimTarget> conflicting_targets(const ClaimRequest &request,
                                                                         const Lease &lease) {
            ClaimRequest lease_view{};
            lease_view.targets = lease.targets;
            return conflicting_targets(request, lease_view);
        }
        [[nodiscard]] static dp::String target_kind_name(ClaimTargetKind kind) {
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
        [[nodiscard]] static dp::String describe_conflict_reason(const dp::String &source,
                                                                 const dp::Vector<ClaimTarget> &conflicts) {
            if (conflicts.empty()) {
                return dp::String{"conflicts with "} + source;
            }

            return dp::String{"conflicts with "} + source + dp::String{" on "} +
                   target_kind_name(conflicts.front().kind) + dp::String{" target"};
        }
        [[nodiscard]] static dp::Vector<dp::String>
        build_conflict_diagnostics(const dp::String &source, const dp::Vector<ClaimTarget> &conflicts) {
            dp::Vector<dp::String> diagnostics;
            diagnostics.push_back(dp::String{"collision detected with "} + source);
            if (!conflicts.empty()) {
                diagnostics.push_back(dp::String{"blocking "} + target_kind_name(conflicts.front().kind) +
                                      dp::String{" id="} + dp::String{conflicts.front().resource_id.toString()});
            }
            return diagnostics;
        }
        [[nodiscard]] static ClaimWindow lease_window(const Lease &lease) {
            ClaimWindow window{};
            window.start_tick = lease.granted_at_tick;
            window.end_tick = lease.expires_at_tick;
            return window;
        }
        [[nodiscard]] static bool request_contains_target(const ClaimRequest &request, const ClaimTarget &target) {
            for (const auto &candidate : request.targets) {
                if (candidate.kind == target.kind && candidate.resource_id == target.resource_id) {
                    return true;
                }
            }

            return false;
        }
        [[nodiscard]] static bool lease_contains_target(const Lease &lease, const ClaimTarget &target) {
            for (const auto &candidate : lease.targets) {
                if (candidate.kind == target.kind && candidate.resource_id == target.resource_id) {
                    return true;
                }
            }

            return false;
        }
        [[nodiscard]] bool request_overlaps_zone(const ClaimRequest &request, const zoneout::UUID &zone_id) const {
            for (const auto &target : request.targets) {
                if (target.kind == ClaimTargetKind::Zone && zones_overlap(target.resource_id, zone_id)) {
                    return true;
                }
            }

            return false;
        }
        [[nodiscard]] bool lease_overlaps_zone(const Lease &lease, const zoneout::UUID &zone_id) const {
            for (const auto &target : lease.targets) {
                if (target.kind == ClaimTargetKind::Zone && zones_overlap(target.resource_id, zone_id)) {
                    return true;
                }
            }

            return false;
        }
        [[nodiscard]] bool claims_compatible_for_current_index(const ClaimRequest &lhs, const ClaimRequest &rhs) const {
            if (index_ == nullptr) {
                return claims_compatible(lhs, rhs);
            }

            return zone_claims_compatible_with_index(lhs, rhs) && node_claims_compatible_with_index(lhs, rhs) &&
                   edge_claims_compatible_with_index(lhs, rhs);
        }
        [[nodiscard]] bool claims_compatible_for_current_index(const ClaimRequest &request, const Lease &lease) const {
            ClaimRequest lease_view{};
            lease_view.access_mode = lease.access_mode;
            lease_view.window.end_tick = lease.expires_at_tick;
            lease_view.targets = lease.targets;

            return claims_compatible_for_current_index(request, lease_view);
        }
        [[nodiscard]] bool zone_claims_compatible_with_index(const ClaimRequest &lhs, const ClaimRequest &rhs) const {
            for (const auto &lhs_target : lhs.targets) {
                if (lhs_target.kind != ClaimTargetKind::Zone) {
                    continue;
                }

                for (const auto &rhs_target : rhs.targets) {
                    if (rhs_target.kind != ClaimTargetKind::Zone) {
                        continue;
                    }

                    if (!zones_overlap(lhs_target.resource_id, rhs_target.resource_id)) {
                        continue;
                    }
                    if (!claim_windows_overlap(lhs.window, rhs.window)) {
                        continue;
                    }

                    const auto policy = overlapping_zone_policy(lhs_target.resource_id, rhs_target.resource_id);
                    const bool both_shared =
                        lhs.access_mode == ClaimAccessMode::Shared && rhs.access_mode == ClaimAccessMode::Shared;
                    if (both_shared && policy.capacity > 1) {
                        continue;
                    }

                    return false;
                }
            }

            return true;
        }
        [[nodiscard]] bool node_claims_compatible_with_index(const ClaimRequest &lhs, const ClaimRequest &rhs) const {
            return spatial_claims_compatible_with_index(lhs, rhs, ClaimTargetKind::Node);
        }
        [[nodiscard]] bool edge_claims_compatible_with_index(const ClaimRequest &lhs, const ClaimRequest &rhs) const {
            return spatial_claims_compatible_with_index(lhs, rhs, ClaimTargetKind::Edge);
        }
        [[nodiscard]] bool zones_overlap(const zoneout::UUID &lhs_zone_id, const zoneout::UUID &rhs_zone_id) const {
            if (lhs_zone_id == rhs_zone_id) {
                return true;
            }

            for (const auto *ancestor : index_->ancestor_zones(lhs_zone_id)) {
                if (ancestor != nullptr && ancestor->id() == rhs_zone_id) {
                    return true;
                }
            }
            for (const auto *ancestor : index_->ancestor_zones(rhs_zone_id)) {
                if (ancestor != nullptr && ancestor->id() == lhs_zone_id) {
                    return true;
                }
            }

            return false;
        }
        [[nodiscard]] ZonePolicy overlapping_zone_policy(const zoneout::UUID &lhs_zone_id,
                                                         const zoneout::UUID &rhs_zone_id) const {
            if (const auto *lhs_zone = index_->zone(lhs_zone_id); lhs_zone != nullptr) {
                if (lhs_zone_id == rhs_zone_id) {
                    return parse_zone_policy(lhs_zone->properties());
                }
                if (const auto *parent = index_->parent_zone(lhs_zone_id);
                    parent != nullptr && parent->id() == rhs_zone_id) {
                    return parse_zone_policy(lhs_zone->properties());
                }
            }

            if (const auto *rhs_zone = index_->zone(rhs_zone_id); rhs_zone != nullptr) {
                if (const auto *parent = index_->parent_zone(rhs_zone_id);
                    parent != nullptr && parent->id() == lhs_zone_id) {
                    return parse_zone_policy(rhs_zone->properties());
                }
            }

            return ZonePolicy{};
        }
        [[nodiscard]] static bool claim_windows_overlap(const ClaimWindow &lhs, const ClaimWindow &rhs) noexcept {
            const auto lhs_start = lhs.start_tick.value_or(0);
            const auto rhs_start = rhs.start_tick.value_or(0);
            const auto lhs_end = lhs.end_tick.value_or(std::numeric_limits<dp::u64>::max());
            const auto rhs_end = rhs.end_tick.value_or(std::numeric_limits<dp::u64>::max());
            return lhs_start <= rhs_end && rhs_start <= lhs_end;
        }
        [[nodiscard]] bool spatial_claims_compatible_with_index(const ClaimRequest &lhs, const ClaimRequest &rhs,
                                                                ClaimTargetKind kind) const {
            for (const auto &lhs_target : lhs.targets) {
                if (lhs_target.kind != kind) {
                    continue;
                }

                for (const auto &rhs_target : rhs.targets) {
                    if (rhs_target.kind != kind) {
                        continue;
                    }
                    if (!claim_windows_overlap(lhs.window, rhs.window)) {
                        continue;
                    }

                    if (lhs_target.resource_id == rhs_target.resource_id) {
                        if (lhs.access_mode == ClaimAccessMode::Exclusive ||
                            rhs.access_mode == ClaimAccessMode::Exclusive) {
                            return false;
                        }
                        continue;
                    }

                    if (shared_constrained_zone(kind, lhs_target.resource_id, rhs_target.resource_id)) {
                        return false;
                    }
                }
            }

            return true;
        }
        [[nodiscard]] bool shared_constrained_zone(ClaimTargetKind kind, const zoneout::UUID &lhs_resource_id,
                                                   const zoneout::UUID &rhs_resource_id) const {
            const auto lhs_zones = kind == ClaimTargetKind::Node ? index_->zones_of_node(lhs_resource_id)
                                                                 : index_->zones_of_edge(lhs_resource_id);
            const auto rhs_zones = kind == ClaimTargetKind::Node ? index_->zones_of_node(rhs_resource_id)
                                                                 : index_->zones_of_edge(rhs_resource_id);

            for (const auto *lhs_zone : lhs_zones) {
                if (lhs_zone == nullptr) {
                    continue;
                }
                const auto lhs_policy = parse_zone_policy(lhs_zone->properties());

                for (const auto *rhs_zone : rhs_zones) {
                    if (rhs_zone == nullptr || lhs_zone->id() != rhs_zone->id()) {
                        continue;
                    }

                    const auto rhs_policy = parse_zone_policy(rhs_zone->properties());
                    const auto effective_capacity = std::min(lhs_policy.capacity, rhs_policy.capacity);
                    const bool explicitly_constrained =
                        lhs_policy.capacity_is_explicit || rhs_policy.capacity_is_explicit;
                    if (lhs_policy.kind == ZonePolicyKind::ExclusiveAccess ||
                        rhs_policy.kind == ZonePolicyKind::ExclusiveAccess ||
                        (explicitly_constrained && effective_capacity <= 1)) {
                        return true;
                    }
                }
            }

            return false;
        }
        [[nodiscard]] static bool target_kind_compatible(const ClaimRequest &lhs, const ClaimRequest &rhs,
                                                         ClaimTargetKind kind) noexcept {
            for (const auto &lhs_target : lhs.targets) {
                if (lhs_target.kind != kind) {
                    continue;
                }

                for (const auto &rhs_target : rhs.targets) {
                    if (rhs_target.kind != kind || lhs_target.resource_id != rhs_target.resource_id) {
                        continue;
                    }

                    if (lhs.access_mode == ClaimAccessMode::Exclusive ||
                        rhs.access_mode == ClaimAccessMode::Exclusive) {
                        return false;
                    }
                }
            }

            return true;
        }
        const WorkspaceIndex *index_ = nullptr;
        dp::Vector<ClaimRequest> active_requests_{};
        dp::Vector<Lease> active_leases_{};
        dp::Vector<Lease> released_leases_{};
    };

} // namespace timenav
