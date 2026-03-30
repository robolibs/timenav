#include <doctest/doctest.h>
#include <timenav/timenav.hpp>

TEST_CASE("zone policy accepts ui editor camelCase aliases through dp maps") {
    dp::Map<dp::String, dp::String> properties = {
        {"traffic.mode", "restricted"},
        {"traffic.maxOccupancy", "3"},
        {"traffic.claimRequired", "true"},
        {"traffic.waitingAllowed", "false"},
        {"traffic.stopAllowed", "false"},
        {"traffic.scheduleWindow", "weekday_day"},
        {"traffic.accessGroup", "gate-a"},
        {"traffic.blocksEntryWithoutGrant", "true"},
        {"traffic.blocksTraversalWithoutGrant", "true"},
    };

    const auto policy = timenav::parse_zone_policy(properties);
    const auto issues = timenav::validate_zone_traffic_properties(properties);

    CHECK(policy.kind == timenav::ZonePolicyKind::Restricted);
    CHECK(policy.capacity == 3);
    CHECK(policy.capacity_is_explicit);
    CHECK(policy.requires_claim);
    REQUIRE(policy.waiting_allowed.has_value());
    CHECK_FALSE(policy.waiting_allowed.value());
    REQUIRE(policy.stop_allowed.has_value());
    CHECK_FALSE(policy.stop_allowed.value());
    REQUIRE(policy.schedule_window.has_value());
    CHECK(policy.schedule_window.value() == "weekday_day");
    REQUIRE(policy.access_group.has_value());
    CHECK(policy.access_group.value() == "gate-a");
    CHECK(issues.empty());
}

TEST_CASE("edge traffic semantics accepts ui editor camelCase aliases through dp maps") {
    dp::Map<dp::String, dp::String> properties = {
        {"traffic.speedLimit", "1.5"},
        {"traffic.laneType", "corridor"},
        {"traffic.passingAllowed", "false"},
        {"traffic.maxOccupancy", "2"},
        {"traffic.clearanceWidth", "1.2"},
        {"traffic.clearanceHeight", "2.0"},
        {"traffic.costBias", "0.7"},
        {"traffic.preferredDirection", "forward"},
        {"traffic.claimRequired", "true"},
        {"traffic.waitingAllowed", "false"},
        {"traffic.stopAllowed", "false"},
        {"traffic.scheduleWindow", "night"},
        {"traffic.accessGroup", "robots-a"},
    };

    const auto semantics = timenav::parse_edge_traffic_semantics(properties, true);
    const auto issues = timenav::validate_edge_traffic_properties(properties);

    CHECK(semantics.directed);
    REQUIRE(semantics.speed_limit.has_value());
    CHECK(semantics.speed_limit.value() == doctest::Approx(1.5));
    REQUIRE(semantics.lane_type.has_value());
    CHECK(semantics.lane_type.value() == "corridor");
    REQUIRE(semantics.capacity.has_value());
    CHECK(semantics.capacity.value() == 2);
    CHECK(semantics.capacity_is_explicit);
    REQUIRE(semantics.schedule_window.has_value());
    CHECK(semantics.schedule_window.value() == "night");
    REQUIRE(semantics.access_group.has_value());
    CHECK(semantics.access_group.value() == "robots-a");
    CHECK(issues.empty());
}

TEST_CASE("policy module reports malformed aliased editor properties precisely") {
    dp::Map<dp::String, dp::String> zone_properties = {
        {"traffic.maxOccupancy", "0"},
        {"traffic.speedLimit", "fast"},
        {"traffic.waitingAllowed", "maybe"},
    };
    dp::Map<dp::String, dp::String> edge_properties = {
        {"traffic.speedLimit", "-1"},
        {"traffic.clearanceWidth", "wide"},
        {"traffic.claimRequired", "sometimes"},
    };

    const auto zone_issues = timenav::validate_zone_traffic_properties(zone_properties);
    const auto edge_issues = timenav::validate_edge_traffic_properties(edge_properties);

    CHECK(zone_issues.size() >= 3);
    CHECK(edge_issues.size() >= 3);
}
