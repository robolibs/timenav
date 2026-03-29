#include <doctest/doctest.h>
#include <timenav/timenav.hpp>

namespace {

    dp::Polygon rectangle(double min_x, double min_y, double max_x, double max_y) {
        dp::Polygon polygon;
        polygon.vertices.push_back(dp::Point{min_x, min_y, 0.0});
        polygon.vertices.push_back(dp::Point{max_x, min_y, 0.0});
        polygon.vertices.push_back(dp::Point{max_x, max_y, 0.0});
        polygon.vertices.push_back(dp::Point{min_x, max_y, 0.0});
        return polygon;
    }

    struct TestWorkspace {
        zoneout::Workspace workspace;
        zoneout::UUID node_a_id;
        zoneout::UUID node_b_id;
        zoneout::UUID node_c_id;
        zoneout::UUID edge_ab_id;
        zoneout::UUID edge_bc_id;

        explicit TestWorkspace(zoneout::Workspace workspace_value) : workspace(std::move(workspace_value)) {}
    };

    TestWorkspace make_test_workspace() {
        auto root = zoneout::ZoneBuilder()
                        .with_name("root")
                        .with_type("workspace")
                        .with_boundary(rectangle(0.0, 0.0, 100.0, 100.0))
                        .with_datum(dp::Geo{52.0, 5.0, 0.0})
                        .with_property("traffic.mode", "shared")
                        .build();

        auto child_a = zoneout::ZoneBuilder()
                           .with_name("child-a")
                           .with_type("field")
                           .with_boundary(rectangle(10.0, 10.0, 40.0, 40.0))
                           .with_datum(dp::Geo{52.0, 5.0, 0.0})
                           .with_property("traffic.speed_limit", "1.2")
                           .build();

        auto child_b = zoneout::ZoneBuilder()
                           .with_name("child-b")
                           .with_type("field")
                           .with_boundary(rectangle(50.0, 10.0, 90.0, 40.0))
                           .with_datum(dp::Geo{52.0, 5.0, 0.0})
                           .build();

        auto grandchild = zoneout::ZoneBuilder()
                              .with_name("grandchild-a1")
                              .with_type("lane")
                              .with_boundary(rectangle(12.0, 12.0, 18.0, 18.0))
                              .with_datum(dp::Geo{52.0, 5.0, 0.0})
                              .build();

        child_a.add_child(std::move(grandchild));
        root.add_child(std::move(child_a));
        root.add_child(std::move(child_b));

        TestWorkspace fixture{zoneout::Workspace(std::move(root))};
        fixture.workspace.set_ref(dp::Geo{52.0, 5.0, 10.0});
        fixture.workspace.set_coord_mode(zoneout::CoordMode::Local);

        fixture.node_a_id = zoneout::UUID("11111111-1111-4111-8111-111111111111");
        fixture.node_b_id = zoneout::UUID("22222222-2222-4222-8222-222222222222");
        fixture.node_c_id = zoneout::UUID("33333333-3333-4333-8333-333333333333");
        fixture.edge_ab_id = zoneout::UUID("44444444-4444-4444-8444-444444444444");
        fixture.edge_bc_id = zoneout::UUID("55555555-5555-4555-8555-555555555555");

        const auto node_a_vertex =
            fixture.workspace.add_node(zoneout::NodeData{fixture.node_a_id, dp::Point{20.0, 20.0, 0.0}});
        const auto node_b_vertex =
            fixture.workspace.add_node(zoneout::NodeData{fixture.node_b_id, dp::Point{30.0, 20.0, 0.0}});
        const auto node_c_vertex =
            fixture.workspace.add_node(zoneout::NodeData{fixture.node_c_id, dp::Point{70.0, 20.0, 0.0}});

        fixture.workspace.add_edge(node_a_vertex, node_b_vertex,
                                   zoneout::EdgeData{fixture.edge_ab_id, {{"traffic.priority", "high"}}});
        fixture.workspace.add_edge(node_b_vertex, node_c_vertex,
                                   zoneout::EdgeData{fixture.edge_bc_id, {{"traffic.priority", "yield"}}});

        return fixture;
    }

    zoneout::Workspace make_invalid_workspace() {
        auto fixture = make_test_workspace();
        fixture.workspace.clear_ref();
        fixture.workspace.set_coord_mode(zoneout::CoordMode::Local);

        const auto node_vertex = fixture.workspace.find_node(fixture.node_a_id);
        if (!node_vertex.has_value()) {
            throw std::runtime_error("expected node_a to exist in invalid workspace fixture");
        }
        fixture.workspace.graph()[*node_vertex].id = zoneout::UUID::null();
        fixture.workspace.graph()[*node_vertex].zone_ids.push_back(zoneout::UUID("ffffffff-ffff-4fff-8fff-ffffffffffff"));

        const auto edge_id = fixture.workspace.find_edge(fixture.edge_ab_id);
        if (!edge_id.has_value()) {
            throw std::runtime_error("expected edge_ab to exist in invalid workspace fixture");
        }
        fixture.workspace.graph().edge_property(*edge_id).zone_ids.push_back(
            zoneout::UUID("99999999-9999-4999-8999-999999999999"));

        fixture.workspace.root_zone().node_ids().push_back(zoneout::UUID("abababab-abab-4bab-8bab-abababababab"));
        return std::move(fixture.workspace);
    }

    struct RouteChoiceWorkspace {
        zoneout::Workspace workspace;
        zoneout::UUID node_a_id;
        zoneout::UUID node_b_id;
        zoneout::UUID node_c_id;
        zoneout::UUID node_d_id;
        zoneout::UUID edge_ab_id;
        zoneout::UUID edge_bd_id;
        zoneout::UUID edge_ac_id;
        zoneout::UUID edge_cd_id;
        zoneout::UUID blocked_zone_id;

        explicit RouteChoiceWorkspace(zoneout::Workspace workspace_value) : workspace(std::move(workspace_value)) {}
    };

