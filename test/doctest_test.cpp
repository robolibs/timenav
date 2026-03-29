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

} // namespace

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
