#include <doctest/doctest.h>
#include <timenav/timenav.hpp>

#include <stdexcept>

namespace {

dp::Polygon rectangle(double min_x, double min_y, double max_x, double max_y) {
    dp::Polygon polygon;
    polygon.vertices.push_back(dp::Point{min_x, min_y, 0.0});
    polygon.vertices.push_back(dp::Point{max_x, min_y, 0.0});
    polygon.vertices.push_back(dp::Point{max_x, max_y, 0.0});
    polygon.vertices.push_back(dp::Point{min_x, max_y, 0.0});
    return polygon;
}

struct ClaimFixture {
    zoneout::Workspace workspace;
    zoneout::UUID node_a_id;
    zoneout::UUID node_b_id;
    zoneout::UUID node_c_id;
    zoneout::UUID edge_ab_id;
    zoneout::UUID edge_bc_id;
    zoneout::UUID bounded_zone_id;
};

ClaimFixture make_claim_fixture() {
    auto root = zoneout::ZoneBuilder()
                    .with_name("root")
                    .with_type("workspace")
                    .with_boundary(rectangle(0.0, 0.0, 100.0, 100.0))
                    .with_datum(dp::Geo{52.0, 5.0, 0.0})
                    .build();
    auto bounded = zoneout::ZoneBuilder()
                       .with_name("bounded")
                       .with_type("lane")
                       .with_boundary(rectangle(10.0, 10.0, 80.0, 30.0))
                       .with_datum(dp::Geo{52.0, 5.0, 0.0})
                       .with_property("traffic.max_occupancy", "2")
                       .build();
    root.add_child(std::move(bounded));

    ClaimFixture fixture{zoneout::Workspace(std::move(root))};
    fixture.bounded_zone_id = fixture.workspace.root_zone().children()[0].id();
    fixture.node_a_id = zoneout::UUID("11111111-1111-4111-8111-111111111111");
    fixture.node_b_id = zoneout::UUID("22222222-2222-4222-8222-222222222222");
    fixture.node_c_id = zoneout::UUID("33333333-3333-4333-8333-333333333333");
    fixture.edge_ab_id = zoneout::UUID("44444444-4444-4444-8444-444444444444");
    fixture.edge_bc_id = zoneout::UUID("55555555-5555-4555-8555-555555555555");

    const auto a = fixture.workspace.add_node(zoneout::NodeData{fixture.node_a_id, dp::Point{20.0, 20.0, 0.0}});
    const auto b = fixture.workspace.add_node(zoneout::NodeData{fixture.node_b_id, dp::Point{40.0, 20.0, 0.0}});
    const auto c = fixture.workspace.add_node(zoneout::NodeData{fixture.node_c_id, dp::Point{60.0, 20.0, 0.0}});
    fixture.workspace.add_edge(a, b, zoneout::EdgeData{fixture.edge_ab_id});
    fixture.workspace.add_edge(b, c, zoneout::EdgeData{fixture.edge_bc_id});

    auto edge_ab = fixture.workspace.find_edge(fixture.edge_ab_id);
    auto edge_bc = fixture.workspace.find_edge(fixture.edge_bc_id);
    if (!edge_ab.has_value() || !edge_bc.has_value()) {
        throw std::runtime_error("expected claim fixture edges");
    }

    fixture.workspace.graph()[a].zone_ids.push_back(fixture.bounded_zone_id);
    fixture.workspace.graph()[b].zone_ids.push_back(fixture.bounded_zone_id);
    fixture.workspace.graph()[c].zone_ids.push_back(fixture.bounded_zone_id);
    fixture.workspace.graph().edge_property(*edge_ab).zone_ids.push_back(fixture.bounded_zone_id);
    fixture.workspace.graph().edge_property(*edge_bc).zone_ids.push_back(fixture.bounded_zone_id);
    fixture.workspace.root_zone().children()[0].node_ids().push_back(fixture.node_a_id);
    fixture.workspace.root_zone().children()[0].node_ids().push_back(fixture.node_b_id);
    fixture.workspace.root_zone().children()[0].node_ids().push_back(fixture.node_c_id);

    return fixture;
}

} // namespace

TEST_CASE("claim module enforces bounded shared capacity for node claims inside the same zone") {
    const auto fixture = make_claim_fixture();
    const timenav::WorkspaceIndex index{fixture.workspace};
    timenav::ClaimManager manager{index};

    timenav::ClaimRequest first{};
    first.id = timenav::ClaimId{1};
    first.access_mode = timenav::ClaimAccessMode::Shared;
    first.window.start_tick = 0;
    first.window.end_tick = 10;
    first.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Node, fixture.node_a_id});
    manager.add_request(first);

    timenav::ClaimRequest second = first;
    second.id = timenav::ClaimId{2};
    second.targets[0].resource_id = fixture.node_b_id;
    manager.add_request(second);

    timenav::ClaimRequest third = first;
    third.id = timenav::ClaimId{3};
    third.targets[0].resource_id = fixture.node_c_id;

    const auto evaluation = manager.evaluate_request(third);

    CHECK(evaluation.decision == timenav::ClaimDecision::Deny);
    CHECK(evaluation.reason.find("capacity") != dp::String::npos);
    REQUIRE(evaluation.blocking_target.has_value());
    CHECK(evaluation.blocking_target->kind == timenav::ClaimTargetKind::Zone);
}

TEST_CASE("claim module enforces bounded shared capacity for edge claims inside the same zone") {
    const auto fixture = make_claim_fixture();
    const timenav::WorkspaceIndex index{fixture.workspace};
    timenav::ClaimManager manager{index};

    timenav::ClaimRequest first{};
    first.id = timenav::ClaimId{11};
    first.access_mode = timenav::ClaimAccessMode::Shared;
    first.window.start_tick = 0;
    first.window.end_tick = 10;
    first.targets.push_back(timenav::ClaimTarget{timenav::ClaimTargetKind::Edge, fixture.edge_ab_id});
    manager.add_request(first);

    timenav::ClaimRequest second = first;
    second.id = timenav::ClaimId{12};
    second.targets[0].resource_id = fixture.edge_bc_id;
    manager.add_request(second);

    timenav::ClaimRequest third = first;
    third.id = timenav::ClaimId{13};
    third.targets[0].resource_id = fixture.edge_ab_id;

    const auto evaluation = manager.evaluate_request(third);

    CHECK(evaluation.decision == timenav::ClaimDecision::Deny);
    CHECK(evaluation.reason.find("capacity") != dp::String::npos);
    CHECK_FALSE(evaluation.diagnostics.empty());
}
