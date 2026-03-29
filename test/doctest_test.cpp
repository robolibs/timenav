#include <doctest/doctest.h>
#include <timenav/timenav.hpp>

TEST_CASE("timenav exposes a version string") { CHECK(timenav::version() == "0.0.1"); }

TEST_CASE("timenav strong id wrappers stay distinct") {
    const timenav::RobotId robot_id{7};
    const timenav::MissionId mission_id{7};
    const timenav::ClaimId claim_id{11};
    const timenav::LeaseId lease_id{13};

    CHECK(robot_id.raw() == 7);
    CHECK(mission_id.raw() == 7);
    CHECK(claim_id.raw() == 11);
    CHECK(lease_id.raw() == 13);
    CHECK(robot_id == timenav::RobotId{7});
    CHECK_FALSE(robot_id == timenav::RobotId{8});
}

TEST_CASE("timenav ids use datapod strong scalar types") {
    const timenav::RobotId robot_id{42};

    CHECK(std::is_same_v<timenav::IdScalar, dp::u64>);
    CHECK(std::is_base_of_v<dp::Strong<dp::u64, timenav::detail::RobotIdTag>, timenav::RobotId>);
    CHECK(robot_id.raw() == static_cast<dp::u64>(42));
}

TEST_CASE("workspace index scaffold can be default constructed") {
    const timenav::WorkspaceIndex index{};

    CHECK(index.empty());
}