    RouteChoiceWorkspace make_route_choice_workspace() {
        auto root = zoneout::ZoneBuilder()
                        .with_name("root")
                        .with_type("workspace")
                        .with_boundary(rectangle(0.0, 0.0, 100.0, 100.0))
                        .with_datum(dp::Geo{52.0, 5.0, 0.0})
                        .build();

        auto blocked_lane = zoneout::ZoneBuilder()
                                .with_name("blocked-lane")
                                .with_type("lane")
                                .with_boundary(rectangle(40.0, 45.0, 60.0, 55.0))
                                .with_datum(dp::Geo{52.0, 5.0, 0.0})
                                .with_property("traffic.blocked", "true")
                                .build();
        const auto blocked_zone_id = blocked_lane.id();
        root.add_child(std::move(blocked_lane));

        RouteChoiceWorkspace fixture{zoneout::Workspace(std::move(root))};
        fixture.blocked_zone_id = blocked_zone_id;
        fixture.node_a_id = zoneout::UUID("66666666-6666-4666-8666-666666666666");
        fixture.node_b_id = zoneout::UUID("77777777-7777-4777-8777-777777777777");
        fixture.node_c_id = zoneout::UUID("88888888-8888-4888-8888-888888888888");
        fixture.node_d_id = zoneout::UUID("99999999-9999-4999-8999-999999999999");
        fixture.edge_ab_id = zoneout::UUID("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
        fixture.edge_bd_id = zoneout::UUID("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb");
        fixture.edge_ac_id = zoneout::UUID("cccccccc-cccc-4ccc-8ccc-cccccccccccc");
        fixture.edge_cd_id = zoneout::UUID("dddddddd-dddd-4ddd-8ddd-dddddddddddd");

        const auto node_a = fixture.workspace.add_node(zoneout::NodeData{fixture.node_a_id, dp::Point{10.0, 50.0, 0.0}});
        const auto node_b = fixture.workspace.add_node(zoneout::NodeData{fixture.node_b_id, dp::Point{40.0, 50.0, 0.0}});
        const auto node_c = fixture.workspace.add_node(zoneout::NodeData{fixture.node_c_id, dp::Point{10.0, 20.0, 0.0}});
        const auto node_d = fixture.workspace.add_node(zoneout::NodeData{fixture.node_d_id, dp::Point{70.0, 50.0, 0.0}});

        fixture.workspace.add_edge(node_a, node_b, zoneout::EdgeData{fixture.edge_ab_id, {}});
        fixture.workspace.add_edge(node_b, node_d, zoneout::EdgeData{fixture.edge_bd_id, {}});
        fixture.workspace.add_edge(node_a, node_c, zoneout::EdgeData{fixture.edge_ac_id, {}});
        fixture.workspace.add_edge(node_c, node_d, zoneout::EdgeData{fixture.edge_cd_id, {}});

        const auto blocked_edge = fixture.workspace.find_edge(fixture.edge_bd_id);
        if (!blocked_edge.has_value()) {
            throw std::runtime_error("expected blocked route edge to exist");
        }
        fixture.workspace.graph().edge_property(*blocked_edge).zone_ids.push_back(fixture.blocked_zone_id);

        return fixture;
    }

    RouteChoiceWorkspace make_penalized_route_workspace() {
        auto fixture = make_route_choice_workspace();

        fixture.workspace.root_zone().children()[0].remove_property("traffic.blocked");
        fixture.workspace.root_zone().children()[0].set_property("traffic.claim_required", "true");

        const auto edge_ab = fixture.workspace.find_edge(fixture.edge_ab_id);
        const auto edge_bd = fixture.workspace.find_edge(fixture.edge_bd_id);
        const auto edge_ac = fixture.workspace.find_edge(fixture.edge_ac_id);
        const auto edge_cd = fixture.workspace.find_edge(fixture.edge_cd_id);
        if (!edge_ab.has_value() || !edge_bd.has_value() || !edge_ac.has_value() || !edge_cd.has_value()) {
            throw std::runtime_error("expected all route choice edges to exist");
        }

        fixture.workspace.graph().set_weight(*edge_ab, 1.0);
        fixture.workspace.graph().set_weight(*edge_bd, 1.0);
        fixture.workspace.graph().set_weight(*edge_ac, 2.0);
        fixture.workspace.graph().set_weight(*edge_cd, 2.0);
        fixture.workspace.graph().edge_property(*edge_ab).properties["traffic.cost_bias"] = "4.5";
        fixture.workspace.graph().edge_property(*edge_bd).properties["traffic.speed_limit"] = "0.25";

        return fixture;
    }

    RouteChoiceWorkspace make_blocked_only_route_workspace() {
        auto fixture = make_route_choice_workspace();

        const auto edge_ac = fixture.workspace.find_edge(fixture.edge_ac_id);
        const auto edge_cd = fixture.workspace.find_edge(fixture.edge_cd_id);
        if (!edge_ac.has_value() || !edge_cd.has_value()) {
            throw std::runtime_error("expected alternative route edges to exist");
        }

        fixture.workspace.graph().edge_property(*edge_ac).properties["traffic.blocked"] = "true";
        fixture.workspace.graph().edge_property(*edge_cd).properties["traffic.blocked"] = "true";
        return fixture;
    }

    TestWorkspace make_unreachable_test_workspace() {
        auto fixture = make_test_workspace();
        fixture.node_c_id = zoneout::UUID("12121212-3434-4567-8999-aaaaaaaaaaaa");

        auto root = zoneout::ZoneBuilder()
                        .with_name("root")
                        .with_type("workspace")
                        .with_boundary(rectangle(0.0, 0.0, 100.0, 100.0))
                        .with_datum(dp::Geo{52.0, 5.0, 0.0})
                        .build();

        TestWorkspace isolated{zoneout::Workspace(std::move(root))};
        isolated.node_a_id = fixture.node_a_id;
        isolated.node_b_id = fixture.node_b_id;
        isolated.node_c_id = fixture.node_c_id;
        isolated.edge_ab_id = fixture.edge_ab_id;

        const auto node_a = isolated.workspace.add_node(zoneout::NodeData{isolated.node_a_id, dp::Point{10.0, 10.0, 0.0}});
        const auto node_b = isolated.workspace.add_node(zoneout::NodeData{isolated.node_b_id, dp::Point{20.0, 10.0, 0.0}});
        isolated.workspace.add_node(zoneout::NodeData{isolated.node_c_id, dp::Point{80.0, 80.0, 0.0}});
        isolated.workspace.add_edge(node_a, node_b, zoneout::EdgeData{isolated.edge_ab_id, {}});
        return isolated;
    }

} // namespace

TEST_CASE("timenav exposes a version string") { CHECK(timenav::version() == "0.0.1"); }

TEST_CASE("route types expose typed defaults") {
    const timenav::RouteStep step{};
    const timenav::RoutePlan plan{};
    const timenav::RouteSearchState search{};

    CHECK_FALSE(step.incoming_edge_id.has_value());
    CHECK(plan.steps.empty());
    CHECK(plan.traversed_node_ids.empty());
    CHECK(plan.traversed_edge_ids.empty());
    CHECK(plan.traversed_zone_ids.empty());
    CHECK(plan.total_cost == doctest::Approx(0.0));
    CHECK_FALSE(search.found);
    CHECK(std::isinf(search.distance));
    CHECK(search.distances.empty());
    CHECK(search.predecessors.empty());
}

TEST_CASE("claim types expose typed defaults") {
    const timenav::ClaimTarget target{};
    const timenav::ClaimRequest request{};
    const timenav::Lease lease{};

    CHECK(target.kind == timenav::ClaimTargetKind::Zone);
    CHECK(target.resource_id.isNull());
    CHECK(request.id.raw() == 0);
    CHECK(request.robot_id.raw() == 0);
    CHECK(request.mission_id.raw() == 0);
    CHECK(request.access_mode == timenav::ClaimAccessMode::Exclusive);
    CHECK(request.priority == 0);
    CHECK(request.targets.empty());
    CHECK(lease.id.raw() == 0);
    CHECK(lease.claim_id.raw() == 0);
    CHECK(lease.robot_id.raw() == 0);
    CHECK(lease.access_mode == timenav::ClaimAccessMode::Exclusive);
    CHECK(lease.targets.empty());
    CHECK_FALSE(lease.expires_at_tick.has_value());
    CHECK(lease.active);
}

TEST_CASE("claim manager scaffold can be default constructed or bound to an index") {
    const timenav::ClaimManager empty_manager{};
    CHECK(empty_manager.empty());
    CHECK(empty_manager.index() == nullptr);
    CHECK(empty_manager.request_count() == 0);
    CHECK(empty_manager.lease_count() == 0);

    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};
    const timenav::ClaimManager indexed_manager{index};

    CHECK(indexed_manager.empty());
    CHECK(indexed_manager.index() == &index);
    CHECK(indexed_manager.request_count() == 0);
    CHECK(indexed_manager.lease_count() == 0);
}

TEST_CASE("robot state exposes typed defaults") {
    const timenav::RobotState state{};

    CHECK(state.robot_id.raw() == 0);
    CHECK(state.mission_id.raw() == 0);
    CHECK_FALSE(state.current_node_id.has_value());
    CHECK_FALSE(state.current_edge_id.has_value());
    CHECK_FALSE(state.route_plan.has_value());
    CHECK(state.pending_claim_ids.empty());
    CHECK(state.active_lease_ids.empty());
    CHECK(state.horizon == 0);
    CHECK(state.updated_at_tick == 0);
}

TEST_CASE("coordinator scaffold can be default constructed or bound to an index") {
    const timenav::Coordinator empty_coordinator{};
    CHECK(empty_coordinator.index() == nullptr);
    CHECK(empty_coordinator.empty());
    CHECK(empty_coordinator.robot_count() == 0);

    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};
    const timenav::Coordinator coordinator{index};

    CHECK(coordinator.index() == &index);
    CHECK(coordinator.empty());
    CHECK(coordinator.robot_count() == 0);
    CHECK(coordinator.claim_manager().index() == &index);
}

TEST_CASE("coordinator registers and updates robot state") {
    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};
    timenav::Coordinator coordinator{index};

    timenav::RobotState state{};
    state.robot_id = timenav::RobotId{11};
    state.mission_id = timenav::MissionId{21};
    state.current_node_id = fixture.node_a_id;
    coordinator.register_robot(state);

    REQUIRE(coordinator.find_robot_state(timenav::RobotId{11}) != nullptr);
    CHECK(coordinator.robot_count() == 1);
    CHECK(coordinator.find_robot_state(timenav::RobotId{11})->mission_id == timenav::MissionId{21});

    state.current_node_id = fixture.node_b_id;
    coordinator.register_robot(state);

    CHECK(coordinator.robot_count() == 1);
    REQUIRE(coordinator.find_robot_state(timenav::RobotId{11})->current_node_id.has_value());
    CHECK(coordinator.find_robot_state(timenav::RobotId{11})->current_node_id.value() == fixture.node_b_id);
    CHECK(coordinator.find_robot_state(timenav::RobotId{99}) == nullptr);
}

TEST_CASE("route to claim helpers derive ordered claim targets and requests") {
    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};

    dp::Vector<zoneout::UUID> route_nodes;
    route_nodes.push_back(fixture.node_a_id);
    route_nodes.push_back(fixture.node_b_id);
    route_nodes.push_back(fixture.node_c_id);

    const auto route_plan = timenav::build_route_plan(index, fixture.node_a_id, fixture.node_c_id, route_nodes);
    REQUIRE(route_plan.is_ok());

    const auto targets = timenav::claim_targets_from_route(route_plan.value());
    const auto request =
        timenav::claim_request_from_route(timenav::ClaimId{111}, timenav::RobotId{12}, timenav::MissionId{13},
                                          route_plan.value(), timenav::ClaimAccessMode::Exclusive);

    REQUIRE(targets.size() ==
            route_plan.value().traversed_zone_ids.size() + route_plan.value().traversed_edge_ids.size() +
                route_plan.value().traversed_node_ids.size());
    CHECK(targets[0].kind == timenav::ClaimTargetKind::Zone);
    CHECK(targets[route_plan.value().traversed_zone_ids.size()].kind == timenav::ClaimTargetKind::Edge);
    CHECK(targets.back().kind == timenav::ClaimTargetKind::Node);
    CHECK(request.id == timenav::ClaimId{111});
    CHECK(request.robot_id == timenav::RobotId{12});
    CHECK(request.mission_id == timenav::MissionId{13});
    REQUIRE(request.targets.size() == targets.size());
    CHECK(request.targets.front().kind == targets.front().kind);
    CHECK(request.targets.front().resource_id == targets.front().resource_id);
    CHECK(request.targets.back().kind == targets.back().kind);
    CHECK(request.targets.back().resource_id == targets.back().resource_id);
}

TEST_CASE("coordinator derives rolling horizon claim requests from registered robot routes") {
    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};
    timenav::Coordinator coordinator{index};

    dp::Vector<zoneout::UUID> route_nodes;
    route_nodes.push_back(fixture.node_a_id);
    route_nodes.push_back(fixture.node_b_id);
    route_nodes.push_back(fixture.node_c_id);

    const auto route_plan = timenav::build_route_plan(index, fixture.node_a_id, fixture.node_c_id, route_nodes);
    REQUIRE(route_plan.is_ok());

    timenav::RobotState state{};
    state.robot_id = timenav::RobotId{31};
    state.mission_id = timenav::MissionId{41};
    state.route_plan = route_plan.value();
    state.horizon = 1;
    coordinator.register_robot(state);

    const auto request = coordinator.claim_request_for_robot(timenav::RobotId{31}, timenav::ClaimId{121});
    const auto expected_zone_targets = std::min<dp::u64>(route_plan.value().traversed_zone_ids.size(), state.horizon + 1);
    const auto expected_edge_targets = std::min<dp::u64>(route_plan.value().traversed_edge_ids.size(), state.horizon);
    const auto expected_node_targets = std::min<dp::u64>(route_plan.value().traversed_node_ids.size(), state.horizon + 1);

    CHECK(request.id == timenav::ClaimId{121});
    CHECK(request.robot_id == timenav::RobotId{31});
    CHECK(request.mission_id == timenav::MissionId{41});
    CHECK(request.targets.size() == expected_zone_targets + expected_edge_targets + expected_node_targets);
    CHECK(request.targets[0].kind == timenav::ClaimTargetKind::Zone);
    CHECK(request.targets[expected_zone_targets].kind == timenav::ClaimTargetKind::Edge);
    CHECK(request.targets[expected_zone_targets + expected_edge_targets].kind == timenav::ClaimTargetKind::Node);
    CHECK(request.targets.back().kind == timenav::ClaimTargetKind::Node);
}

