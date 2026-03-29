#pragma once

#include <datapod/datapod.hpp>

namespace timenav {

    using IdScalar = dp::u64;

    namespace detail {
        struct RobotIdTag;
        struct MissionIdTag;
        struct ClaimIdTag;
        struct LeaseIdTag;
    } // namespace detail

    class RobotId : public dp::Strong<IdScalar, detail::RobotIdTag> {
      public:
        using dp::Strong<IdScalar, detail::RobotIdTag>::Strong;

        [[nodiscard]] constexpr IdScalar raw() const noexcept { return this->v_; }
    };

    class MissionId : public dp::Strong<IdScalar, detail::MissionIdTag> {
      public:
        using dp::Strong<IdScalar, detail::MissionIdTag>::Strong;

        [[nodiscard]] constexpr IdScalar raw() const noexcept { return this->v_; }
    };

    class ClaimId : public dp::Strong<IdScalar, detail::ClaimIdTag> {
      public:
        using dp::Strong<IdScalar, detail::ClaimIdTag>::Strong;

        [[nodiscard]] constexpr IdScalar raw() const noexcept { return this->v_; }
    };

    class LeaseId : public dp::Strong<IdScalar, detail::LeaseIdTag> {
      public:
        using dp::Strong<IdScalar, detail::LeaseIdTag>::Strong;

        [[nodiscard]] constexpr IdScalar raw() const noexcept { return this->v_; }
    };

} // namespace timenav
