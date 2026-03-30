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

struct RichRouteWorkspace {
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
    zoneout::UUID restricted_zone_id;
    zoneout::UUID slow_zone_id;
};

RichRouteWorkspace make_rich_route_workspace() {
    auto root = zoneout::ZoneBuilder()
                    .with_name("root")
                    .with_type("workspace")
                    .with_boundary(rectangle(0.0, 0.0, 100.0, 100.0))
                    .with_datum(dp::Geo{52.0, 5.0, 0.0})
                    .build();
    auto blocked = zoneout::ZoneBuilder()
                       .with_name("blocked")
                       .with_type("lane")
                       .with_boundary(rectangle(10.0, 10.0, 30.0, 30.0))
                       .with_datum(dp::Geo{52.0, 5.0, 0.0})
                       .with_property("traffic.blocked", "true")
                       .build();
    auto restricted = zoneout::ZoneBuilder()
                          .with_name("restricted")
                          .with_type("lane")
                          .with_boundary(rectangle(40.0, 10.0, 60.0, 30.0))
                          .with_datum(dp::Geo{52.0, 5.0, 0.0})
                          .with_property("traffic.access_group", "gates")
                          .build();
    auto slow = zoneout::ZoneBuilder()
                    .with_name("slow")
                    .with_type("lane")
                    .with_boundary(rectangle(60.0, 10.0, 90.0, 30.0))
                    .with_datum(dp::Geo{52.0, 5.0, 0.0})
                    .with_property("traffic.mode", "slow")
                    .with_property("traffic.speed_limit", "0.5")
                    .build();
    root.add_child(std::move(blocked));
    root.add_child(std::move(restricted));
    root.add_child(std::move(slow));

    RichRouteWorkspace fixture{zoneout::Workspace(std::move(root))};
    fixture.node_a_id = zoneout::UUID("10000000-0000-4000-8000-000000000001");
    fixture.node_b_id = zoneout::UUID("10000000-0000-4000-8000-000000000002");
    fixture.node_c_id = zoneout::UUID("10000000-0000-4000-8000-000000000003");
    fixture.node_d_id = zoneout::UUID("10000000-0000-4000-8000-000000000004");
    fixture.edge_ab_id = zoneout::UUID("20000000-0000-4000-8000-000000000001");
    fixture.edge_bd_id = zoneout::UUID("20000000-0000-4000-8000-000000000002");
    fixture.edge_ac_id = zoneout::UUID("20000000-0000-4000-8000-000000000003");
    fixture.edge_cd_id = zoneout::UUID("20000000-0000-4000-8000-000000000004");
    fixture.blocked_zone_id = fixture.workspace.root_zone().children()[0].id();
    fixture.restricted_zone_id = fixture.workspace.root_zone().children()[1].id();
    fixture.slow_zone_id = fixture.workspace.root_zone().children()[2].id();

    const auto a = fixture.workspace.add_node(zoneout::NodeData{fixture.node_a_id, dp::Point{5.0, 20.0, 0.0}});
    const auto b = fixture.workspace.add_node(zoneout::NodeData{fixture.node_b_id, dp::Point{20.0, 20.0, 0.0}});
    const auto c = fixture.workspace.add_node(zoneout::NodeData{fixture.node_c_id, dp::Point{50.0, 20.0, 0.0}});
    const auto d = fixture.workspace.add_node(zoneout::NodeData{fixture.node_d_id, dp::Point{80.0, 20.0, 0.0}});

    fixture.workspace.add_edge(a, b, zoneout::EdgeData{fixture.edge_ab_id});
    fixture.workspace.add_edge(b, d, zoneout::EdgeData{fixture.edge_bd_id});
    fixture.workspace.add_edge(a, c, zoneout::EdgeData{fixture.edge_ac_id, {{"traffic.claim_required", "true"}}});
    fixture.workspace.add_edge(c, d, zoneout::EdgeData{fixture.edge_cd_id, {{"traffic.speed_limit", "0.5"}}});

    auto edge_ab = fixture.workspace.find_edge(fixture.edge_ab_id);
    auto edge_bd = fixture.workspace.find_edge(fixture.edge_bd_id);
    auto edge_ac = fixture.workspace.find_edge(fixture.edge_ac_id);
    auto edge_cd = fixture.workspace.find_edge(fixture.edge_cd_id);
    if (!edge_ab.has_value() || !edge_bd.has_value() || !edge_ac.has_value() || !edge_cd.has_value()) {
        throw std::runtime_error("expected route fixture edges");
    }

    fixture.workspace.graph()[a].zone_ids.push_back(fixture.blocked_zone_id);
    fixture.workspace.graph()[b].zone_ids.push_back(fixture.blocked_zone_id);
    fixture.workspace.graph()[c].zone_ids.push_back(fixture.restricted_zone_id);
    fixture.workspace.graph()[d].zone_ids.push_back(fixture.slow_zone_id);
    fixture.workspace.graph().edge_property(*edge_ab).zone_ids.push_back(fixture.blocked_zone_id);
    fixture.workspace.graph().edge_property(*edge_bd).zone_ids.push_back(fixture.blocked_zone_id);
    fixture.workspace.graph().edge_property(*edge_ac).zone_ids.push_back(fixture.restricted_zone_id);
    fixture.workspace.graph().edge_property(*edge_cd).zone_ids.push_back(fixture.slow_zone_id);

    return fixture;
}

} // namespace