TEST_CASE("coordinator updates robot progress by node and edge") {
    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};
    timenav::Coordinator coordinator{index};

    timenav::RobotState state{};
    state.robot_id = timenav::RobotId{51};
    coordinator.register_robot(state);

    CHECK(coordinator.update_robot_progress(timenav::RobotId{51}, fixture.node_b_id, fixture.edge_bc_id, 42));
    REQUIRE(coordinator.find_robot_state(timenav::RobotId{51}) != nullptr);
    REQUIRE(coordinator.find_robot_state(timenav::RobotId{51})->current_node_id.has_value());
    REQUIRE(coordinator.find_robot_state(timenav::RobotId{51})->current_edge_id.has_value());
    CHECK(coordinator.find_robot_state(timenav::RobotId{51})->current_node_id.value() == fixture.node_b_id);
    CHECK(coordinator.find_robot_state(timenav::RobotId{51})->current_edge_id.value() == fixture.edge_bc_id);
    CHECK(coordinator.find_robot_state(timenav::RobotId{51})->updated_at_tick == 42);
    CHECK_FALSE(coordinator.update_robot_progress(timenav::RobotId{99}, fixture.node_a_id, fixture.edge_ab_id, 1));
}

TEST_CASE("coordinator releases leases behind current progress") {
    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};
    timenav::Coordinator coordinator{index};

    dp::Vector<zoneout::UUID> route_nodes;
    route_nodes.push_back(fixture.node_a_id);
    route_nodes.push_back(fixture.node_b_id);
    route_nodes.push_back(fixture.node_c_id);
    const auto route_plan = timenav::build_route_plan(index, fixture.node_a_id, fixture.node_c_id, route_nodes);
    REQUIRE(route_plan.is_ok());

    timenav::RobotState state{};
    state.robot_id = timenav::RobotId{61};
    state.route_plan = route_plan.value();
    state.current_node_id = fixture.node_b_id;
    state.active_lease_ids.push_back(timenav::LeaseId{201});
    state.active_lease_ids.push_back(timenav::LeaseId{202});
    coordinator.register_robot(state);

    timenav::Lease behind{};
    behind.id = timenav::LeaseId{201};
    behind.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Node, fixture.node_a_id});
    coordinator.claim_manager().add_lease(behind);

    timenav::Lease ahead{};
    ahead.id = timenav::LeaseId{202};
    ahead.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Edge, fixture.edge_bc_id});
    coordinator.claim_manager().add_lease(ahead);

    CHECK(coordinator.release_behind_progress(timenav::RobotId{61}) == 1);
    REQUIRE(coordinator.find_robot_state(timenav::RobotId{61}) != nullptr);
    CHECK(coordinator.find_robot_state(timenav::RobotId{61})->active_lease_ids.size() == 1);
    CHECK(coordinator.find_robot_state(timenav::RobotId{61})->active_lease_ids[0] == timenav::LeaseId{202});
    CHECK(coordinator.claim_manager().find_lease(timenav::LeaseId{201}) == nullptr);
    CHECK(coordinator.claim_manager().find_lease(timenav::LeaseId{202}) != nullptr);
}

TEST_CASE("schedule window helpers detect route zone conflicts") {
    auto fixture = make_test_workspace();
    fixture.workspace.root_zone().children()[0].set_property("traffic.schedule_window", "day");
    fixture.workspace.root_zone().children()[1].set_property("traffic.schedule_window", "night");

    const timenav::WorkspaceIndex index{fixture.workspace};
    dp::Vector<zoneout::UUID> route_nodes;
    route_nodes.push_back(fixture.node_a_id);
    route_nodes.push_back(fixture.node_b_id);
    route_nodes.push_back(fixture.node_c_id);
    const auto route_plan = timenav::build_route_plan(index, fixture.node_a_id, fixture.node_c_id, route_nodes);
    REQUIRE(route_plan.is_ok());

    const auto conflicts = timenav::route_schedule_window_conflicts(index, route_plan.value(), "day");

    REQUIRE(conflicts.size() == 1);
    CHECK(conflicts[0] == fixture.workspace.root_zone().children()[1].id());
    CHECK_FALSE(timenav::route_matches_schedule_window(index, route_plan.value(), "day"));
    CHECK_FALSE(timenav::route_matches_schedule_window(index, route_plan.value(), "night"));
}

TEST_CASE("arbitration hooks choose proceed yield or replan from simple priority signals") {
    CHECK(timenav::arbitrate_right_of_way(timenav::ArbitrationContext{5.0, 1.0, false, false}) ==
          timenav::ArbitrationDecision::Proceed);
    CHECK(timenav::arbitrate_right_of_way(timenav::ArbitrationContext{1.0, 5.0, false, false}) ==
          timenav::ArbitrationDecision::Yield);
    CHECK(timenav::arbitrate_right_of_way(timenav::ArbitrationContext{1.0, 1.0, false, true}) ==
          timenav::ArbitrationDecision::Yield);
    CHECK(timenav::arbitrate_right_of_way(timenav::ArbitrationContext{1.0, 1.0, true, false}) ==
          timenav::ArbitrationDecision::Proceed);
    CHECK(timenav::arbitrate_right_of_way(timenav::ArbitrationContext{1.0, 1.0, false, false}) ==
          timenav::ArbitrationDecision::Replan);
}

TEST_CASE("claim manager stores active claim requests") {
    timenav::ClaimManager manager{};

    timenav::ClaimRequest request{};
    request.id = timenav::ClaimId{41};
    request.robot_id = timenav::RobotId{7};
    request.mission_id = timenav::MissionId{9};
    request.targets.push_back(
        timenav::ClaimTarget{timenav::ClaimTargetKind::Zone, zoneout::UUID("10101010-1010-4010-8010-101010101010")});

    manager.add_request(request);

    CHECK_FALSE(manager.empty());
    CHECK(manager.request_count() == 1);
    REQUIRE(manager.find_request(timenav::ClaimId{41}) != nullptr);
    CHECK(manager.find_request(timenav::ClaimId{41})->robot_id == timenav::RobotId{7});
    CHECK(manager.requests().size() == 1);
    CHECK(manager.find_request(timenav::ClaimId{99}) == nullptr);
}

TEST_CASE("claim manager stores granted leases") {
    timenav::ClaimManager manager{};

    timenav::Lease lease{};
    lease.id = timenav::LeaseId{51};
    lease.claim_id = timenav::ClaimId{41};
    lease.robot_id = timenav::RobotId{7};
    lease.targets.push_back(
        timenav::ClaimTarget{timenav::ClaimTargetKind::Zone, zoneout::UUID("20202020-2020-4020-8020-202020202020")});
    lease.expires_at_tick = 123;

    manager.add_lease(lease);

    CHECK_FALSE(manager.empty());
    CHECK(manager.lease_count() == 1);
    REQUIRE(manager.find_lease(timenav::LeaseId{51}) != nullptr);
    CHECK(manager.find_lease(timenav::LeaseId{51})->claim_id == timenav::ClaimId{41});
    CHECK(manager.leases().size() == 1);
    CHECK(manager.find_lease(timenav::LeaseId{99}) == nullptr);
}

TEST_CASE("zone claim compatibility distinguishes overlapping exclusive and shared access") {
    const auto shared_zone = zoneout::UUID("30303030-3030-4030-8030-303030303030");
    const auto other_zone = zoneout::UUID("40404040-4040-4040-8040-404040404040");

    timenav::ClaimRequest exclusive_request{};
    exclusive_request.access_mode = timenav::ClaimAccessMode::Exclusive;
    exclusive_request.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Zone, shared_zone});

    timenav::ClaimRequest shared_request{};
    shared_request.access_mode = timenav::ClaimAccessMode::Shared;
    shared_request.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Zone, shared_zone});

    timenav::ClaimRequest disjoint_request{};
    disjoint_request.access_mode = timenav::ClaimAccessMode::Exclusive;
    disjoint_request.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Zone, other_zone});

    timenav::ClaimRequest shared_peer{};
    shared_peer.access_mode = timenav::ClaimAccessMode::Shared;
    shared_peer.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Zone, shared_zone});

    CHECK_FALSE(timenav::ClaimManager::zone_claims_compatible(exclusive_request, shared_request));
    CHECK(timenav::ClaimManager::zone_claims_compatible(shared_request, shared_peer));
    CHECK(timenav::ClaimManager::zone_claims_compatible(exclusive_request, disjoint_request));
}

TEST_CASE("edge and node claim compatibility detect overlapping exclusive targets") {
    const auto shared_node = zoneout::UUID("50505050-5050-4050-8050-505050505050");
    const auto shared_edge = zoneout::UUID("60606060-6060-4060-8060-606060606060");

    timenav::ClaimRequest lhs{};
    lhs.access_mode = timenav::ClaimAccessMode::Exclusive;
    lhs.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Node, shared_node});
    lhs.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Edge, shared_edge});

    timenav::ClaimRequest rhs{};
    rhs.access_mode = timenav::ClaimAccessMode::Shared;
    rhs.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Node, shared_node});
    rhs.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Edge, shared_edge});

    timenav::ClaimRequest disjoint{};
    disjoint.access_mode = timenav::ClaimAccessMode::Exclusive;
    disjoint.targets.push_back(
        timenav::ClaimTarget{timenav::ClaimTargetKind::Node, zoneout::UUID("70707070-7070-4070-8070-707070707070")});
    disjoint.targets.push_back(
        timenav::ClaimTarget{timenav::ClaimTargetKind::Edge, zoneout::UUID("80808080-8080-4080-8080-808080808080")});

    CHECK_FALSE(timenav::ClaimManager::node_claims_compatible(lhs, rhs));
    CHECK_FALSE(timenav::ClaimManager::edge_claims_compatible(lhs, rhs));
    CHECK_FALSE(timenav::ClaimManager::claims_compatible(lhs, rhs));
    CHECK(timenav::ClaimManager::claims_compatible(lhs, disjoint));
}

