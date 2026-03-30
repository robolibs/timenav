# timenav Plan

## Status

Current status, stated narrowly and honestly:

- buildable: yes
- tested: yes
- usable foundation: yes
- architecturally aligned with the intended design: mostly yes
- checklist-complete against this plan: yes
- production-finished: no

What is complete:

- the core header-only library structure exists under `include/timenav/`
- workspace indexing and coordinate handling are implemented on top of `zoneout` and `concord`
- traffic/property parsing, route planning, claims, lease handling, coordinator behavior, and partial VDA5050 `3.0.0` compatibility are implemented
- the implementation checklist in this file is complete

What is not claimed:

- full VDA5050 `3.0.0` schema parity
- a production-mature scheduler
- complete user/operator documentation
- fully hardened operational behavior for all real fleet deployments

This file now serves two purposes:

1. the architecture record for what `timenav` is supposed to be
2. a completed implementation checklist for the current development phase

Further work should go into a new production-readiness or roadmap document instead of reopening this checklist indirectly through stale wording.

---

## Goal

`timenav` is a header-only C++ navigation and coordination library that:

- uses `zoneout` as the authoritative workspace, graph, and zone model
- preserves `zoneout` richness instead of flattening it away
- plans node-to-node routes over a shared workspace graph
- models claims, leases, and coordination semantics over zones, nodes, and edges
- applies traffic and scheduling rules on top of the shared workspace
- exposes a partial VDA5050 `3.0.0` compatibility layer without reducing the library to a wire-protocol clone

The intended architecture remains:

- core domain model
- traffic / scheduling / claiming model
- VDA5050 `3.0.0` compatibility layer

---

## Scope

### Version Scope

`timenav` uses VDA5050 `3.0.0` as its only interoperability reference.

Rules:

- no backward-compatibility work for VDA5050 `1.x` or `2.x`
- `3.0.0` naming and concepts win when versions differ
- internal domain types do not need to be identical to VDA payload shapes

### Non-Goals

`timenav` is not:

- a broker
- an MQTT runtime
- a transport/session manager
- a full VDA5050 schema implementation library

It assumes connectivity and message exchange are handled externally.

---

## Engineering Rules

### Header-only

- library code belongs in `include/`
- `src/main.cpp`, if present, is CI/CD-only and not part of the library design

### datapod-first

Use `datapod` (`dp::`) types by default when suitable equivalents exist.

### Coordinate handling

Use `concord::` for coordinate and frame transformations.

### Workspace source of truth

`zoneout` remains the canonical source of stored spatial/map data.

---

## Current Implementation Summary

Implemented areas:

- strong ids
- workspace ownership/borrowing and indexing
- zone hierarchy and membership traversal
- local/global coordinate helpers
- traffic/property parsing and validation
- effective zone and edge semantics
- graph traversal and route planning
- route diagnostics and route-plan extraction
- claim requests, leases, evaluation, refresh, revoke, and release/expiry handling
- coordinator state tracking, rolling claims, scheduling decisions, queue/replan behavior, and progress-based release
- partial VDA5050 `3.0.0` transport mappings

Still considered partial in maturity terms:

- VDA compatibility depth
- scheduler sophistication
- production-facing documentation

---

## Maturity Gaps

These are not checklist failures. They are the main reasons the project should still not be described as fully finished or production-ready.

### VDA Compatibility

Implemented:

- partial order/state/connection/factsheet/action/response mapping
- compatibility helpers shaped around VDA5050 `3.0.0`

Not claimed:

- full schema parity
- full protocol behavior coverage
- transport/runtime ownership

### Scheduling

Implemented:

- timed reservation windows
- queue vs replan decisions
- missed-slot handling
- lease refresh/revoke interactions

Still not a mature scheduler:

- no evidence yet of long-horizon fleet scheduling sophistication
- no advanced optimization or deadlock policy claims
- no operational tuning guidance yet

### Documentation

Implemented:

- short `README.md`
- detailed `book/` documentation skeleton

Still missing:

- polished user guide
- examples-rich narrative docs
- production deployment guidance

---

## Detailed Assessment

This section is an audit of the repository as it exists now, not a restatement of the intended architecture.

Audit basis:

- full repository scan of `include/`, `test/`, `examples/`, `book/`, and top-level docs
- `make build` passes
- `make test` passes
- test suite result at audit time:
  - `doctest_test`: `113` test cases, `1083` assertions
  - plus focused module tests for claims, policy, routes, and workspace indexing
