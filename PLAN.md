# timenav Plan

## Goal

`timenav` should become a navigation and coordination library that:

- uses `zoneout` as the authoritative workspace / map / zone / graph model
- preserves `zoneout` richness instead of flattening it away
  - root/reference point
  - both global and local coordinates
  - recursive zones
  - graph + zone relationships
- assumes robots share the same `zoneout` workspace model
- assumes robots are already connected and can exchange the needed state externally
- is **compatible with VDA5050 3.0.0**
- uses VDA5050 `3.0.0` as the only interoperability reference
- does **not** become merely a VDA5050 implementation library
- supports both:
  - zone claiming / reservation
  - node-to-node navigation over a graph
  - time schedule management
  - traffic control rules

The right architecture is not "copy VDA5050 structs into C++ and stop there".
The right architecture is:

- **core domain model**
- **traffic / scheduling / claiming model**
- **VDA5050 3.0.0 compatibility layer**

That keeps the internal design clean while still letting us speak in VDA5050 terms without shrinking the library to the wire protocol.

### Version Scope

`timenav` does **not** target VDA5050 1.x or 2.x.

Rules:

- only VDA5050 `3.0.0` is a design reference
- no backward-compatibility work for older VDA5050 versions
- the compatibility layer should use `3.0.0` field names, topic names, and concepts
- if older naming differs, `3.0.0` wins
- internal `timenav` domain types do not need to be identical to VDA payload shapes

### Non-Goal

`timenav` is **not** "the VDA5050 library".

It is:

- a coordination / scheduling / traffic-control library
- with a VDA5050 `3.0.0` compatibility layer
- built on top of `zoneout`

### Communication Scope

`timenav` does **not** own robot-to-robot or fleet-to-robot communication.

Assumptions:

- robots already have connectivity
- robots can already exchange state through some external mechanism
- all participants can refer to the same `zoneout` workspace entities by UUID

So `timenav` should focus on:

- shared semantics
- planning
- claims
- schedules
- traffic rules
- conflict evaluation

And should **not** focus on:

- MQTT
- brokers
- wire transport runtime
- connection/session management
- distributed networking mechanics

---

## Repo Context

Current repo state:

- header-only scaffold
- no real implementation yet
- dependencies already include:
  - `zoneout`
  - `graphix`
  - `concord`
  - `entropy`
  - `datapod`
  - `optinum`

This is good enough to start the real design.

---

## Engineering Rules

### datapod-first Types

`timenav` should use `datapod` (`dp::`) types aggressively and by default.

This includes:

- scalar aliases
  - `dp::i8`
  - `dp::i16`
  - `dp::i32`
  - `dp::i64`
  - `dp::u8`
  - `dp::u16`
  - `dp::u32`
  - `dp::u64`
  - `dp::f32`
  - `dp::f64`
- strings
- arrays
- vectors
- maps
- sets
- points / geometry helpers
- other reusable utility/container/value types already provided by `dp::`

Rule:

- if `dp::` already provides the type, prefer `dp::`
- do not casually introduce standard-library duplicates for the same role
- use standard types only when `dp::` does not provide a suitable equivalent or interoperability forces it

This should make `timenav` feel like it belongs in the same family as the rest of the robolibs stack.

### Coordinate Transformations

Coordinate transformation logic should use `concord::`.

Rule:

- `zoneout` remains the source of stored spatial data
- `concord::` performs conversions and frame transformations
- `timenav` should not scatter custom haversine / ENU / ad hoc conversion code throughout the codebase

Use `concord::` for:

- local <-> global coordinate conversion
- reference-frame transformations
- route/planning frame conversions when needed
- future robot/body/map frame conversions

---

## External Reference

This plan is based on the official VDA5050 sources checked on **March 29, 2026**:

- VDA page: <https://www.vda.de/en/topics/automotive-industry/vda-5050>
- official repo: <https://github.com/VDA5050/VDA5050>
- current repo version: **3.0.0**
- official markdown/spec root: <https://raw.githubusercontent.com/VDA5050/VDA5050/main/VDA5050_EN.md>

Important findings from VDA5050 `3.0.0`:

- standard topics include:
  - `order`
  - `instantActions`
  - `state`
  - `visualization`
  - `connection`
  - `factsheet`
  - `zoneSet`
  - `responses`