TEST_CASE("claim manager evaluates requests against active requests and leases") {
    timenav::ClaimManager manager{};
    const auto shared_zone = zoneout::UUID("90909090-9090-4090-8090-909090909090");
    const auto free_zone = zoneout::UUID("a0a0a0a0-a0a0-40a0-80a0-a0a0a0a0a0a0");

    timenav::ClaimRequest active_request{};
    active_request.id = timenav::ClaimId{61};
    active_request.access_mode = timenav::ClaimAccessMode::Exclusive;
    active_request.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Zone, shared_zone});
    manager.add_request(active_request);

    timenav::Lease active_lease{};
    active_lease.id = timenav::LeaseId{71};
    active_lease.access_mode = timenav::ClaimAccessMode::Exclusive;
    active_lease.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Zone, free_zone});
    manager.add_lease(active_lease);

    timenav::ClaimRequest denied_by_request{};
    denied_by_request.id = timenav::ClaimId{62};
    denied_by_request.access_mode = timenav::ClaimAccessMode::Exclusive;
    denied_by_request.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Zone, shared_zone});

    timenav::ClaimRequest denied_by_lease{};
    denied_by_lease.id = timenav::ClaimId{63};
    denied_by_lease.access_mode = timenav::ClaimAccessMode::Exclusive;
    denied_by_lease.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Zone, free_zone});

    timenav::ClaimRequest granted{};
    granted.id = timenav::ClaimId{64};
    granted.access_mode = timenav::ClaimAccessMode::Shared;
    granted.targets.push_back(
        timenav::ClaimTarget{timenav::ClaimTargetKind::Zone, zoneout::UUID("b0b0b0b0-b0b0-40b0-80b0-b0b0b0b0b0b0")});

    const auto request_conflict = manager.evaluate_request(denied_by_request);
    const auto lease_conflict = manager.evaluate_request(denied_by_lease);
    const auto grant = manager.evaluate_request(granted);

    CHECK(request_conflict.decision == timenav::ClaimDecision::Deny);
    REQUIRE(request_conflict.conflicting_claim_id.has_value());
    CHECK(request_conflict.conflicting_claim_id.value() == timenav::ClaimId{61});
    CHECK(lease_conflict.decision == timenav::ClaimDecision::Deny);
    REQUIRE(lease_conflict.conflicting_lease_id.has_value());
    CHECK(lease_conflict.conflicting_lease_id.value() == timenav::LeaseId{71});
    CHECK(grant.decision == timenav::ClaimDecision::Grant);
    CHECK_FALSE(grant.conflicting_claim_id.has_value());
    CHECK_FALSE(grant.conflicting_lease_id.has_value());
}

TEST_CASE("claim manager releases active leases by id") {
    timenav::ClaimManager manager{};

    timenav::Lease lease{};
    lease.id = timenav::LeaseId{81};
    manager.add_lease(lease);

    CHECK(manager.release_lease(timenav::LeaseId{81}));
    CHECK(manager.lease_count() == 0);
    CHECK(manager.find_lease(timenav::LeaseId{81}) == nullptr);
    CHECK_FALSE(manager.release_lease(timenav::LeaseId{81}));
}

TEST_CASE("claim manager expires leases at or before the current tick") {
    timenav::ClaimManager manager{};

    timenav::Lease expired{};
    expired.id = timenav::LeaseId{91};
    expired.expires_at_tick = 10;
    manager.add_lease(expired);

    timenav::Lease retained{};
    retained.id = timenav::LeaseId{92};
    retained.expires_at_tick = 20;
    manager.add_lease(retained);

    timenav::Lease no_expiry{};
    no_expiry.id = timenav::LeaseId{93};
    manager.add_lease(no_expiry);

    CHECK(manager.expire_leases(10) == 1);
    CHECK(manager.find_lease(timenav::LeaseId{91}) == nullptr);
    CHECK(manager.find_lease(timenav::LeaseId{92}) != nullptr);
    CHECK(manager.find_lease(timenav::LeaseId{93}) != nullptr);
}

TEST_CASE("claim manager regression covers conflicting and non-conflicting scenarios") {
    timenav::ClaimManager manager{};
    const auto zone_id = zoneout::UUID("c0c0c0c0-c0c0-40c0-80c0-c0c0c0c0c0c0");
    const auto edge_id = zoneout::UUID("d0d0d0d0-d0d0-40d0-80d0-d0d0d0d0d0d0");

    timenav::ClaimRequest baseline{};
    baseline.id = timenav::ClaimId{101};
    baseline.access_mode = timenav::ClaimAccessMode::Exclusive;
    baseline.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Zone, zone_id});
    manager.add_request(baseline);

    timenav::Lease lease{};
    lease.id = timenav::LeaseId{102};
    lease.access_mode = timenav::ClaimAccessMode::Exclusive;
    lease.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Edge, edge_id});
    lease.expires_at_tick = 20;
    manager.add_lease(lease);

    timenav::ClaimRequest conflicting_zone{};
    conflicting_zone.id = timenav::ClaimId{103};
    conflicting_zone.access_mode = timenav::ClaimAccessMode::Exclusive;
    conflicting_zone.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Zone, zone_id});

    timenav::ClaimRequest conflicting_edge{};
    conflicting_edge.id = timenav::ClaimId{104};
    conflicting_edge.access_mode = timenav::ClaimAccessMode::Exclusive;
    conflicting_edge.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Edge, edge_id});

    timenav::ClaimRequest non_conflicting{};
    non_conflicting.id = timenav::ClaimId{105};
    non_conflicting.access_mode = timenav::ClaimAccessMode::Shared;
    non_conflicting.targets.push_back(
        timenav::ClaimTarget{timenav::ClaimTargetKind::Node, zoneout::UUID("e0e0e0e0-e0e0-40e0-80e0-e0e0e0e0e0e0")});

    CHECK(manager.evaluate_request(conflicting_zone).decision == timenav::ClaimDecision::Deny);
    CHECK(manager.evaluate_request(conflicting_edge).decision == timenav::ClaimDecision::Deny);
    CHECK(manager.evaluate_request(non_conflicting).decision == timenav::ClaimDecision::Grant);

    CHECK(manager.release_lease(timenav::LeaseId{102}));
    CHECK(manager.evaluate_request(conflicting_edge).decision == timenav::ClaimDecision::Grant);

    manager.add_lease(lease);
    CHECK(manager.expire_leases(25) == 1);
    CHECK(manager.evaluate_request(conflicting_edge).decision == timenav::ClaimDecision::Grant);
}

TEST_CASE("graph traversal adapter exposes graph neighbors by uuid") {
    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};
    const timenav::GraphTraversalAdapter adapter{index};

    const auto node_a_neighbors = adapter.neighbors(fixture.node_a_id);
    REQUIRE(node_a_neighbors.size() == 1);
    CHECK(node_a_neighbors[0].node_id == fixture.node_b_id);
    CHECK(node_a_neighbors[0].edge_id == fixture.edge_ab_id);
    CHECK(node_a_neighbors[0].weight == doctest::Approx(1.0));

    const auto node_b_neighbors = adapter.neighbors(fixture.node_b_id);
    REQUIRE(node_b_neighbors.size() == 2);
    CHECK(node_b_neighbors[0].node_id == fixture.node_a_id);
    CHECK(node_b_neighbors[1].node_id == fixture.node_c_id);

    CHECK(adapter.neighbors(zoneout::UUID("12121212-1212-4121-8121-121212121212")).empty());
}

TEST_CASE("shortest path search finds a basic route without traffic constraints") {
    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};

    const auto search = timenav::shortest_path_search(index, fixture.node_a_id, fixture.node_c_id);

    CHECK(search.found);
    CHECK(search.distance == doctest::Approx(2.0));
    REQUIRE(search.distances.contains(fixture.node_b_id));
    REQUIRE(search.predecessors.contains(fixture.node_b_id));
    CHECK(search.predecessors.at(fixture.node_b_id) == fixture.node_a_id);
    REQUIRE(search.predecessors.contains(fixture.node_c_id));
    CHECK(search.predecessors.at(fixture.node_c_id) == fixture.node_b_id);
}

TEST_CASE("route reconstruction builds an ordered node sequence from predecessors") {
    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};
    const auto search = timenav::shortest_path_search(index, fixture.node_a_id, fixture.node_c_id);

    const auto route_nodes = timenav::reconstruct_route_nodes(search, fixture.node_a_id, fixture.node_c_id);

    REQUIRE(route_nodes.size() == 3);
    CHECK(route_nodes[0] == fixture.node_a_id);
    CHECK(route_nodes[1] == fixture.node_b_id);
    CHECK(route_nodes[2] == fixture.node_c_id);
}

TEST_CASE("route cost accumulation sums traversed edge weights") {
    auto fixture = make_test_workspace();
    const auto edge_ab = fixture.workspace.find_edge(fixture.edge_ab_id);
    const auto edge_bc = fixture.workspace.find_edge(fixture.edge_bc_id);
    REQUIRE(edge_ab.has_value());
    REQUIRE(edge_bc.has_value());
    fixture.workspace.graph().set_weight(*edge_ab, 1.25);
    fixture.workspace.graph().set_weight(*edge_bc, 2.75);

    const timenav::WorkspaceIndex index{fixture.workspace};
    dp::Vector<zoneout::UUID> route_nodes;
    route_nodes.push_back(fixture.node_a_id);
    route_nodes.push_back(fixture.node_b_id);
    route_nodes.push_back(fixture.node_c_id);

    const auto route_cost = timenav::accumulate_route_cost(index, route_nodes);

    REQUIRE(route_cost.is_ok());
    CHECK(route_cost.value() == doctest::Approx(4.0));
}

