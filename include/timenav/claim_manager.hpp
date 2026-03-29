#pragma once

#include <algorithm>

#include "timenav/claim.hpp"
#include "timenav/workspace_index.hpp"

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
        void bind_index(const WorkspaceIndex &index) noexcept { index_ = &index; }
        void clear() {
            active_requests_.clear();
            active_leases_.clear();
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
        [[nodiscard]] bool release_lease(LeaseId id) {
            for (auto it = active_leases_.begin(); it != active_leases_.end(); ++it) {
                if (it->id == id) {
                    active_leases_.erase(it);
                    return true;
                }
            }

            return false;
        }
        [[nodiscard]] dp::u64 expire_leases(dp::u64 current_tick) {
            const auto previous_size = active_leases_.size();
            active_leases_.erase(std::remove_if(active_leases_.begin(), active_leases_.end(),
                                                [current_tick](const Lease &lease) {
                                                    return lease.expires_at_tick.has_value() &&
                                                           lease.expires_at_tick.value() <= current_tick;
                                                }),
                                 active_leases_.end());
            return previous_size - active_leases_.size();
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
            lease_view.targets = lease.targets;
            return claims_compatible(request, lease_view);
        }

        [[nodiscard]] ClaimEvaluation evaluate_request(const ClaimRequest &request) const {
            for (const auto &active_request : active_requests_) {
                if (active_request.id == request.id) {
                    continue;
                }

                if (!claims_compatible(request, active_request)) {
                    return ClaimEvaluation{ClaimDecision::Deny,
                                           dp::String{"conflicts with active request"},
                                           active_request.id,
                                           dp::nullopt,
                                           {}};
                }
            }

            for (const auto &active_lease : active_leases_) {
                if (!active_lease.active) {
                    continue;
                }

                if (!claims_compatible(request, active_lease)) {
                    return ClaimEvaluation{ClaimDecision::Deny,
                                           dp::String{"conflicts with granted lease"},
                                           dp::nullopt,
                                           active_lease.id,
                                           {}};
                }
            }

            return ClaimEvaluation{ClaimDecision::Grant,
                                   dp::String{"claim is compatible with current state"},
                                   dp::nullopt,
                                   dp::nullopt,
                                   {}};
        }

      private:
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
    };

} // namespace timenav
