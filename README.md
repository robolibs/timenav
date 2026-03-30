# timenav

`timenav` is a header-only C++ navigation and coordination library built on top of `zoneout`.

It currently provides:

- workspace indexing and coordinate helpers
- traffic/property parsing
- route planning
- claims and leases
- coordinator/scheduling primitives
- partial VDA5050 `3.0.0` compatibility mappings

Project status:

- buildable and tested
- usable as a foundation
- not yet a claim of full VDA5050 parity or production-finished coordination middleware

Development commands:

```sh
make build
make test
```

More detailed documentation lives in [book/](./book).