TEST_CASE("edge blocking excludes routes through blocked zone or edge policy") {
    auto fixture = make_route_choice_workspace();
    const auto edge_ab = fixture.workspace.find_edge(fixture.edge_ab_id);
    const auto edge_bd = fixture.workspace.find_edge(fixture.edge_bd_id);
    const auto edge_ac = fixture.workspace.find_edge(fixture.edge_ac_id);
    const auto edge_cd = fixture.workspace.find_edge(fixture.edge_cd_id);
    REQUIRE(edge_ab.has_value());
    REQUIRE(edge_bd.has_value());
    REQUIRE(edge_ac.has_value());
    REQUIRE(edge_cd.has_value());

    fixture.workspace.graph().set_weight(*edge_ab, 1.0);
    fixture.workspace.graph().set_weight(*edge_bd, 1.0);
    fixture.workspace.graph().set_weight(*edge_ac, 2.0);
    fixture.workspace.graph().set_weight(*edge_cd, 2.0);

    const timenav::WorkspaceIndex index{fixture.workspace};

    CHECK(timenav::is_edge_hard_blocked(index, fixture.edge_bd_id));
    CHECK_FALSE(timenav::is_edge_hard_blocked(index, fixture.edge_cd_id));

    const auto search = timenav::shortest_path_search_with_blocking(index, fixture.node_a_id, fixture.node_d_id);
    const auto route_nodes = timenav::reconstruct_route_nodes(search, fixture.node_a_id, fixture.node_d_id);

    CHECK(search.found);
    CHECK(search.distance == doctest::Approx(4.0));
    REQUIRE(route_nodes.size() == 3);
    CHECK(route_nodes[0] == fixture.node_a_id);
    CHECK(route_nodes[1] == fixture.node_c_id);
    CHECK(route_nodes[2] == fixture.node_d_id);
}

TEST_CASE("planner penalties prefer lower-risk routes over lower raw weight routes") {
    const auto fixture = make_penalized_route_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};

    const auto blocked_penalty = timenav::edge_traversal_penalty(index, fixture.edge_bd_id);
    const auto biased_penalty = timenav::edge_traversal_penalty(index, fixture.edge_ab_id);
    const auto neutral_penalty = timenav::edge_traversal_penalty(index, fixture.edge_cd_id);
    const auto search = timenav::shortest_path_search_with_penalties(index, fixture.node_a_id, fixture.node_d_id);
    const auto route_nodes = timenav::reconstruct_route_nodes(search, fixture.node_a_id, fixture.node_d_id);

    CHECK(blocked_penalty > biased_penalty);
    CHECK(biased_penalty > neutral_penalty);
    CHECK(search.found);
    REQUIRE(route_nodes.size() == 3);
    CHECK(route_nodes[0] == fixture.node_a_id);
    CHECK(route_nodes[1] == fixture.node_c_id);
    CHECK(route_nodes[2] == fixture.node_d_id);
}

TEST_CASE("route extraction builds traversed nodes edges zones and steps") {
    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};

    dp::Vector<zoneout::UUID> route_nodes;
    route_nodes.push_back(fixture.node_a_id);
    route_nodes.push_back(fixture.node_b_id);
    route_nodes.push_back(fixture.node_c_id);

    const auto traversed_edges = timenav::extract_traversed_edge_ids(index, route_nodes);
    const auto traversed_zones = timenav::extract_traversed_zone_ids(index, route_nodes);
    const auto route_plan = timenav::build_route_plan(index, fixture.node_a_id, fixture.node_c_id, route_nodes);

    REQUIRE(traversed_edges.is_ok());
    REQUIRE(traversed_zones.is_ok());
    REQUIRE(route_plan.is_ok());
    REQUIRE(traversed_edges.value().size() == 2);
    CHECK(traversed_edges.value()[0] == fixture.edge_ab_id);
    CHECK(traversed_edges.value()[1] == fixture.edge_bc_id);
    CHECK(route_plan.value().start_node_id == fixture.node_a_id);
    CHECK(route_plan.value().goal_node_id == fixture.node_c_id);
    REQUIRE(route_plan.value().steps.size() == 3);
    CHECK_FALSE(route_plan.value().steps[0].incoming_edge_id.has_value());
    REQUIRE(route_plan.value().steps[1].incoming_edge_id.has_value());
    CHECK(route_plan.value().steps[1].incoming_edge_id.value() == fixture.edge_ab_id);
    REQUIRE(route_plan.value().steps[2].incoming_edge_id.has_value());
    CHECK(route_plan.value().steps[2].incoming_edge_id.value() == fixture.edge_bc_id);
    CHECK(route_plan.value().traversed_node_ids == route_nodes);
    CHECK(route_plan.value().traversed_edge_ids == traversed_edges.value());
    CHECK(route_plan.value().traversed_zone_ids.size() >= 1);
    CHECK(route_plan.value().total_cost == doctest::Approx(2.0));
}

TEST_CASE("planner failure reporting distinguishes unreachable and policy-blocked routes") {
    {
        const auto fixture = make_blocked_only_route_workspace();
        const timenav::WorkspaceIndex index{fixture.workspace};

        const auto result = timenav::plan_route(index, fixture.node_a_id, fixture.node_d_id, false);

        CHECK_FALSE(result.plan.has_value());
        REQUIRE(result.failure.has_value());
        CHECK(result.failure->kind == timenav::RouteFailureKind::PolicyBlocked);
        CHECK_FALSE(result.failure->blocked_edge_ids.empty());
    }

    {
        const auto fixture = make_unreachable_test_workspace();
        const timenav::WorkspaceIndex index{fixture.workspace};

        const auto result = timenav::plan_route(index, fixture.node_a_id, fixture.node_c_id, false);

        CHECK_FALSE(result.plan.has_value());
        REQUIRE(result.failure.has_value());
        CHECK(result.failure->kind == timenav::RouteFailureKind::Unreachable);
        CHECK(result.failure->blocked_edge_ids.empty());
    }
}

TEST_CASE("planner regression covers blocked and allowed alternative routes") {
    {
        const auto fixture = make_route_choice_workspace();
        const timenav::WorkspaceIndex index{fixture.workspace};

        const auto result = timenav::plan_route(index, fixture.node_a_id, fixture.node_d_id, false);

        REQUIRE(result.plan.has_value());
        CHECK_FALSE(result.failure.has_value());
        CHECK(result.plan->traversed_node_ids.size() == 3);
        CHECK(result.plan->traversed_node_ids[1] == fixture.node_c_id);
        CHECK(result.plan->traversed_edge_ids.size() == 2);
    }

    {
        const auto fixture = make_penalized_route_workspace();
        const timenav::WorkspaceIndex index{fixture.workspace};

        const auto result = timenav::plan_route(index, fixture.node_a_id, fixture.node_d_id, true);

        REQUIRE(result.plan.has_value());
        CHECK_FALSE(result.failure.has_value());
        CHECK(result.plan->traversed_node_ids.size() == 3);
        CHECK(result.plan->traversed_node_ids[1] == fixture.node_c_id);
        CHECK(result.plan->total_cost == doctest::Approx(4.0));
    }
}

TEST_CASE("zone policy exposes typed defaults") {
    const timenav::ZonePolicy policy{};

    CHECK(policy.kind == timenav::ZonePolicyKind::Informational);
    CHECK(policy.capacity == 1);
    CHECK_FALSE(policy.requires_claim);
    CHECK_FALSE(policy.blocks_traversal_without_grant);
    CHECK_FALSE(policy.blocks_entry_without_grant);
    CHECK(policy.properties.empty());
}

TEST_CASE("edge traffic semantics exposes typed defaults") {
    const timenav::EdgeTrafficSemantics semantics{};

    CHECK_FALSE(semantics.directed);
    CHECK_FALSE(semantics.speed_limit.has_value());
    CHECK_FALSE(semantics.lane_type.has_value());
    CHECK_FALSE(semantics.reversible.has_value());
    CHECK_FALSE(semantics.passing_allowed.has_value());
    CHECK_FALSE(semantics.priority.has_value());
    CHECK_FALSE(semantics.capacity.has_value());
    CHECK_FALSE(semantics.no_stop.has_value());
    CHECK_FALSE(semantics.preferred_direction.has_value());
    CHECK(semantics.properties.empty());
}

TEST_CASE("zone policy parser reads known zone traffic keys") {
    const std::unordered_map<std::string, std::string> properties = {
        {"traffic.policy", "exclusive"},
        {"traffic.capacity", "3"},
        {"traffic.claim_required", "true"},
        {"traffic.priority", "7.5"},
        {"traffic.speed_limit", "1.2"},
        {"traffic.waiting_allowed", "false"},
        {"traffic.stop_allowed", "true"},
        {"traffic.entry_rule", "badge_check"},
        {"traffic.exit_rule", "gate_release"},
        {"traffic.robot_class", "forklift"},
        {"traffic.schedule_window", "weekday_day"},
        {"traffic.access_group", "ops"},
    };

    const auto policy = timenav::parse_zone_policy(properties);

    CHECK(policy.kind == timenav::ZonePolicyKind::ExclusiveAccess);
    CHECK(policy.capacity == 3);
    CHECK(policy.requires_claim);
    CHECK(policy.blocks_entry_without_grant);
    REQUIRE(policy.priority.has_value());
    CHECK(policy.priority.value() == doctest::Approx(7.5));
    REQUIRE(policy.speed_limit.has_value());
    CHECK(policy.speed_limit.value() == doctest::Approx(1.2));
    REQUIRE(policy.waiting_allowed.has_value());
    CHECK_FALSE(policy.waiting_allowed.value());
    REQUIRE(policy.stop_allowed.has_value());
    CHECK(policy.stop_allowed.value());
    REQUIRE(policy.entry_rule.has_value());
    CHECK(policy.entry_rule.value() == "badge_check");
    REQUIRE(policy.exit_rule.has_value());
    CHECK(policy.exit_rule.value() == "gate_release");
    REQUIRE(policy.robot_class.has_value());
    CHECK(policy.robot_class.value() == "forklift");
    REQUIRE(policy.schedule_window.has_value());
    CHECK(policy.schedule_window.value() == "weekday_day");
    REQUIRE(policy.access_group.has_value());
    CHECK(policy.access_group.value() == "ops");
    CHECK(policy.properties.size() == properties.size());
}

