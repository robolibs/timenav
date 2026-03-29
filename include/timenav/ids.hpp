#pragma once

#include <cstdint>

namespace timenav {

    struct RobotId {
        using value_type = std::uint64_t;

        value_type value = 0;

        constexpr RobotId() noexcept = default;
        explicit constexpr RobotId(value_type raw_value) noexcept : value(raw_value) {}

        [[nodiscard]] constexpr value_type raw() const noexcept { return value; }

        friend constexpr bool operator==(RobotId lhs, RobotId rhs) noexcept = default;
    };

    struct MissionId {
        using value_type = std::uint64_t;

        value_type value = 0;

        constexpr MissionId() noexcept = default;
        explicit constexpr MissionId(value_type raw_value) noexcept : value(raw_value) {}

        [[nodiscard]] constexpr value_type raw() const noexcept { return value; }

        friend constexpr bool operator==(MissionId lhs, MissionId rhs) noexcept = default;
    };

    struct ClaimId {
        using value_type = std::uint64_t;

        value_type value = 0;

        constexpr ClaimId() noexcept = default;
        explicit constexpr ClaimId(value_type raw_value) noexcept : value(raw_value) {}

        [[nodiscard]] constexpr value_type raw() const noexcept { return value; }

        friend constexpr bool operator==(ClaimId lhs, ClaimId rhs) noexcept = default;
    };

    struct LeaseId {
        using value_type = std::uint64_t;

        value_type value = 0;

        constexpr LeaseId() noexcept = default;
        explicit constexpr LeaseId(value_type raw_value) noexcept : value(raw_value) {}

        [[nodiscard]] constexpr value_type raw() const noexcept { return value; }

        friend constexpr bool operator==(LeaseId lhs, LeaseId rhs) noexcept = default;
    };

} // namespace timenav
