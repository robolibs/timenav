# Modules

## `ids.hpp`

Strong typed identifiers for robots, missions, claims, and leases.

## `workspace_index.hpp`

Indexes:

- zone lookup
- node lookup
- edge lookup
- parent/child/ancestor/descendant zone traversal
- zone memberships for nodes and edges
- workspace property access
- coordinate conversions
- validation issues

## `zone_policy.hpp`

Defines:

- `ZonePolicy`
- `EdgeTrafficSemantics`
- parsing and validation helpers
- policy merge rules
- effective edge semantic derivation

## `route.hpp`

Defines:

- route steps and route plans
- graph traversal
- blocked/penalized planning
- route extraction
- planning diagnostics

## `claim.hpp` and `claim_manager.hpp`

Defines:

- claim requests
- leases
- compatibility checks
- evaluation results
- refresh/release/revoke/expiry behavior

## `robot_state.hpp` and `coordinator.hpp`

Defines:

- robot progress state
- route/claim derived scheduling helpers
- rolling horizon claims
- progress-based release
- queue vs replan decisions

## `vda/`

Defines partial compatibility structs and projection helpers for:

- orders
- state
- connection
- instant actions
- factsheet
- responses