TEST_CASE("edge traffic semantics parser reads known edge traffic keys") {
    const std::unordered_map<std::string, std::string> properties = {
        {"traffic.speed_limit", "2.5"},
        {"traffic.lane_type", "corridor"},
        {"traffic.reversible", "true"},
        {"traffic.passing_allowed", "false"},
        {"traffic.priority", "4.0"},
        {"traffic.capacity", "2"},
        {"traffic.clearance_width", "1.4"},
        {"traffic.clearance_height", "2.1"},
        {"traffic.surface_type", "concrete"},
        {"traffic.robot_class", "tow"},
        {"traffic.allowed_payload", "light"},
        {"traffic.cost_bias", "0.8"},
        {"traffic.no_stop", "true"},
        {"traffic.preferred_direction", "eastbound"},
    };

    const auto semantics = timenav::parse_edge_traffic_semantics(properties, true);

    CHECK(semantics.directed);
    REQUIRE(semantics.speed_limit.has_value());
    CHECK(semantics.speed_limit.value() == doctest::Approx(2.5));
    REQUIRE(semantics.lane_type.has_value());
    CHECK(semantics.lane_type.value() == "corridor");
    REQUIRE(semantics.reversible.has_value());
    CHECK(semantics.reversible.value());
    REQUIRE(semantics.passing_allowed.has_value());
    CHECK_FALSE(semantics.passing_allowed.value());
    REQUIRE(semantics.priority.has_value());
    CHECK(semantics.priority.value() == doctest::Approx(4.0));
    REQUIRE(semantics.capacity.has_value());
    CHECK(semantics.capacity.value() == 2);
    REQUIRE(semantics.clearance_width.has_value());
    CHECK(semantics.clearance_width.value() == doctest::Approx(1.4));
    REQUIRE(semantics.clearance_height.has_value());
    CHECK(semantics.clearance_height.value() == doctest::Approx(2.1));
    REQUIRE(semantics.surface_type.has_value());
    CHECK(semantics.surface_type.value() == "concrete");
    REQUIRE(semantics.robot_class.has_value());
    CHECK(semantics.robot_class.value() == "tow");
    REQUIRE(semantics.allowed_payload.has_value());
    CHECK(semantics.allowed_payload.value() == "light");
    REQUIRE(semantics.cost_bias.has_value());
    CHECK(semantics.cost_bias.value() == doctest::Approx(0.8));
    REQUIRE(semantics.no_stop.has_value());
    CHECK(semantics.no_stop.value());
    REQUIRE(semantics.preferred_direction.has_value());
    CHECK(semantics.preferred_direction.value() == "eastbound");
    CHECK(semantics.properties.size() == properties.size());
}

TEST_CASE("traffic parsing utilities parse booleans numbers and strings") {
    const auto bool_value = timenav::parse_traffic_bool("true");
    REQUIRE(bool_value.is_ok());
    CHECK(bool_value.value());

    const auto integer_value = timenav::parse_traffic_u64("42");
    REQUIRE(integer_value.is_ok());
    CHECK(integer_value.value() == 42);

    const auto number_value = timenav::parse_traffic_f64("2.75");
    REQUIRE(number_value.is_ok());
    CHECK(number_value.value() == doctest::Approx(2.75));

    const auto string_value = timenav::parse_traffic_string("corridor");
    REQUIRE(string_value.is_ok());
    CHECK(string_value.value() == "corridor");

    CHECK(timenav::parse_traffic_bool("sometimes").is_err());
    CHECK(timenav::parse_traffic_u64("forty-two").is_err());
    CHECK(timenav::parse_traffic_f64("fast").is_err());
    CHECK(timenav::parse_traffic_string("").is_err());
}

TEST_CASE("traffic property validation reports malformed values") {
    const std::unordered_map<std::string, std::string> bad_zone_properties = {
        {"traffic.policy", "mystery"},
        {"traffic.capacity", "0"},
        {"traffic.speed_limit", "-1.0"},
        {"traffic.claim_required", "sometimes"},
    };
    const auto zone_issues = timenav::validate_zone_traffic_properties(bad_zone_properties);

    REQUIRE(zone_issues.size() == 4);
    dp::u64 zone_warnings = 0;
    dp::u64 zone_errors = 0;
    for (const auto &issue : zone_issues) {
        if (issue.severity == timenav::TrafficIssueSeverity::Warning) {
            ++zone_warnings;
        } else if (issue.severity == timenav::TrafficIssueSeverity::Error) {
            ++zone_errors;
        }
    }
    CHECK(zone_warnings == 1);
    CHECK(zone_errors == 3);

    const std::unordered_map<std::string, std::string> bad_edge_properties = {
        {"traffic.capacity", "0"},
        {"traffic.speed_limit", "slow"},
        {"traffic.clearance_width", "-2.0"},
        {"traffic.no_stop", "later"},
    };
    const auto edge_issues = timenav::validate_edge_traffic_properties(bad_edge_properties);

    REQUIRE(edge_issues.size() == 4);
    for (const auto &issue : edge_issues) {
        CHECK(issue.severity == timenav::TrafficIssueSeverity::Error);
    }
}

TEST_CASE("zone policy merge rules honor dominance capacity and child overrides") {
    timenav::ZonePolicy parent{};
    parent.kind = timenav::ZonePolicyKind::ExclusiveAccess;
    parent.capacity = 5;
    parent.requires_claim = true;
    parent.speed_limit = 1.0;
    parent.waiting_allowed = true;
    parent.properties["traffic.policy"] = "exclusive";

    timenav::ZonePolicy child{};
    child.kind = timenav::ZonePolicyKind::SharedAccess;
    child.capacity = 2;
    child.speed_limit = 0.5;
    child.waiting_allowed = false;
    child.entry_rule = dp::String{"badge"};
    child.properties["traffic.entry_rule"] = "badge";

    const auto merged = timenav::merge_zone_policy(parent, child);

    CHECK(merged.kind == timenav::ZonePolicyKind::ExclusiveAccess);
    CHECK(merged.capacity == 2);
    CHECK(merged.requires_claim);
    REQUIRE(merged.speed_limit.has_value());
    CHECK(merged.speed_limit.value() == doctest::Approx(0.5));
    REQUIRE(merged.waiting_allowed.has_value());
    CHECK_FALSE(merged.waiting_allowed.value());
    REQUIRE(merged.entry_rule.has_value());
    CHECK(merged.entry_rule.value() == "badge");
    const auto policy_it = merged.properties.find("traffic.policy");
    REQUIRE(policy_it != merged.properties.end());
    CHECK(policy_it->second == "exclusive");
    const auto entry_rule_it = merged.properties.find("traffic.entry_rule");
    REQUIRE(entry_rule_it != merged.properties.end());
    CHECK(entry_rule_it->second == "badge");

    timenav::ZonePolicy restricted_child{};
    restricted_child.kind = timenav::ZonePolicyKind::Restricted;
    const auto restricted = timenav::merge_zone_policy(parent, restricted_child);
    CHECK(restricted.kind == timenav::ZonePolicyKind::Restricted);
    CHECK(restricted.blocks_entry_without_grant);
    CHECK(restricted.blocks_traversal_without_grant);
}

TEST_CASE("effective edge semantics combine structural and zone-derived rules") {
    const std::unordered_map<std::string, std::string> edge_properties = {
        {"traffic.speed_limit", "2.5"},
        {"traffic.capacity", "3"},
        {"traffic.robot_class", "tow"},
        {"traffic.no_stop", "false"},
    };

    timenav::ZonePolicy zone_a{};
    zone_a.speed_limit = 1.0;
    zone_a.capacity = 2;

    timenav::ZonePolicy zone_b{};
    zone_b.kind = timenav::ZonePolicyKind::NoStop;
    zone_b.robot_class = dp::String{"forklift"};

    dp::Vector<timenav::ZonePolicy> zone_policies;
    zone_policies.push_back(zone_a);
    zone_policies.push_back(zone_b);

    const auto semantics = timenav::derive_effective_edge_semantics(edge_properties, true, zone_policies);

    CHECK(semantics.directed);
    REQUIRE(semantics.speed_limit.has_value());
    CHECK(semantics.speed_limit.value() == doctest::Approx(1.0));
    REQUIRE(semantics.capacity.has_value());
    CHECK(semantics.capacity.value() == 2);
    REQUIRE(semantics.robot_class.has_value());
    CHECK(semantics.robot_class.value() == "tow");
    REQUIRE(semantics.no_stop.has_value());
    CHECK(semantics.no_stop.value());
}

TEST_CASE("policy layer regression covers parsing validation inheritance and derivation") {
    const std::unordered_map<std::string, std::string> parent_zone_properties = {
        {"traffic.policy", "exclusive"},
        {"traffic.capacity", "4"},
        {"traffic.speed_limit", "1.5"},
        {"traffic.claim_required", "true"},
    };
    const std::unordered_map<std::string, std::string> child_zone_properties = {
        {"traffic.policy", "shared"},
        {"traffic.capacity", "2"},
        {"traffic.waiting_allowed", "false"},
        {"traffic.entry_rule", "child_gate"},
    };
    const std::unordered_map<std::string, std::string> edge_properties = {
        {"traffic.speed_limit", "2.5"},
        {"traffic.capacity", "3"},
        {"traffic.no_stop", "false"},
    };
    const std::unordered_map<std::string, std::string> bad_zone_properties = {
        {"traffic.policy", "unknown"},
        {"traffic.capacity", "invalid"},
        {"traffic.stop_allowed", "later"},
    };

    const auto parent_policy = timenav::parse_zone_policy(parent_zone_properties);
    const auto child_policy = timenav::parse_zone_policy(child_zone_properties);
    const auto merged_policy = timenav::merge_zone_policy(parent_policy, child_policy);
    const auto bad_zone_issues = timenav::validate_zone_traffic_properties(bad_zone_properties);

    dp::Vector<timenav::ZonePolicy> containing_policies;
    containing_policies.push_back(parent_policy);
    containing_policies.push_back(merged_policy);
    const auto effective_edge = timenav::derive_effective_edge_semantics(edge_properties, false, containing_policies);

    CHECK(parent_policy.kind == timenav::ZonePolicyKind::ExclusiveAccess);
    CHECK(child_policy.kind == timenav::ZonePolicyKind::SharedAccess);
    CHECK(merged_policy.kind == timenav::ZonePolicyKind::ExclusiveAccess);
    CHECK(merged_policy.capacity == 2);
    REQUIRE(merged_policy.waiting_allowed.has_value());
    CHECK_FALSE(merged_policy.waiting_allowed.value());
    REQUIRE(merged_policy.entry_rule.has_value());
    CHECK(merged_policy.entry_rule.value() == "child_gate");

    REQUIRE(bad_zone_issues.size() == 3);
    const bool first_issue_is_supported = bad_zone_issues[0].severity == timenav::TrafficIssueSeverity::Error ||
                                          bad_zone_issues[0].severity == timenav::TrafficIssueSeverity::Warning;
    CHECK(first_issue_is_supported);

    CHECK_FALSE(effective_edge.directed);
    REQUIRE(effective_edge.speed_limit.has_value());
    CHECK(effective_edge.speed_limit.value() == doctest::Approx(1.5));
    REQUIRE(effective_edge.capacity.has_value());
    CHECK(effective_edge.capacity.value() == 2);
    REQUIRE(effective_edge.no_stop.has_value());
    CHECK_FALSE(effective_edge.no_stop.value());
}

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

