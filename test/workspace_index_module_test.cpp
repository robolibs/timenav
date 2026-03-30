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

zoneout::Workspace make_invalid_workspace_fixture() {
    auto root = zoneout::ZoneBuilder()
                    .with_name("root")
                    .with_type("workspace")
                    .with_boundary(rectangle(0.0, 0.0, 100.0, 100.0))
                    .with_datum(dp::Geo{52.0, 5.0, 0.0})
                    .build();
    auto child = zoneout::ZoneBuilder()
                     .with_name("child")
                     .with_type("lane")
                     .with_boundary(rectangle(10.0, 10.0, 40.0, 40.0))
                     .with_datum(dp::Geo{52.0, 5.0, 0.0})
                     .build();
    root.add_child(std::move(child));

    zoneout::Workspace workspace{std::move(root)};
    workspace.set_coord_mode(zoneout::CoordMode::Local);

    const auto node_a_id = zoneout::UUID("aa111111-1111-4111-8111-111111111111");
    const auto node_b_id = zoneout::UUID("bb222222-2222-4222-8222-222222222222");
    const auto edge_id = zoneout::UUID("cc333333-3333-4333-8333-333333333333");

    const auto node_a = workspace.add_node(zoneout::NodeData{node_a_id, dp::Point{20.0, 20.0, 0.0}});
    const auto node_b = workspace.add_node(zoneout::NodeData{node_b_id, dp::Point{30.0, 20.0, 0.0}});
    workspace.add_edge(node_a, node_b, zoneout::EdgeData{edge_id});

    workspace.root_zone().node_ids().push_back(node_a_id);
    workspace.root_zone().node_ids().push_back(zoneout::UUID("ffffffff-ffff-4fff-8fff-ffffffffffff"));
    workspace.root_zone().children()[0].node_ids().push_back(node_b_id);

    workspace.graph()[node_a].zone_ids.push_back(workspace.root_zone().id());
    workspace.graph()[node_b].zone_ids.push_back(zoneout::UUID("eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee"));

    const auto edge_vertex = workspace.find_edge(edge_id);
    if (!edge_vertex.has_value()) {
        throw std::runtime_error("expected invalid workspace edge");
    }
    workspace.graph().edge_property(*edge_vertex).zone_ids.push_back(zoneout::UUID("dddddddd-dddd-4ddd-8ddd-dddddddddddd"));

    workspace.clear_ref();
    return workspace;
}

} // namespace

TEST_CASE("workspace index module reports malformed workspace issues in dedicated coverage") {
    const auto workspace = make_invalid_workspace_fixture();
    const timenav::WorkspaceIndex index{workspace};

    const auto issues = index.validation_issues();

    CHECK(issues.size() >= 4);
    bool saw_reference = false;
    bool saw_membership = false;
    for (const auto &issue : issues) {
        saw_reference = saw_reference || issue.category == "invalid_reference";
        saw_membership = saw_membership || issue.category == "broken_membership";
    }
    CHECK(saw_reference);
    CHECK(saw_membership);
}

TEST_CASE("workspace index module reads dedicated property access in a focused test file") {
    auto workspace = make_invalid_workspace_fixture();
    workspace.root_zone().set_property("traffic.mode", "shared");
    const timenav::WorkspaceIndex index{workspace};

    const auto mode = index.zone_property(workspace.root_zone().id(), dp::String{"traffic.mode"});

    REQUIRE(mode.has_value());
    CHECK(mode.value() == "shared");
}

TEST_CASE("workspace index module reports duplicate ids and traffic property issues") {
    auto workspace = make_invalid_workspace_fixture();

    const auto duplicate_zone_id = workspace.root_zone().children()[0].id();
    workspace.root_zone().children()[0].set_property("traffic.speedLimit", "fast");
    auto duplicate_zone = zoneout::ZoneBuilder()
                              .with_name("duplicate-child")
                              .with_type("lane")
                              .with_boundary(rectangle(45.0, 10.0, 70.0, 40.0))
                              .with_datum(dp::Geo{52.0, 5.0, 0.0})
                              .build();
    duplicate_zone.set_property("traffic.mode", "  SHARED  ");
    workspace.root_zone().add_child(std::move(duplicate_zone));
    workspace.root_zone().children().back().set_id(duplicate_zone_id);

    const auto duplicate_node_id = workspace.graph()[workspace.graph().vertices()[0]].id;
    workspace.add_node(zoneout::NodeData{duplicate_node_id, dp::Point{35.0, 35.0, 0.0}});

    const auto edge_id = workspace.graph().edge_property(*workspace.find_edge(
        zoneout::UUID("cc333333-3333-4333-8333-333333333333"))).id;
    const auto extra_node = workspace.add_node(zoneout::NodeData{
        zoneout::UUID("dd444444-4444-4444-8444-444444444444"), dp::Point{40.0, 40.0, 0.0}});
    workspace.graph()[extra_node].zone_ids.push_back(workspace.root_zone().id());
    workspace.add_edge(workspace.graph().vertices()[0], extra_node,
                       zoneout::EdgeData{edge_id, {{"traffic.preferredDirection", "sideways"}}});

    const timenav::WorkspaceIndex index{workspace};
    const auto issues = index.validation_issues();

    bool saw_duplicate_zone = false;
    bool saw_duplicate_node = false;
    bool saw_duplicate_edge = false;
    bool saw_zone_traffic_issue = false;
    bool saw_edge_traffic_issue = false;
    for (const auto &issue : issues) {
        saw_duplicate_zone = saw_duplicate_zone || (issue.category == "duplicate_id" && issue.resource_kind == "zone");
        saw_duplicate_node = saw_duplicate_node || (issue.category == "duplicate_id" && issue.resource_kind == "node");
        saw_duplicate_edge = saw_duplicate_edge || (issue.category == "duplicate_id" && issue.resource_kind == "edge");
        saw_zone_traffic_issue =
            saw_zone_traffic_issue || (issue.category == "traffic_property" && issue.resource_kind == "zone" &&
                                       issue.message.find("traffic.speedLimit") != dp::String::npos);
        saw_edge_traffic_issue =
            saw_edge_traffic_issue || (issue.category == "traffic_property" && issue.resource_kind == "edge" &&
                                       issue.message.find("traffic.preferredDirection") != dp::String::npos);
    }

    CHECK(saw_duplicate_zone);
    CHECK(saw_duplicate_node);
    CHECK(saw_duplicate_edge);
    CHECK(saw_zone_traffic_issue);
    CHECK(saw_edge_traffic_issue);
}