- the standard already includes:
  - maps
  - zones
  - corridor / edge requests
  - request/response mechanisms
  - shared planned paths
- `state` already includes concepts like:
  - `zoneSets`
  - `zoneRequests`
  - `edgeRequests`
  - `plannedPath`
  - `intermediatePath`
  - `mobileRobotPosition`

This matters because `timenav` should **extend VDA5050**, not recreate an older simpler model.

---

## VDA5050 Summary For timenav

### What VDA5050 is good at

- shared concepts and vocabulary for robot/fleet coordination
- standard message shapes for orders, state, actions, connection, factsheet
- graph-like motion through nodes and edges
- request/response flows for permissions
- optional zone and corridor management

### What VDA5050 intentionally does not solve

Per the spec, it does **not** define:

- traffic management algorithms
- deadlock resolution strategy
- scheduling policy
- route optimization strategy
- internal map / planning architecture
- broader system behavior outside the interface

That means `timenav` must define those parts itself.

### What timenav should reuse directly

- order / state / request vocabulary
- robot/fleet roles
- node / edge / order-update concepts
- request lifecycle concepts
  - requested
  - granted
  - revoked
  - expired
- connection / health separation
- map and zone-set concepts

### What timenav should add

- stronger workspace integration from `zoneout`
- recursive zone hierarchy
- richer zone semantics than plain flat zone sets
- explicit zone claims and leases
- schedule-aware reservation windows
- traffic priority and right-of-way rules
- conflict-resolution policies
- occupancy and capacity management
- zone occupancy / exclusivity rules
- synchronization between graph routing and zone permissions
- cleaner internal scheduling / reservation engine
- time-based coordination beyond what VDA defines

---

## Core Product Direction

`timenav` should be a **coordination kernel** over a `zoneout::Workspace`.

At a high level:

- `zoneout` says what the world is
  - workspace
  - zones
  - graph
  - coordinates
- `timenav` says how robots move and claim access in that world
  - missions
  - routes
  - claims
  - leases
  - reservations
  - schedules
  - priorities
  - right-of-way rules
  - traffic policies
  - coordination state

So:

- `zoneout` = spatial authority
- `timenav` = temporal / coordination authority

That split is clean.

---

## Proposed Architecture

### 1. Spatial Layer

Backed by `zoneout`.

Responsibilities:

- load and validate workspace
- expose zones and graph nodes/edges
- expose zone hierarchy
- expose node-to-zone and zone-to-node relationships
- expose reference / coordinate mode

Main adapter type:

```cpp
struct WorkspaceView;
```

Responsibilities:

- wraps `zoneout::Workspace`
- gives fast lookups by:
  - zone id
  - node id
  - edge id
- precomputes:
  - ancestors / descendants
  - zone containment sets
  - node-zone memberships
  - edge-zone memberships

### 2. Traffic Domain Layer

Pure `timenav` concepts.

Main concepts:

- robot
- mission
- route
- claim
- reservation
- lease
- lock
- release
- progress

Proposed types:

```cpp
struct RobotId;
struct MissionId;
struct ClaimId;
struct LeaseId;

enum class ClaimTargetKind { Zone, Edge, Node };
enum class ClaimMode { Shared, Exclusive, Corridor, Replanning };
enum class ClaimState { Requested, Granted, Denied, Revoked, Expired, Released };

struct ClaimTargetRef {
    ClaimTargetKind kind;
    zoneout::UUID id;
};

struct ClaimRequest {
    ClaimId id;
    RobotId robot_id;
    ClaimMode mode;
    std::vector<ClaimTargetRef> targets;
    std::chrono::system_clock::time_point requested_at;
    std::optional<std::chrono::milliseconds> ttl;
    std::unordered_map<std::string, std::string> properties;
};

struct ClaimLease {
    LeaseId id;
    ClaimId claim_id;
    RobotId robot_id;
    ClaimState state;
    std::chrono::system_clock::time_point granted_at;
    std::optional<std::chrono::system_clock::time_point> expires_at;
};
```

### 3. Route Layer

Built on `graphix` data already stored in `zoneout`.

Responsibilities:

- node-to-node path planning
- edge sequencing
- route snapshots
- route feasibility checks against current claims

Proposed types:

```cpp
struct RouteStep {
    zoneout::UUID node_id;
    std::optional<zoneout::UUID> incoming_edge_id;
};

struct RoutePlan {
    zoneout::UUID start_node_id;
    zoneout::UUID goal_node_id;
    std::vector<RouteStep> steps;
    std::vector<zoneout::UUID> traversed_zone_ids;
    double total_cost;
};
```

### 4. Coordination Layer

Central logic that decides whether claims can be granted.

Responsibilities:

- zone conflict checking
- edge conflict checking
- robot progress updates
- lease expiry
- replan triggers
- queueing / fairness policy

Main service:

```cpp
class ClaimManager;
```

Responsibilities:

- evaluate incoming claim requests
- maintain active leases
- resolve compatibility
- revoke or expire leases
- notify planner / robot adapter

### 5. VDA Compatibility Layer

This must not be the core.
It should be a compatibility / mapping layer.

Responsibilities:

- map internal route / claim / schedule state into VDA5050 `3.0.0` concepts
- keep VDA5050 `3.0.0` semantics recognizable where useful
- provide a conceptual bridge for systems that already think in VDA terms

Main principle:

- **standard fields stay standard**
- `timenav` extensions stay in `timenav` types instead of distorting the core

Do not pollute VDA messages casually.

---

## Relationship With zoneout

`timenav` should consume a `zoneout::Workspace` directly.

Important rule:

- `zoneout` remains richer than the VDA compatibility view
- `timenav` should preserve that richness internally
- the VDA5050 `3.0.0` layer is a projection / adapter, not the canonical storage model
- `zoneout` is not responsible for traffic semantics
- `zoneout` stores graph and zone properties generically
- the UI writes those properties
- `timenav` interprets known property keys into behavior

Expected workspace usage:

- `Workspace.root_zone()` or equivalent root access
- `Workspace.graph()`
- zone hierarchy via recursive `Zone`
- node memberships via node `zone_ids`
- zone memberships via zone `node_ids`
- workspace `ref`
- workspace `coord_mode`

This means `timenav` does **not** need its own map format.

It only needs:

- a loader / adapter from `zoneout`
- derived indexes
- planning / coordination structures

### Required Workspace Adapter Features

The first real implementation target in `timenav` should be:

```cpp
class WorkspaceIndex;
```

With capabilities:

- `zone(zone_id)`
- `node(node_id)`
- `edge(edge_id)`
- `parent_zone(zone_id)`
- `child_zones(zone_id)`
- `ancestor_zones(zone_id)`
- `descendant_zones(zone_id)`
- `nodes_in_zone(zone_id)`
- `zones_of_node(node_id)`
- `zones_of_edge(edge_id)`
- `edge_between(node_a, node_b)`

This index becomes the foundation for both planning and claims.

---

## Property Conventions

`zoneout` should not care about traffic-property key meaning.

Responsibility split:

- UI:
  - exposes editable traffic-related properties
  - writes them into `zoneout` graph/zone properties
- `zoneout`:
  - stores those properties without owning the meaning
- `timenav`:
  - interprets recognized keys
  - ignores unknown keys safely

### Edge Fields vs Edge Properties

For edge semantics:

- `directed` remains the explicit structural field
- other traffic/navigation semantics should live in `edge.properties`

This is the correct split.

Do **not** promote every planning detail into first-class wire fields unless there is a strong structural reason.

### Recommended Edge Property Keys

Recommended keys for `edge.properties`:

- `traffic.speed_limit`
- `traffic.lane_type`
- `traffic.reversible`
- `traffic.passing_allowed`
- `traffic.priority`
- `traffic.capacity`
- `traffic.clearance_width`
- `traffic.clearance_height`
- `traffic.surface_type`
- `traffic.robot_class`
- `traffic.allowed_payload`
- `traffic.cost_bias`
- `traffic.no_stop`
- `traffic.preferred_direction`

Suggested meanings:

- `traffic.speed_limit`
  - maximum traversal speed on the edge
- `traffic.lane_type`
  - examples: `drive`, `service`, `dock`, `crossing`, `corridor`