TEST_CASE("workspace index can borrow or own a workspace and expose the root zone") {
    auto fixture = make_test_workspace();
    auto &workspace = fixture.workspace;
    const auto root_id = workspace.root_zone().id();

    const timenav::WorkspaceIndex borrowed_index{workspace};
    CHECK(borrowed_index.has_workspace());
    CHECK_FALSE(borrowed_index.owns_workspace());
    REQUIRE(borrowed_index.workspace() != nullptr);
    REQUIRE(borrowed_index.root_zone() != nullptr);
    CHECK(borrowed_index.root_zone()->id() == root_id);
    CHECK(borrowed_index.root_zone_id().value() == root_id);

    auto owned_workspace = std::make_shared<zoneout::Workspace>(make_test_workspace().workspace);
    const auto owned_root_id = owned_workspace->root_zone().id();

    const timenav::WorkspaceIndex owned_index{owned_workspace};
    CHECK(owned_index.has_workspace());
    CHECK(owned_index.owns_workspace());
    REQUIRE(owned_index.workspace() != nullptr);
    REQUIRE(owned_index.root_zone() != nullptr);
    CHECK(owned_index.root_zone()->id() == owned_root_id);
    CHECK(owned_index.root_zone_id().value() == owned_root_id);
}

TEST_CASE("workspace index resolves zones by uuid") {
    const auto fixture = make_test_workspace();
    const auto &workspace = fixture.workspace;
    const auto &root = workspace.root_zone();
    REQUIRE(root.child_count() == 2);

    const auto &child_a = root.children().at(0);
    const auto &child_b = root.children().at(1);
    const auto &grandchild = child_a.children().at(0);

    const timenav::WorkspaceIndex index{workspace};

    REQUIRE(index.zone(root.id()) != nullptr);
    CHECK(index.zone(root.id())->name() == "root");
    REQUIRE(index.zone(child_a.id()) != nullptr);
    CHECK(index.zone(child_a.id())->name() == "child-a");
    REQUIRE(index.zone(child_b.id()) != nullptr);
    CHECK(index.zone(child_b.id())->name() == "child-b");
    REQUIRE(index.zone(grandchild.id()) != nullptr);
    CHECK(index.zone(grandchild.id())->name() == "grandchild-a1");
    CHECK(index.zone(zoneout::UUID{}) == nullptr);
}

TEST_CASE("workspace index resolves nodes by uuid") {
    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};

    REQUIRE(index.node(fixture.node_a_id) != nullptr);
    CHECK(index.node(fixture.node_a_id)->position == dp::Point{20.0, 20.0, 0.0});
    REQUIRE(index.node(fixture.node_b_id) != nullptr);
    CHECK(index.node(fixture.node_b_id)->position == dp::Point{30.0, 20.0, 0.0});
    REQUIRE(index.node(fixture.node_c_id) != nullptr);
    CHECK(index.node(fixture.node_c_id)->position == dp::Point{70.0, 20.0, 0.0});
    CHECK(index.node(zoneout::UUID("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa")) == nullptr);
}

TEST_CASE("workspace index resolves edges by uuid") {
    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};

    REQUIRE(index.edge(fixture.edge_ab_id) != nullptr);
    CHECK(index.edge(fixture.edge_ab_id)->id == fixture.edge_ab_id);
    REQUIRE(index.edge(fixture.edge_bc_id) != nullptr);
    CHECK(index.edge(fixture.edge_bc_id)->id == fixture.edge_bc_id);
    CHECK(index.edge(zoneout::UUID("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb")) == nullptr);
}

TEST_CASE("workspace index resolves parent and child zones") {
    const auto fixture = make_test_workspace();
    const auto &root = fixture.workspace.root_zone();
    const auto &child_a = root.children().at(0);
    const auto &child_b = root.children().at(1);
    const auto &grandchild = child_a.children().at(0);

    const timenav::WorkspaceIndex index{fixture.workspace};

    CHECK(index.parent_zone(root.id()) == nullptr);
    REQUIRE(index.parent_zone(child_a.id()) != nullptr);
    CHECK(index.parent_zone(child_a.id())->id() == root.id());
    REQUIRE(index.parent_zone(child_b.id()) != nullptr);
    CHECK(index.parent_zone(child_b.id())->id() == root.id());
    REQUIRE(index.parent_zone(grandchild.id()) != nullptr);
    CHECK(index.parent_zone(grandchild.id())->id() == child_a.id());

    const auto root_children = index.child_zones(root.id());
    REQUIRE(root_children.size() == 2);
    CHECK(root_children[0]->id() == child_a.id());
    CHECK(root_children[1]->id() == child_b.id());

    const auto child_a_children = index.child_zones(child_a.id());
    REQUIRE(child_a_children.size() == 1);
    CHECK(child_a_children[0]->id() == grandchild.id());
    CHECK(index.child_zones(child_b.id()).empty());
}

TEST_CASE("workspace index resolves ancestor and descendant zones") {
    const auto fixture = make_test_workspace();
    const auto &root = fixture.workspace.root_zone();
    const auto &child_a = root.children().at(0);
    const auto &child_b = root.children().at(1);
    const auto &grandchild = child_a.children().at(0);

    const timenav::WorkspaceIndex index{fixture.workspace};

    const auto grandchild_ancestors = index.ancestor_zones(grandchild.id());
    REQUIRE(grandchild_ancestors.size() == 2);
    CHECK(grandchild_ancestors[0]->id() == child_a.id());
    CHECK(grandchild_ancestors[1]->id() == root.id());

    const auto child_a_ancestors = index.ancestor_zones(child_a.id());
    REQUIRE(child_a_ancestors.size() == 1);
    CHECK(child_a_ancestors[0]->id() == root.id());
    CHECK(index.ancestor_zones(root.id()).empty());

    const auto root_descendants = index.descendant_zones(root.id());
    REQUIRE(root_descendants.size() == 3);
    CHECK(root_descendants[0]->id() == child_a.id());
    CHECK(root_descendants[1]->id() == grandchild.id());
    CHECK(root_descendants[2]->id() == child_b.id());

    const auto child_a_descendants = index.descendant_zones(child_a.id());
    REQUIRE(child_a_descendants.size() == 1);
    CHECK(child_a_descendants[0]->id() == grandchild.id());
    CHECK(index.descendant_zones(child_b.id()).empty());
}

TEST_CASE("workspace index supports basic uuid resolution across zones nodes and edges") {
    const auto fixture = make_test_workspace();
    const auto &workspace = fixture.workspace;
    const auto &root = workspace.root_zone();
    const auto &child_a = root.children().at(0);
    const auto &child_b = root.children().at(1);
    const auto &grandchild = child_a.children().at(0);

    const timenav::WorkspaceIndex index{workspace};

    REQUIRE(index.zone(root.id()) != nullptr);
    REQUIRE(index.zone(child_a.id()) != nullptr);
    REQUIRE(index.zone(child_b.id()) != nullptr);
    REQUIRE(index.zone(grandchild.id()) != nullptr);
    REQUIRE(index.node(fixture.node_a_id) != nullptr);
    REQUIRE(index.node(fixture.node_b_id) != nullptr);
    REQUIRE(index.node(fixture.node_c_id) != nullptr);
    REQUIRE(index.edge(fixture.edge_ab_id) != nullptr);
    REQUIRE(index.edge(fixture.edge_bc_id) != nullptr);

    CHECK(index.node(fixture.node_a_id)->zone_ids.size() == 2);
    CHECK(index.node(fixture.node_b_id)->zone_ids.size() == 2);
    CHECK(index.node(fixture.node_c_id)->zone_ids.size() == 2);
    CHECK(index.edge(fixture.edge_ab_id)->zone_ids.size() == 2);
    CHECK(index.edge(fixture.edge_bc_id)->zone_ids.size() == 3);

    const auto root_descendants = index.descendant_zones(root.id());
    CHECK(root_descendants.size() == 3);
    CHECK(index.parent_zone(grandchild.id())->id() == child_a.id());
    CHECK(index.ancestor_zones(grandchild.id()).back()->id() == root.id());
}

TEST_CASE("workspace index lists nodes in each zone") {
    const auto fixture = make_test_workspace();
    const auto &root = fixture.workspace.root_zone();
    const auto &child_a = root.children().at(0);
    const auto &child_b = root.children().at(1);
    const auto &grandchild = child_a.children().at(0);

    const timenav::WorkspaceIndex index{fixture.workspace};

    const auto root_nodes = index.nodes_in_zone(root.id());
    REQUIRE(root_nodes.size() == 3);
    CHECK(root_nodes[0]->id == fixture.node_a_id);
    CHECK(root_nodes[1]->id == fixture.node_b_id);
    CHECK(root_nodes[2]->id == fixture.node_c_id);

    const auto child_a_nodes = index.nodes_in_zone(child_a.id());
    REQUIRE(child_a_nodes.size() == 2);
    CHECK(child_a_nodes[0]->id == fixture.node_a_id);
    CHECK(child_a_nodes[1]->id == fixture.node_b_id);

    const auto child_b_nodes = index.nodes_in_zone(child_b.id());
    REQUIRE(child_b_nodes.size() == 1);
    CHECK(child_b_nodes[0]->id == fixture.node_c_id);

    CHECK(index.nodes_in_zone(grandchild.id()).empty());
}