- additional subsystem review performed across multiple independent passes

### Overall Assessment

High-level conclusion:

- the project is real, coherent, and materially implemented
- it is not just a scaffold
- it is not fully finished in the product sense
- the checklist in this file is complete, but maturity still lags in several areas

Current practical status:

- core implementation: strong
- scheduling/coordination behavior: usable but still shallow in orchestration terms
- VDA compatibility: meaningful but explicitly partial
- docs/examples: honest, but still too thin for strong integrator readiness

Suggested overall completion estimate:

- against the implementation checklist in this file: `100%`
- against the intended architectural foundation: about `75%`
- against a stronger “finished library” standard: about `65%`

### Subsystem Assessment

#### 1. Core Model / Index / Property Parsing

Estimated completion:

- about `70%`

Implemented well:

- `WorkspaceIndex` is a functional read model over `zoneout::Workspace`
- ownership/borrowing and refresh behavior are implemented
- hierarchy traversal and membership queries are implemented
- local/global coordinate access and guarded `concord::` conversions are implemented
- `ZonePolicy` and `EdgeTrafficSemantics` are implemented as typed interpretations of `traffic.*` properties
- alias normalization for UI-facing camelCase vs snake_case property keys exists
- tests cover indexing, property parsing, malformed values, hierarchy traversal, and coordinate/reference behavior

Still shallow or missing:

- property parsing is permissive and can silently fall back to defaults
- malformed `traffic.*` values are not fully integrated into general workspace validation
- duplicate UUID collisions are not surfaced strongly enough
- semantic-string normalization is inconsistent
- typed models still lack stronger controlled-vocabulary validation for several fields
- `ids.hpp` is intentionally minimal and does not yet provide richer helper surfaces

Main strengths:

- good API shape
- strong practical utility already
- good test coverage relative to surface area

Main risks:

- bad configuration can still look valid if callers do not explicitly validate
- duplicate-id corruption would be hard to detect early

#### 2. Routing / Claims / Coordinator / Scheduler

Estimated completion:

- routing: `80-85%`
- claims and lease lifecycle: `70-75%`
- coordinator/scheduler orchestration: `45-55%`
- subsystem overall: about `65%`

Implemented well:

- routing is real and not placeholder logic
- route extraction, route validation, route diagnostics, penalties, and blocked/unreachable distinctions exist
- claim and lease data model is implemented
- request/lease storage, conflict evaluation, bounded shared capacity, lease release/expiry/refresh/revoke are implemented
- coordinator state tracks robot progress, route assignment, hold reasons, pending claims, active leases, replanning state, and schedule ticks
- rolling-horizon claim generation exists
- progress-based release exists
- schedule-window extraction and queue/replan decisions exist
- missed-slot handling exists

Still shallow or missing:

- there is no truly integrated reservation/grant pipeline that autonomously turns requests into granted leases
- queue position is synthetic, not managed by a durable queue
- request priority exists but is not deeply integrated into scheduling and arbitration behavior
- right-of-way logic exists mostly as helper behavior, not as a deeply integrated scheduler policy
- the scheduler is overlap-based, not a strong temporal resource allocator
- scheduler/resource semantics are still simpler than claim-manager semantics in some areas
- there is no persistence, recovery, or concurrency/distributed coordination model
- scheduler functionality lives inside `coordinator.hpp` rather than as a more mature standalone scheduling subsystem

Main strengths:

- routing is the strongest part of the subsystem
- claim/lease lifecycle is concrete and test-backed
- rolling-horizon progress handling is present and useful

Main risks:

- the richness of the data model can make orchestration look more complete than it is
- scheduling decisions and claim decisions can still diverge in edge cases
- end-to-end fleet progression behavior is not yet as strong as the internal helper coverage might suggest

#### 3. VDA Compatibility / Docs / Examples / Product Readiness

Estimated completion:

- usable VDA compatibility subsystem: about `60%`
- full VDA-facing product surface: materially lower and not claimed

Implemented well:

- VDA compatibility shapes exist for order, state, connection, factsheet, instant actions, and responses
- adapter mappings from route/workspace/robot state into VDA-shaped structures are implemented
- the VDA layer does carry real route semantics such as reservations, speed limits, access groups, schedule windows, and bidirectionality hints
- docs are unusually honest about scope and maturity
- architecture documentation correctly describes the VDA layer as downstream compatibility rather than the core model