- `traffic.reversible`
  - whether an operational direction can be changed dynamically despite current intended direction rules
- `traffic.passing_allowed`
  - whether side-by-side or overtaking use is allowed conceptually
- `traffic.priority`
  - relative right-of-way / preference
- `traffic.capacity`
  - max concurrent occupancy count
- `traffic.clearance_width`
  - width constraint for robots / payload
- `traffic.clearance_height`
  - height constraint
- `traffic.surface_type`
  - useful for robot or weather-specific rules
- `traffic.robot_class`
  - optional class restriction
- `traffic.allowed_payload`
  - optional capability or load restriction
- `traffic.cost_bias`
  - planner-specific penalty/bonus
- `traffic.no_stop`
  - disallow waiting on the edge
- `traffic.preferred_direction`
  - a soft bias, different from hard `directed`

### Recommended Zone Property Keys

Recommended keys for `zone.properties`:

- `traffic.policy`
- `traffic.capacity`
- `traffic.priority`
- `traffic.claim_required`
- `traffic.entry_rule`
- `traffic.exit_rule`
- `traffic.speed_limit`
- `traffic.waiting_allowed`
- `traffic.stop_allowed`
- `traffic.replan_trigger`
- `traffic.blocked`
- `traffic.robot_class`
- `traffic.schedule_window`
- `traffic.access_group`

Suggested meanings:

- `traffic.policy`
  - examples: `exclusive`, `shared`, `corridor`, `restricted`, `slow`, `informational`
- `traffic.capacity`
  - max concurrent robots or reservations
- `traffic.priority`
  - right-of-way weight inside arbitration
- `traffic.claim_required`
  - whether entry requires an active claim/lease
- `traffic.entry_rule`
  - semantic rule name for entry checks
- `traffic.exit_rule`
  - semantic rule name for exit checks
- `traffic.speed_limit`
  - zone-level cap, merged with edge rules
- `traffic.waiting_allowed`
  - whether a robot may wait/queue in the zone
- `traffic.stop_allowed`
  - whether a robot may stop in the zone
- `traffic.replan_trigger`
  - whether entry/revocation should force replanning behavior
- `traffic.blocked`
  - temporary hard block
- `traffic.robot_class`
  - access restriction by robot class
- `traffic.schedule_window`
  - optional time-window rule reference
- `traffic.access_group`
  - group-based coordination or authorization

### Parsing Rules

`timenav` should define deterministic parsing rules:

- known keys are parsed into typed traffic semantics
- malformed values should produce validation warnings or errors
- unknown keys should be preserved and ignored by default
- explicit structural fields like `directed` should win over conflicting property hints

### Validation Rules

`timenav` should validate known traffic properties, for example:

- `traffic.speed_limit` must parse as positive numeric
- `traffic.capacity` must parse as integer >= 1
- `traffic.priority` must parse as numeric
- `traffic.claim_required` must parse as boolean
- `traffic.blocked` must parse as boolean

Validation should happen in:

- workspace ingestion / indexing
- dedicated property validation helpers
- tests with malformed property fixtures

---

## Zone Model In timenav

This is the most important design area.

### Key Principle

Zones are not just visualization layers.
They are coordination primitives.

A zone can mean:

- exclusive area
- slow area
- handoff area
- charging area
- docking area
- loading area
- one-way area
- corridor gate
- temporary blocked area
- replan area

### Proposed Zone Semantics

Each zone should have a `policy`.

```cpp
enum class ZonePolicyKind {
    Informational,
    ExclusiveAccess,
    SharedAccess,
    CapacityLimited,
    Corridor,
    Replanning,
    Restricted,
    NoStop,
    Slowdown
};
```

And:

```cpp
struct ZonePolicy {
    ZonePolicyKind kind;
    std::size_t capacity = 1;
    bool requires_claim = false;
    bool blocks_traversal_without_grant = false;
    bool blocks_entry_without_grant = false;
    std::unordered_map<std::string, std::string> properties;
};
```

This should live in `timenav`, not be forced into `zoneout`.

The mapping from `zoneout::Zone.properties()` to `ZonePolicy` should be handled by an adapter/parser.

### Inheritance

Because `zoneout` zones are recursive, `timenav` should define inheritance rules:

- child zones can override parent policy
- parent restrictions may still apply
- the effective policy for a node/edge is the merged result of all containing zones

Recommended merge rules:

- `Restricted` dominates everything
- `ExclusiveAccess` dominates `SharedAccess`
- smaller `capacity` wins
- explicit child override beats inherited default where not safety-critical

This needs to be deterministic and documented.

---

## Claiming Model

### Why Claims Matter

You want:

- zone claiming
- then node-to-node navigation

That means the route planner cannot be independent of claims.

A robot should not commit to a route if required zones are unavailable.

### Claim Targets

Claims should support:

- zones
- edges
- nodes

Even if zone claims are the primary concept.

Why:

- some systems need edge-level corridor reservation
- some systems need node occupancy at docks/intersections
- VDA5050 already has edge/corridor request concepts

### Claim Lifecycle

Use a VDA-like lifecycle:

- `REQUESTED`
- `GRANTED`
- `DENIED`
- `REVOKED`
- `EXPIRED`
- `RELEASED`

`DENIED` is not standard wording in all VDA places, but internally useful.

### Lease-Based Claims

Claims should not be indefinite.

Every granted claim should become a lease:

- grant time
- expiry time
- heartbeat or progress refresh
- explicit release

This prevents stale locks.

### Claim Granularity

There should be two modes:

1. **pre-claim route**
- ask for all required zones before movement

2. **rolling claim**
- claim a horizon ahead of the robot
- release behind the robot

Both are useful.
The second is the realistic long-term mode.

---

## Path Planning Model

### Graph Authority

Path planning must use the `zoneout` graph directly.

That graph already has:

- node ids
- edge ids
- zone memberships
- geometry

`timenav` should not duplicate graph topology.

### Planner Responsibilities

- shortest path / least cost path
- policy-aware routing
- avoid blocked zones
- avoid revoked or incompatible claims
- optionally prefer lower congestion

### Recommended Planner Design

Start simple:

- Dijkstra / A*
- edge weights from workspace edge metadata
- additional penalties from zone policy

Later:

- time-expanded planning
- multi-robot reservation-aware planning
- conflict-based search or priority planning if needed

### Route And Claim Coupling

Planner output should include:

- traversed node ids
- traversed edge ids
- touched zone ids

That lets the coordinator derive required claims directly from a route.

---

## Proposed Public API Direction

Initial public API should stay small.

### Loading

```cpp
WorkspaceIndex index = WorkspaceIndex::from_workspace(workspace);
```

### Planning

```cpp
RoutePlan route = planner.plan(robot_id, start_node_id, goal_node_id, constraints);
```

### Claiming

```cpp
ClaimRequest req = coordinator.make_zone_claim(robot_id, route);
ClaimLease lease = claim_manager.request(req);
```

### Progress

```cpp
coordinator.update_robot_progress(robot_id, current_node_id, current_edge_id);
```

### Inspection

```cpp
auto active = claim_manager.active_claims_for(robot_id);
auto conflicts = claim_manager.conflicts_for(req);
```

---

## VDA5050 Mapping Strategy

This is important.

### Do Not Make Internal Types Equal To Wire Types

Bad idea:

- internal domain model == VDA JSON schema

Good idea:

- internal domain model
- plus conversion layer

Why:

- internal planning needs richer semantics
- VDA-compatible shapes are interoperability artifacts
- `timenav` wants stronger zone semantics than standard VDA alone

### Mapping Proposal

#### Internal -> VDA Order

Map route plan to:

- `orderId`
- `orderUpdateId`
- `nodes`
- `edges`
- actions on nodes or edges where needed

#### Internal -> VDA State

Map runtime state to:

- `nodeStates`
- `edgeStates`
- `agvPosition` / `mobileRobotPosition`
- `zoneRequests`
- `edgeRequests`
- `zoneSets`
- `actionStates`

#### Extension Strategy

For things VDA does not model well enough:

- use `timenav` extension payloads
- or separate `timenav` topic family

Recommended extension approach:

- keep VDA topics clean for standard fields
- add `timenav/v1/...` topics for advanced zone coordination

Examples:

- `timenav/v1/.../claims`
- `timenav/v1/.../leases`
- `timenav/v1/.../policies`
- `timenav/v1/.../workspace-sync`

