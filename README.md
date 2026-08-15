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

## Process inspection

Phase 1 provides single-process inspection through procfs:

```bash
./build/debug/lx process $$
./build/debug/lx doctor
```

Command arguments matching common secret option names are redacted by default.
Use `--raw-command` only when the unmodified process command is explicitly
required. Process listing and process actions are not implemented yet.

## Port inspection

Phase 2 queries the kernel through `NETLINK_SOCK_DIAG` without invoking `ss`,
`netstat`, or another command:

```bash
./build/debug/lx port
./build/debug/lx port 8080
```

TCP listeners and unconnected bound UDP sockets are shown for IPv4 and IPv6.
Process ownership remains `unresolved` until Phase 3 maps socket inodes to PIDs.

## Design specification

[`LX_CPP17_Linux_Resource_Manager_Design.md`](LX_CPP17_Linux_Resource_Manager_Design.md)
is the authoritative product, architecture, security, testing, and development
specification. Changes must preserve its layer boundaries and no-shell rule.

## License

Licensed under the Apache License, Version 2.0. See [`LICENSE`](LICENSE).
