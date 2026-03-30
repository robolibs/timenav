# VDA Compatibility

## Scope

`timenav` uses VDA5050 `3.0.0` as its only interoperability reference.

The compatibility layer is intentionally partial.

It exists to:

- expose recognizable VDA-shaped transport objects
- map internal route and runtime state into those objects
- let integrators bridge into systems that already reason in VDA terms

It does not currently claim:

- full schema parity
- full transport/runtime ownership
- complete protocol behavior coverage

## Practical Interpretation

Treat the VDA layer as:

- a compatibility projection
- a transport-adjacent helper
- not the canonical domain model of the library

The internal model remains richer in several areas, especially around zone hierarchy, claims, leases, and scheduling.