This is cleaner than overstuffing standard state messages.

---

## Data Model Proposal

### Core IDs

Use explicit strong ids:

```cpp
struct RobotId { std::string value; };
struct MissionId { std::string value; };
struct ClaimId { std::string value; };
struct LeaseId { std::string value; };
```

Do not use raw strings everywhere.

### Robot Runtime State

```cpp
struct RobotRuntimeState {
    RobotId id;
    zoneout::UUID current_node_id;
    std::optional<zoneout::UUID> current_edge_id;
    std::vector<zoneout::UUID> active_zone_ids;
    std::vector<ClaimId> active_claim_ids;
    bool connected = false;
    bool localized = false;
    bool driving = false;
};
```

### Mission

```cpp
struct Mission {
    MissionId id;
    RobotId robot_id;
    zoneout::UUID start_node_id;
    zoneout::UUID goal_node_id;
    std::unordered_map<std::string, std::string> properties;
};
```

### Policy Snapshot

```cpp
struct EffectivePolicy {
    std::vector<zoneout::UUID> contributing_zone_ids;
    ZonePolicy merged_policy;
};
```

---

## Recommended Package Structure

Suggested include layout:

```text
include/timenav/
  timenav.hpp
  ids.hpp
  workspace_index.hpp
  zone_policy.hpp
  route.hpp
  planner.hpp
  claim.hpp
  claim_manager.hpp
  coordinator.hpp
  robot_state.hpp
  vda/
    order.hpp
    state.hpp
    connection.hpp
    instant_actions.hpp
    factsheet.hpp
    responses.hpp
    adapter.hpp
```

Suggested tests:

```text
test/
  test_workspace_index.cpp
  test_zone_policy.cpp
  test_planner.cpp
  test_claim_manager.cpp
  test_coordinator.cpp
  test_vda_adapter.cpp
```

---

## Phase Plan

### Phase 0: Foundation

Goal:

- establish internal model and indexing

Tasks:

- add strong id types
- add `WorkspaceIndex`
- add basic zone/node/edge lookup APIs
- add ancestor / descendant / membership derivation
- add small fixtures based on `zoneout::Workspace`

Done when:

- `timenav` can load a `zoneout::Workspace`
- code can answer:
  - which zones contain node X
  - which nodes are in zone Y
  - which edges touch zone Z

### Phase 1: Zone Policy Layer

Goal:

- interpret `zoneout` zone metadata into runtime coordination policy

Tasks:

- define `ZonePolicy`
- define merge rules
- parse zone properties into policy
- build effective policy for node / edge / route

Done when:

- a node or edge can be assigned a deterministic effective policy

### Phase 2: Basic Planner

Goal:

- route node-to-node across the workspace graph

Tasks:

- add graph traversal adapter over `zoneout::Workspace::graph`
- implement Dijkstra or A*
- add route output with traversed nodes / edges / zones
- add route blocking from effective policy

Done when:

- planner can produce a route from node A to B
- blocked zones make route fail or reroute

### Phase 3: Claim Engine

Goal:

- support explicit claim request / grant / release

Tasks:

- add claim request / lease model
- add compatibility matrix
- add lease expiry
- add conflict queries
- add release and revoke flows

Done when:

- multiple robots can request overlapping zones and get deterministic outcomes

### Phase 4: Coordinator

Goal:

- connect route planning with claims and robot progress

Tasks:

- derive required claims from route
- add rolling-horizon claim mode
- release claims behind robot progress
- replan when revoked or blocked

Done when:

- simulated robots can move node-to-node while holding and releasing relevant claims

### Phase 5: VDA5050 Adapter

Goal:

- expose VDA5050 `3.0.0` compatible adapter objects and mappings

Tasks:

- define C++ compatibility structs for:
  - order
  - state
  - connection
  - instant actions
  - factsheet
  - responses
- add adapters from internal runtime to compatibility objects
- add request/response helpers for zone / edge requests

Done when:

- internal route/claim state can be projected into VDA5050-style messages

### Phase 6: Extensions

Goal:

- add richer features beyond base VDA

Tasks:

- richer zone policies
- priority / fairness
- capacity > 1 zones
- temporary closures
- maintenance windows
- path sharing

