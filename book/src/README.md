# Overview

`timenav` is a header-only C++ coordination library for shared robot workspaces.

It is built around three layers:

1. a core domain model over `zoneout`
2. planning, traffic, claim, lease, and coordination behavior
3. a partial VDA5050 `3.0.0` compatibility layer

The library is intended to preserve workspace richness instead of flattening it into a thin transport model.

Core dependencies:

- `zoneout` for workspace, zones, nodes, and edges
- `concord` for coordinate/frame conversion
- `datapod` for value/container conventions

This book is intentionally documentation-only. It is not wired into the build.