TEST_CASE("workspace index lists zones for each node") {
    const auto fixture = make_test_workspace();
    const auto &root = fixture.workspace.root_zone();
    const auto &child_a = root.children().at(0);
    const auto &child_b = root.children().at(1);

    const timenav::WorkspaceIndex index{fixture.workspace};

    const auto node_a_zones = index.zones_of_node(fixture.node_a_id);
    REQUIRE(node_a_zones.size() == 2);
    CHECK(node_a_zones[0]->id() == root.id());
    CHECK(node_a_zones[1]->id() == child_a.id());

    const auto node_b_zones = index.zones_of_node(fixture.node_b_id);
    REQUIRE(node_b_zones.size() == 2);
    CHECK(node_b_zones[0]->id() == root.id());
    CHECK(node_b_zones[1]->id() == child_a.id());

    const auto node_c_zones = index.zones_of_node(fixture.node_c_id);
    REQUIRE(node_c_zones.size() == 2);
    CHECK(node_c_zones[0]->id() == root.id());
    CHECK(node_c_zones[1]->id() == child_b.id());

    CHECK(index.zones_of_node(zoneout::UUID("cccccccc-cccc-4ccc-8ccc-cccccccccccc")).empty());
}

TEST_CASE("workspace index lists zones for each edge") {
    const auto fixture = make_test_workspace();
    const auto &root = fixture.workspace.root_zone();
    const auto &child_a = root.children().at(0);
    const auto &child_b = root.children().at(1);

    const timenav::WorkspaceIndex index{fixture.workspace};

    const auto edge_ab_zones = index.zones_of_edge(fixture.edge_ab_id);
    REQUIRE(edge_ab_zones.size() == 2);
    CHECK(edge_ab_zones[0]->id() == root.id());
    CHECK(edge_ab_zones[1]->id() == child_a.id());

    const auto edge_bc_zones = index.zones_of_edge(fixture.edge_bc_id);
    REQUIRE(edge_bc_zones.size() == 3);
    CHECK(edge_bc_zones[0]->id() == root.id());
    CHECK(edge_bc_zones[1]->id() == child_a.id());
    CHECK(edge_bc_zones[2]->id() == child_b.id());

    CHECK(index.zones_of_edge(zoneout::UUID("dddddddd-dddd-4ddd-8ddd-dddddddddddd")).empty());
}

TEST_CASE("workspace index resolves an edge between two nodes") {
    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};

    REQUIRE(index.edge_between(fixture.node_a_id, fixture.node_b_id) != nullptr);
    CHECK(index.edge_between(fixture.node_a_id, fixture.node_b_id)->id == fixture.edge_ab_id);
    REQUIRE(index.edge_between(fixture.node_b_id, fixture.node_c_id) != nullptr);
    CHECK(index.edge_between(fixture.node_b_id, fixture.node_c_id)->id == fixture.edge_bc_id);
    REQUIRE(index.edge_between(fixture.node_b_id, fixture.node_a_id) != nullptr);
    CHECK(index.edge_between(fixture.node_b_id, fixture.node_a_id)->id == fixture.edge_ab_id);
    CHECK(index.edge_between(fixture.node_a_id, fixture.node_c_id) == nullptr);
    CHECK(index.edge_between(fixture.node_a_id, zoneout::UUID("eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee")) == nullptr);
}

TEST_CASE("workspace index exposes the workspace reference origin") {
    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};

    REQUIRE(index.ref().has_value());
    CHECK(index.ref()->latitude == doctest::Approx(52.0));
    CHECK(index.ref()->longitude == doctest::Approx(5.0));
    CHECK(index.ref()->altitude == doctest::Approx(10.0));

    const timenav::WorkspaceIndex empty_index{};
    CHECK_FALSE(empty_index.ref().has_value());
}

TEST_CASE("workspace index exposes the workspace coordinate mode") {
    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};

    REQUIRE(index.coord_mode().has_value());
    CHECK(index.coord_mode().value() == zoneout::CoordMode::Local);

    const timenav::WorkspaceIndex empty_index{};
    CHECK_FALSE(empty_index.coord_mode().has_value());
}

TEST_CASE("workspace index converts local and global coordinates with concord") {
    const auto fixture = make_test_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};

    const auto global_origin = index.local_to_global(dp::Point{0.0, 0.0, 0.0});
    REQUIRE(global_origin.is_ok());
    CHECK(global_origin.value().latitude == doctest::Approx(52.0));
    CHECK(global_origin.value().longitude == doctest::Approx(5.0));
    CHECK(global_origin.value().altitude == doctest::Approx(10.0));

    const auto local_origin = index.global_to_local(dp::Geo{52.0, 5.0, 10.0});
    REQUIRE(local_origin.is_ok());
    CHECK(local_origin.value().x == doctest::Approx(0.0).epsilon(1e-6));
    CHECK(local_origin.value().y == doctest::Approx(0.0).epsilon(1e-6));
    CHECK(local_origin.value().z == doctest::Approx(0.0).epsilon(1e-6));

    const auto round_trip_global = index.local_to_global(dp::Point{5.0, 7.5, 1.0});
    REQUIRE(round_trip_global.is_ok());
    const auto round_trip_local = index.global_to_local(round_trip_global.value());
    REQUIRE(round_trip_local.is_ok());
    CHECK(round_trip_local.value().x == doctest::Approx(5.0).epsilon(1e-6));
    CHECK(round_trip_local.value().y == doctest::Approx(7.5).epsilon(1e-6));
    CHECK(round_trip_local.value().z == doctest::Approx(1.0).epsilon(1e-6));
}

TEST_CASE("workspace index reads zone and edge properties") {
    const auto fixture = make_test_workspace();
    const auto &root = fixture.workspace.root_zone();
    const auto &child_a = root.children().at(0);

    const timenav::WorkspaceIndex index{fixture.workspace};

    REQUIRE(index.zone_property(root.id(), "traffic.mode").has_value());
    CHECK(index.zone_property(root.id(), "traffic.mode").value() == "shared");
    REQUIRE(index.zone_property(child_a.id(), "traffic.speed_limit").has_value());
    CHECK(index.zone_property(child_a.id(), "traffic.speed_limit").value() == "1.2");
    CHECK_FALSE(index.zone_property(child_a.id(), "traffic.unknown").has_value());

    REQUIRE(index.edge_property(fixture.edge_ab_id, "traffic.priority").has_value());
    CHECK(index.edge_property(fixture.edge_ab_id, "traffic.priority").value() == "high");
    REQUIRE(index.edge_property(fixture.edge_bc_id, "traffic.priority").has_value());
    CHECK(index.edge_property(fixture.edge_bc_id, "traffic.priority").value() == "yield");
    CHECK_FALSE(index.edge_property(fixture.edge_bc_id, "traffic.unknown").has_value());
}

TEST_CASE("workspace index reports validation issues for ids memberships and references") {
    const auto invalid_workspace = make_invalid_workspace();
    const timenav::WorkspaceIndex index{invalid_workspace};
    const auto issues = index.validation_issues();

    CHECK_FALSE(index.is_valid());
    CHECK(issues.size() >= 4);

    bool found_missing_id = false;
    bool found_broken_membership = false;
    bool found_invalid_reference = false;

    for (const auto &issue : issues) {
        if (issue.category == "missing_id") {
            found_missing_id = true;
        } else if (issue.category == "broken_membership") {
            found_broken_membership = true;
        } else if (issue.category == "invalid_reference") {
            found_invalid_reference = true;
        }
    }

    CHECK(found_missing_id);
    CHECK(found_broken_membership);
    CHECK(found_invalid_reference);
}

TEST_CASE("workspace index milestone two regression covers memberships hierarchy and coordinate access") {
    const auto fixture = make_test_workspace();
    const auto &root = fixture.workspace.root_zone();
    const auto &child_a = root.children().at(0);
    const auto &child_b = root.children().at(1);
    const auto &grandchild = child_a.children().at(0);

    const timenav::WorkspaceIndex index{fixture.workspace};

    const auto root_nodes = index.nodes_in_zone(root.id());
    const auto child_a_nodes = index.nodes_in_zone(child_a.id());
    const auto child_b_nodes = index.nodes_in_zone(child_b.id());
    const auto node_b_zones = index.zones_of_node(fixture.node_b_id);
    const auto edge_bc_zones = index.zones_of_edge(fixture.edge_bc_id);
    const auto root_descendants = index.descendant_zones(root.id());
    const auto grandchild_ancestors = index.ancestor_zones(grandchild.id());

    REQUIRE(root_nodes.size() == 3);
    REQUIRE(child_a_nodes.size() == 2);
    REQUIRE(child_b_nodes.size() == 1);
    REQUIRE(node_b_zones.size() == 2);
    REQUIRE(edge_bc_zones.size() == 3);
    REQUIRE(root_descendants.size() == 3);
    REQUIRE(grandchild_ancestors.size() == 2);

    CHECK(root_nodes[1]->id == fixture.node_b_id);
    CHECK(child_a_nodes[1]->id == fixture.node_b_id);
    CHECK(child_b_nodes[0]->id == fixture.node_c_id);
    CHECK(node_b_zones[1]->id() == child_a.id());
    CHECK(edge_bc_zones[2]->id() == child_b.id());
    CHECK(root_descendants[1]->id() == grandchild.id());
    CHECK(grandchild_ancestors[1]->id() == root.id());

    const auto node_b_global = index.local_to_global(index.node(fixture.node_b_id)->position);
    REQUIRE(node_b_global.is_ok());
    const auto node_b_round_trip = index.global_to_local(node_b_global.value());
    REQUIRE(node_b_round_trip.is_ok());
    CHECK(node_b_round_trip.value().x == doctest::Approx(30.0).epsilon(1e-6));
    CHECK(node_b_round_trip.value().y == doctest::Approx(20.0).epsilon(1e-6));
    CHECK(node_b_round_trip.value().z == doctest::Approx(0.0).epsilon(1e-6));
}