---

## Milestone Execution Plan

This section is the actual implementation checklist.

Rules:

- each slice should be small enough for one focused commit
- each slice should be testable on its own
- prefer one behavior change per slice
- do not start a new milestone before the previous milestone is green

Target:

- at least **5 milestones**
- at least **10 slices per milestone**
- about **50 total slices/commits**

### Milestone 1: Workspace Index Foundation

- [x] Slice 1.1: add `ids.hpp` with strong id wrappers for robot, mission, claim, and lease ids
- [x] Slice 1.2: switch the new ids to preferred `dp::` scalar/string types where applicable
- [x] Slice 1.3: add `workspace_index.hpp` scaffold and empty `WorkspaceIndex` type
- [x] Slice 1.4: add workspace ownership/reference handling in `WorkspaceIndex`
- [x] Slice 1.5: add zone lookup by UUID
- [x] Slice 1.6: add node lookup by UUID
- [x] Slice 1.7: add edge lookup by UUID
- [x] Slice 1.8: add parent-zone lookup and child-zone lookup
- [x] Slice 1.9: add ancestor-zone and descendant-zone traversal helpers
- [x] Slice 1.10: add initial tests for basic workspace indexing and UUID resolution

### Milestone 2: Membership And Spatial Semantics

- [x] Slice 2.1: add `nodes_in_zone(zone_id)` query
- [x] Slice 2.2: add `zones_of_node(node_id)` query
- [x] Slice 2.3: add `zones_of_edge(edge_id)` query
- [x] Slice 2.4: add `edge_between(node_a, node_b)` query
- [x] Slice 2.5: expose workspace `ref` through the index
- [x] Slice 2.6: expose workspace `coord_mode` through the index
- [x] Slice 2.7: add `concord::`-based helpers for local/global conversions inside the indexing layer
- [x] Slice 2.8: add property-read helpers for zones and edges
- [x] Slice 2.9: add validation helpers for missing ids / broken memberships / invalid references
- [x] Slice 2.10: add tests for memberships, hierarchy traversal, and local/global coordinate access

### Milestone 3: Zone Policy And Property Parsing

- [x] Slice 3.1: add `zone_policy.hpp` with typed zone policy enums and structs
- [x] Slice 3.2: add typed edge-traffic semantics struct for parsed edge properties
- [x] Slice 3.3: add parser for known `zone.properties` traffic keys
- [x] Slice 3.4: add parser for known `edge.properties` traffic keys
- [x] Slice 3.5: add boolean/number/string parsing utilities for traffic properties
- [x] Slice 3.6: add validation errors/warnings for malformed traffic property values
- [x] Slice 3.7: add effective-policy merge rules for nested zones
- [x] Slice 3.8: add effective edge semantics derivation combining structural fields and properties
- [x] Slice 3.9: add tests for property parsing, bad values, and inheritance/override rules
- [x] Slice 3.10: add documentation comments/examples for the supported `traffic.*` keys

### Milestone 4: Routing And Planning

- [x] Slice 4.1: add `route.hpp` with route step and route plan types
- [x] Slice 4.2: add graph traversal adapter over the `zoneout` graph
- [x] Slice 4.3: add basic shortest-path search without traffic constraints
- [x] Slice 4.4: add route reconstruction from predecessor state
- [x] Slice 4.5: add route cost accumulation from edge weight
- [x] Slice 4.6: add edge blocking from hard zone/edge policy
- [x] Slice 4.7: add planner penalties from speed limit / cost bias / restricted policies
- [x] Slice 4.8: add extraction of traversed nodes / edges / zones from a route
- [x] Slice 4.9: add failure reporting for unreachable routes and policy-blocked routes
- [x] Slice 4.10: add planner tests on small workspace fixtures with blocked and allowed alternatives

### Milestone 5: Claims, Leases, And Conflicts

- [x] Slice 5.1: add `claim.hpp` with request, lease, and target types
- [x] Slice 5.2: add `claim_manager.hpp` scaffold
- [x] Slice 5.3: add storage for active claim requests
- [x] Slice 5.4: add storage for granted leases
- [x] Slice 5.5: add compatibility matrix for zone claims
- [x] Slice 5.6: add compatibility matrix for edge and node claims
- [x] Slice 5.7: add claim grant/deny evaluation
- [x] Slice 5.8: add lease release path
- [x] Slice 5.9: add lease expiry handling
- [x] Slice 5.10: add tests for conflicting and non-conflicting claim scenarios

