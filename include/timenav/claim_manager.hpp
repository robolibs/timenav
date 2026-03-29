#pragma once

#include "timenav/claim.hpp"
#include "timenav/workspace_index.hpp"

namespace timenav {

    class ClaimManager {
      public:
        ClaimManager() = default;
        explicit ClaimManager(const WorkspaceIndex &index) : index_(&index) {}

        [[nodiscard]] bool empty() const noexcept { return request_count() == 0 && lease_count() == 0; }
        [[nodiscard]] const WorkspaceIndex *index() const noexcept { return index_; }
        [[nodiscard]] dp::u64 request_count() const noexcept { return active_requests_.size(); }
        [[nodiscard]] dp::u64 lease_count() const noexcept { return active_leases_.size(); }
        [[nodiscard]] const dp::Vector<ClaimRequest> &requests() const noexcept { return active_requests_; }
        [[nodiscard]] const dp::Vector<Lease> &leases() const noexcept { return active_leases_; }

        void add_request(const ClaimRequest &request) { active_requests_.push_back(request); }
        void add_lease(const Lease &lease) { active_leases_.push_back(lease); }

        [[nodiscard]] const ClaimRequest *find_request(ClaimId id) const noexcept {
            for (const auto &request : active_requests_) {
                if (request.id == id) {
                    return &request;
                }
            }

            return nullptr;
        }

        [[nodiscard]] const Lease *find_lease(LeaseId id) const noexcept {
            for (const auto &lease : active_leases_) {
                if (lease.id == id) {
                    return &lease;
                }
            }

            return nullptr;
        }

      private:
        const WorkspaceIndex *index_ = nullptr;
        dp::Vector<ClaimRequest> active_requests_{};
        dp::Vector<Lease> active_leases_{};
    };

} // namespace timenav
