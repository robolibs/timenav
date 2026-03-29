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
                        .build();

        auto child_a = zoneout::ZoneBuilder()
                           .with_name("child-a")
                           .with_type("field")
                           .with_boundary(rectangle(10.0, 10.0, 40.0, 40.0))
                           .with_datum(dp::Geo{52.0, 5.0, 0.0})
                           .build();

        auto child_b = zoneout::ZoneBuilder()
                           .with_name("child-b")
                           .with_type("field")
                           .with_boundary(rectangle(50.0, 10.0, 90.0, 40.0))
                           .with_datum(dp::Geo{52.0, 5.0, 0.0})
                           .build();

        root.add_child(std::move(child_a));
        root.add_child(std::move(child_b));

        TestWorkspace fixture{zoneout::Workspace(std::move(root))};

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

        fixture.workspace.add_edge(node_a_vertex, node_b_vertex, zoneout::EdgeData{fixture.edge_ab_id});
        fixture.workspace.add_edge(node_b_vertex, node_c_vertex, zoneout::EdgeData{fixture.edge_bc_id});

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

    const timenav::WorkspaceIndex index{workspace};

    REQUIRE(index.zone(root.id()) != nullptr);
    CHECK(index.zone(root.id())->name() == "root");
    REQUIRE(index.zone(child_a.id()) != nullptr);
    CHECK(index.zone(child_a.id())->name() == "child-a");
    REQUIRE(index.zone(child_b.id()) != nullptr);
    CHECK(index.zone(child_b.id())->name() == "child-b");
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