### Milestone 6: Scheduling And Coordination

- [x] Slice 6.1: add `robot_state.hpp` with robot runtime state
- [x] Slice 6.2: add `coordinator.hpp` scaffold
- [x] Slice 6.3: add robot registration/state tracking
- [x] Slice 6.4: add route-to-claim derivation helpers
- [x] Slice 6.5: add rolling-horizon claim generation
- [x] Slice 6.6: add progress updates by current node / edge
- [x] Slice 6.7: add release-behind behavior after progress updates
- [ ] Slice 6.8: add basic schedule-window checks from zone properties
- [ ] Slice 6.9: add simple priority/right-of-way arbitration hooks
- [ ] Slice 6.10: add tests for multi-robot progress, release, and scheduling conflicts

### Milestone 7: VDA5050 3.0.0 Compatibility Layer

- [ ] Slice 7.1: add `vda/order.hpp`
- [ ] Slice 7.2: add `vda/state.hpp`
- [ ] Slice 7.3: add `vda/connection.hpp`
- [ ] Slice 7.4: add `vda/instant_actions.hpp`
- [ ] Slice 7.5: add `vda/factsheet.hpp`
- [ ] Slice 7.6: add `vda/responses.hpp`
- [ ] Slice 7.7: add `vda/adapter.hpp`
- [ ] Slice 7.8: map internal route plans to VDA order-compatible objects
- [ ] Slice 7.9: map internal runtime/claim state to VDA state-compatible objects
- [ ] Slice 7.10: add tests for VDA `3.0.0` compatibility mappings

This gives at least **70** possible slices. You do not have to implement all of them immediately, but the first **50** are already naturally available by stopping somewhere in milestone 6 or early milestone 7.

---

## Implementation Order

Do this in order:

1. `ids.hpp`
2. `workspace_index.hpp`
3. `zone_policy.hpp`
4. `route.hpp`
5. `planner.hpp`
6. `claim.hpp`
7. `claim_manager.hpp`
8. `coordinator.hpp`
9. `vda/` transport structs
10. adapters and examples

Do **not** start with JSON schema cloning.
Do **not** start with transport before internal semantics exist.

---

## Example Scenario Target

This should be the first end-to-end example:

- load a `zoneout::Workspace`
- define:
  - one root farm zone
  - one field zone
  - one exclusive gate zone
  - graph through the field and gate
- robot A requests route from node N1 to N5
- coordinator derives:
  - graph path
  - needed zones
  - needed gate claim
- claim manager grants or denies
- robot progress updates release zones behind it

If `timenav` can do that well, the architecture is probably right.

---

## Open Design Decisions

These need explicit decisions before implementation goes too far.

### 1. Canonical Membership Source

Current recommendation:

- `zoneout` graph node/edge memberships remain canonical
- `timenav` derives effective policy from those memberships

### 2. Claim Target Scope

Recommendation:

- support zone + edge + node claims internally from day one
- expose zone claiming first in public examples

### 3. Manual vs Derived Zone Policy

Recommendation:

- zone geometry and memberships are spatial facts
- zone policy is semantic and comes from zone properties / config

### 4. Transport Shape

Recommendation:

- internal model first
- VDA-compatible adapter second

### 5. Time Model

Recommendation:

- use `std::chrono`
- lease expiry and heartbeat support from day one

---

## Non-Goals For First Version

Do not try to solve all of this in `0.0.1`:

- full VDA schema parity
- multi-fleet federation
- optimal multi-agent scheduling
- deadlock-proof global planner
- UI / visualization platform
- safety certification concerns

First version should prove:

- workspace integration
- route planning
- zone claim semantics
- VDA-shaped adapter model

---

## Concrete Next Step

First actual implementation pass should create:

- `include/timenav/ids.hpp`
- `include/timenav/workspace_index.hpp`
- `include/timenav/zone_policy.hpp`
- `test/test_workspace_index.cpp`

That is the correct starting point.
