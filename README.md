# LX — Linux Resource Manager

LX is a resource-oriented Linux inspection and management tool written in
C++17. It will provide a unified view of sockets, processes, systemd services,
and journal entries through native Linux APIs, without shelling out to
traditional system utilities.

The project is being built incrementally. The current milestone is repository
foundation work; resource providers are not implemented yet.

## Build and test

The development build requires CMake 3.20 or newer, Ninja, a C++17 compiler,
Git, and network access the first time pinned dependencies are fetched.

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Additional presets are available for `release`, `asan`, and `ubsan` builds.
See [`docs/development.md`](docs/development.md) for the complete workflow.

## Design specification

[`LX_CPP17_Linux_Resource_Manager_Design.md`](LX_CPP17_Linux_Resource_Manager_Design.md)
is the authoritative product, architecture, security, testing, and development
specification. Changes must preserve its layer boundaries and no-shell rule.

## License

Licensed under the Apache License, Version 2.0. See [`LICENSE`](LICENSE).
