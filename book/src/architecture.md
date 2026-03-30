# Architecture

## Core Principles

- `zoneout` remains the source of truth for map/workspace data
- `timenav` adds semantics rather than replacing the workspace model
- the public transport layer is downstream of the internal domain model, not the reverse

## Main Layers

### Workspace Layer

This layer indexes zones, nodes, edges, hierarchy relationships, memberships, and coordinate metadata.

Main header:

- `include/timenav/workspace_index.hpp`

### Traffic And Policy Layer

This layer parses `traffic.*` properties into typed policies and derives effective semantics for zones and edges.

Main header:

- `include/timenav/zone_policy.hpp`

### Planning Layer

This layer traverses the graph, derives routes, applies penalties, and reports failures.

Main header:

- `include/timenav/route.hpp`

### Claim And Lease Layer

This layer evaluates resource conflicts across zones, nodes, and edges and manages lease state.

Main headers:

- `include/timenav/claim.hpp`
- `include/timenav/claim_manager.hpp`

### Coordination Layer

This layer tracks robot progress, rolling claims, reservation windows, queueing, and replan decisions.

Main headers:

- `include/timenav/robot_state.hpp`
- `include/timenav/coordinator.hpp`

### Compatibility Layer

This layer projects internal state into partial VDA5050 `3.0.0`-shaped transport objects.

Main directory:

- `include/timenav/vda/`