Still shallow or missing:

- the VDA structs are explicitly partial, not schema-complete
- no transport/runtime ownership exists, by design
- no proof of full interoperability against a real external VDA stack is present
- state mapping still uses simplified summaries in several places
- examples are too weak for integrator use
- the main example currently only prints the version
- docs explain intent better than they explain adoption
- there is no strong supported/unsupported field matrix yet

Main strengths:

- very honest scope
- sensible “adapter, not ontology” architecture
- meaningful, not fake, compatibility projection

Main risks:

- users may overestimate VDA depth because the adapter surface exists
- example and documentation quality are still below the implementation quality

### Why The Project Is Not Yet “Fully Implemented”

The main reasons are:

1. the scheduler is still a practical first coordination layer, not a mature fleet scheduler
2. the reservation/claim pipeline still needs deeper orchestration behavior
3. VDA compatibility is explicitly partial
4. docs and examples lag behind the implementation
5. hardening and stricter validation still have room to grow

### Honest One-Line Summary

Best short description today:

- `timenav` is a substantially implemented and tested coordination library foundation with partial VDA5050 `3.0.0` compatibility, but it should still be treated as not fully finished in scheduler depth, VDA breadth, and integrator readiness.

---

## Checklist Result

The implementation checklist for this phase is complete.

Progress:

- `139 / 139` checklist items complete
- completion percentage: `100.00%`

### Milestone 1: Workspace Index Foundation

- [x] align remaining internals more consistently with `dp::` conventions
- [x] harden validation coverage and failure reporting
- [x] verify reference / coord-mode handling against all intended `zoneout` workflows
- [x] add stronger test coverage for malformed memberships and invalid references
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

- [x] verify all coordinate transforms go through `concord::` only
- [x] tighten local/global workflow validation
- [x] add stronger edge-membership and hierarchy consistency tests
- [x] document exact behavior when `coord_mode` and `ref` do not agree
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

- [x] cover the full agreed `traffic.*` property set
- [x] finish stable validation/warning API for malformed property values
- [x] verify merge/inheritance rules exactly match the intended hierarchy semantics
- [x] cross-check parsing against the UI property editors now in use
- [x] Slice 3.1: add `zone_policy.hpp` with typed zone policy enums and structs
- [x] Slice 3.2: add typed edge-traffic semantics struct for parsed edge properties
- [x] Slice 3.3: add parser for known `zone.properties` traffic keys
- [x] Slice 3.4: add parser for known `edge.properties` traffic keys
- [x] Slice 3.5: add boolean/number/string parsing utilities for traffic properties
- [x] Slice 3.6: add validation errors/warnings for malformed traffic property values
- [x] Slice 3.7: add effective-policy merge rules for nested zones
- [x] Slice 3.8: add effective edge semantics derivation combining structural fields and properties
- [x] Slice 3.9: add policy-layer regression tests
- [x] Slice 3.10: document supported `traffic.*` keys in code/comments

### Milestone 4: Route Planning

- [x] ensure planner honors `directed` semantics exactly
- [x] incorporate edge property semantics more completely into costing
- [x] incorporate zone property semantics more completely into costing
- [x] propagate penalized search costs into returned `RoutePlan` totals and timings
- [x] improve blocked-vs-unreachable diagnostics
- [x] add richer workspace fixtures to validate planner behavior
- [x] Slice 4.1: add `route.hpp`
- [x] Slice 4.2: add graph traversal adapter over the `zoneout` graph
- [x] Slice 4.3: add shortest-path search over nodes
- [x] Slice 4.4: add route reconstruction from predecessor state
- [x] Slice 4.5: add route-plan shape and cost helpers
- [x] Slice 4.6: add traversal extraction for nodes, edges, zones, and steps
- [x] Slice 4.7: add policy-aware blocked-edge handling
- [x] Slice 4.8: add penalty-aware planning
- [x] Slice 4.9: add route failure reporting
- [x] Slice 4.10: add planner regression tests

### Milestone 5: Claims And Leases

- [x] reflect real traffic semantics, not only simple exclusivity checks
- [x] improve denial/conflict explanations
- [x] deepen capacity-limited resource handling
- [x] connect lease behavior more tightly to coordinator/scheduler expectations
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