TEST_CASE("route module distinguishes blocked resources from restricted and slow semantics") {
    const auto fixture = make_rich_route_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};

    const auto result = timenav::plan_route(index, fixture.node_a_id, fixture.node_d_id, true);

    REQUIRE(result.plan.has_value());
    CHECK(result.plan->traversed_node_ids.size() == 3);
    CHECK(result.plan->traversed_node_ids[1] == fixture.node_c_id);
    CHECK(result.plan->traversed_zone_ids.size() >= 2);
}

TEST_CASE("route module reports blocked failure diagnostics with restricted and slow context") {
    auto fixture = make_rich_route_workspace();
    const auto edge_ac = fixture.workspace.find_edge(fixture.edge_ac_id);
    const auto edge_cd = fixture.workspace.find_edge(fixture.edge_cd_id);
    REQUIRE(edge_ac.has_value());
    REQUIRE(edge_cd.has_value());
    fixture.workspace.graph().edge_property(*edge_ac).properties["traffic.blocked"] = "true";
    fixture.workspace.graph().edge_property(*edge_cd).properties["traffic.blocked"] = "true";
    const timenav::WorkspaceIndex index{fixture.workspace};

    const auto result = timenav::plan_route(index, fixture.node_a_id, fixture.node_d_id, true);

    CHECK_FALSE(result.plan.has_value());
    REQUIRE(result.failure.has_value());
    CHECK(result.failure->kind == timenav::RouteFailureKind::PolicyBlocked);
    CHECK_FALSE(result.failure->diagnostics.empty());
    bool saw_claim_note = false;
    bool saw_slow_note = false;
    for (const auto &diagnostic : result.failure->diagnostics) {
        saw_claim_note = saw_claim_note || diagnostic.find("claim") != dp::String::npos;
        saw_slow_note = saw_slow_note || diagnostic.find("slow") != dp::String::npos;
    }
    CHECK(saw_claim_note);
    CHECK(saw_slow_note);
}

TEST_CASE("route module reports unreachable diagnostics distinctly from blocked routes") {
    const auto fixture = make_rich_route_workspace();
    const timenav::WorkspaceIndex index{fixture.workspace};

    const auto result = timenav::plan_route(index, fixture.node_d_id,
                                            zoneout::UUID("ffffffff-ffff-4fff-8fff-ffffffffffff"), true);

    REQUIRE(result.failure.has_value());
    CHECK(result.failure->kind == timenav::RouteFailureKind::MissingGoalNode);
    CHECK_FALSE(result.failure->diagnostics.empty());
}
