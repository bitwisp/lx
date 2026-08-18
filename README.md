# LX — Linux Resource Manager

LX is a resource-oriented Linux inspection and management tool written in
C++17. It provides a unified view of sockets, processes, systemd services,
and journal entries through native Linux APIs, without shelling out to
traditional system utilities.

The project is built incrementally and currently includes native procfs,
INET_DIAG, pidfd, sd-bus, and sd-journal integrations.

## Build and test

The development build requires CMake 3.20 or newer, Ninja, a C++17 compiler,
Git, and network access the first time pinned dependencies are fetched.

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Additional presets are available for `release`, `asan`, `ubsan`, `tui-debug`,
and `tsan` builds.
See [`docs/development.md`](docs/development.md) for the complete workflow.

## Process inspection

Inspect or list processes through procfs:

```bash
./build/debug/lx process $$
./build/debug/lx process
./build/debug/lx process --name nginx
./build/debug/lx doctor
```

Command arguments matching common secret option names are redacted by default.
Use `--raw-command` only when the unmodified process command is explicitly
required. Process lists also support `--user` and `--service` filters.

## Status and terminal dashboard

Show sampled host CPU, memory, uptime, and per-process CPU usage:

```bash
./build/debug/lx status
./build/debug/lx status --json
./build/debug/lx process
```

Build the optional FTXUI dashboard separately:

```bash
cmake --preset tui-debug
cmake --build --preset tui-debug
./build/tui-debug/lx tui
```

It provides live Services, Ports, and Processes panels plus inspect, find,
logs, and confirmed service/process actions. The normal presets remain free of
the FTXUI dependency.

## Install, DEB/RPM packages, and benchmarks

Create the release package with TUI, LTO, the manual page, and Bash/Zsh/Fish
completion:

```bash
cmake --preset package-release
cmake --build --preset package-release
ctest --preset package-release
cpack --config build/package-release/CPackConfig.cmake -G DEB
cpack --config build/package-release/CPackConfig.cmake -G RPM
```

The packages and SHA-256 checksums are written to
`build/package-release/packages`. Install the generated DEB on Debian or Ubuntu
with:

```bash
sudo apt install ./build/package-release/packages/lx-resource-manager_0.1.0_*.deb
lx --version
lx doctor
lx tui
```

CPack derives runtime dependencies from the release binary; FTXUI is linked
into LX and does not require a separate runtime package.

Build and record the Phase 10B benchmark suite with:

```bash
cmake --preset benchmark-release
cmake --build --preset benchmark-release
build/benchmark-release/benchmarks/lx_benchmarks \
  --benchmark_out=build/benchmark-release/phase10b-benchmarks.json \
  --benchmark_out_format=json
```

## Unified resources

Inspect and search related ports, processes, services, and recent logs:

```bash
./build/debug/lx inspect port:8080
./build/debug/lx inspect pid:1234
./build/debug/lx inspect nginx
./build/debug/lx find nginx
```

Explicit prefixes avoid ambiguity. If a bare number exists as both a port and
PID, LX returns a conflict and suggests the two explicit forms.

## JSON automation

Major read-only commands provide stable schema version 1 JSON:

```bash
./build/debug/lx process --json
./build/debug/lx inspect port:8080 --json
./build/debug/lx log nginx --follow --json
```

Follow mode uses NDJSON. See
[`docs/json-schema-v1.md`](docs/json-schema-v1.md) for the public contract.

## Port inspection

Phase 2 queries the kernel through `NETLINK_SOCK_DIAG` without invoking `ss`,
`netstat`, or another command:

```bash
./build/debug/lx port
./build/debug/lx port 8080
```

TCP listeners and unconnected bound UDP sockets are shown for IPv4 and IPv6.
Phase 3 resolves each socket inode through `/proc/*/fd` and displays every
owning process name and PID. Shared descriptors remain represented as multiple
owners; inaccessible or short-lived processes produce warnings without hiding
the socket.

## Process control

Phase 4 uses native Linux signal APIs. It prefers pidfds at runtime and falls
back to `kill(2)` on older kernels:

```bash
./build/debug/lx process stop 1234
./build/debug/lx process kill 1234
./build/debug/lx port free 8080
```

SIGKILL and port release are confirmed interactively unless `--yes` is given.
PID 1, the running LX process, unresolved port owners, and changed port
ownership are always rejected; `--yes` never bypasses these protections.

## Design specification

[`LX_CPP17_Linux_Resource_Manager_Design.md`](LX_CPP17_Linux_Resource_Manager_Design.md)
is the authoritative product, architecture, security, testing, and development
specification. Changes must preserve its layer boundaries and no-shell rule.

## License

Licensed under the Apache License, Version 2.0. See [`LICENSE`](LICENSE).