- [x] build a stronger time-scheduling model beyond simple window checks
- [x] deepen priority/right-of-way behavior
- [x] add queueing and reservation-window logic
- [x] define richer robot progress/state lifecycle behavior
- [x] Slice 6.1: add `robot_state.hpp` with robot runtime state
- [x] Slice 6.2: add `coordinator.hpp` scaffold
- [x] Slice 6.3: add robot registration/state tracking
- [x] Slice 6.4: add route-to-claim derivation helpers
- [x] Slice 6.5: add rolling-horizon claim generation
- [x] Slice 6.6: add progress updates by current node / edge
- [x] Slice 6.7: add release-behind behavior after progress updates
- [x] Slice 6.8: add basic schedule-window checks from zone properties
- [x] Slice 6.9: add simple priority/right-of-way arbitration hooks
- [x] Slice 6.10: add tests for multi-robot progress, release, and scheduling conflicts

### Milestone 7: VDA5050 `3.0.0` Compatibility Layer

- [x] align compatibility structs more closely with VDA5050 `3.0.0`
- [x] add missing zone/edge request concepts where relevant
- [x] expand state/order mappings substantially
- [x] keep the adapter thin while still being meaningfully `3.0.0` compatible
- [x] Slice 7.1: add `vda/order.hpp`
- [x] Slice 7.2: add `vda/state.hpp`
- [x] Slice 7.3: add `vda/connection.hpp`
- [x] Slice 7.4: add `vda/instant_actions.hpp`
- [x] Slice 7.5: add `vda/factsheet.hpp`
- [x] Slice 7.6: add `vda/responses.hpp`
- [x] Slice 7.7: add `vda/adapter.hpp`
- [x] Slice 7.8: map internal route plans to VDA order-compatible objects
- [x] Slice 7.9: map internal runtime/claim state to VDA state-compatible objects
- [x] Slice 7.10: add tests for VDA `3.0.0` compatibility mappings

### Remaining Work Checklist Result

- [x] define an actual scheduling model, not only a route/window filter
- [x] define reservation windows over time for zones and edges
- [x] define queueing rules for waiting robots
- [x] define what happens when a robot misses its expected time slot
- [x] define how schedule conflict resolution interacts with replanning
- [x] define right-of-way rules beyond simple priority comparison
- [x] fix bounded-capacity enforcement for shared zone and edge claims
- [x] define waiting/no-stop behavior on lanes and zones
- [x] define corridor semantics clearly
- [x] define how blocked/restricted/slow policies affect planning vs claiming
- [x] define claim target semantics precisely for zones, edges, and nodes
- [x] define richer denial reasons and operator/debug visibility
- [x] define lease refresh / extension behavior
- [x] define revoke behavior and required downstream reactions
- [x] fix rolling-horizon release/claim behavior to use per-step zone coverage instead of de-duplicated route zones
- [x] clean requests and leases correctly on robot unregister/reset
- [x] improve blocked-vs-unreachable diagnostics
- [x] add richer workspace fixtures to validate planner behavior
- [x] reduce remaining casual `std::` usage where `dp::` equivalents should be used
- [x] standardize string/container/value conventions across all headers
- [x] verify result/error surfaces use the intended library conventions consistently
- [x] define refresh / mutation semantics for borrowed `WorkspaceIndex` inputs
- [x] deepen `order` compatibility with real `3.0.0` expectations
- [x] deepen `state` compatibility with real `3.0.0` expectations
- [x] incorporate zone/edge request concepts more faithfully
- [x] make compatibility mapping broader without letting it become the core model
- [x] validate malformed public `RoutePlan` input before mapping to VDA types
- [x] narrow or restate compatibility claims unless protocol coverage is materially deeper
- [x] add dedicated tests per major header/module
- [x] add malformed property and malformed workspace tests
- [x] add multi-robot conflict tests
- [x] add scheduler/priority/right-of-way tests
- [x] add compatibility-layer tests against representative VDA `3.0.0` scenarios
- [x] add tests for malformed `RoutePlan` inputs and inconsistent route shapes
- [x] add tests for repeated-zone routes and zone re-entry during progress updates
- [x] add tests for bounded-capacity saturation beyond two robots

---

## Next Document

If the project needs a next planning phase, create a new document for:

- production readiness
- API cleanup
- docs/examples expansion
- operational validation
- VDA depth expansion

Do not reopen this file by mixing completed implementation history with future maturity work.
