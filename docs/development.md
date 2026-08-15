# LX development guide

## Prerequisites

- Linux
- CMake 3.20 or newer
- Ninja
- GCC or Clang with C++17 support
- Git and network access for the first dependency fetch

Phase 0 does not require libsystemd or pkg-config. Those system dependencies
will be introduced with the systemd adapter rather than imposed on the current
CLI skeleton.

## Configure, build, and test

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The same workflow supports the `release`, `asan`, and `ubsan` presets. Run a
sanitizer preset after parser, native-resource, or lifetime-sensitive changes.
The ASan test launcher disables LeakSanitizer because test discovery must also
work in ptrace-managed environments; AddressSanitizer checks remain enabled.

The no-shell policy is registered with CTest and can also be run directly:

```bash
cmake -DLX_SOURCE_DIR="$PWD" -P cmake/NoShellGate.cmake
```

## Architecture and coding style

Dependencies point inward:

```text
CLI -> Application -> Domain/Contracts <- Linux adapters
```

Presentation code must not read procfs or invoke Netlink, D-Bus, journal, or
signal APIs. Linux adapters must not format terminal output. Native handles
must use RAII and remain outside domain models.

Use C++17 value semantics, strong enums, standard containers, algorithms, and
explicit ownership. Avoid C-style buffers, manual lifetime management, global
state, and classes that merely wrap public data in boilerplate accessors.

## Git workflow

Each commit must contain one logical change, use Conventional Commits, pass the
tests available at that point, and leave unrelated local files untouched.
Feature implementation proceeds in the order defined by the design
specification; Phase 1 starts with the ProcFS reader and parsers.

## Phase 1 process support

The process path follows the project boundaries:

```text
CLI -> ProcessService -> IProcessProvider -> ProcFsProcessProvider
```

`ProcFsReader` accepts an alternate root so parser and provider tests use
isolated fixtures. The production provider reads `stat`, `status`, `cmdline`,
`exe`, and `cwd` directly from procfs. Missing core records fail the request;
optional metadata failures produce structured warnings and a partial result.

Inspect a process with:

```bash
./build/debug/lx process 1
./build/debug/lx process $$ --raw-command
```

The default output performs best-effort command-line secret redaction. LX does
not read process environments. Phase 1 deliberately excludes process listing,
filtering, signaling, service mapping, sockets, and JSON output.

## Phase 2 socket support

Socket inspection follows `CLI -> PortService -> ISocketProvider ->
NetlinkSocketProvider`. The adapter sends INET_DIAG dump requests for TCP/UDP
and IPv4/IPv6, validates multipart Netlink responses, and converts kernel types
into domain `SocketInfo` values. `lx port` lists TCP listeners and unconnected
UDP bindings; `lx port PORT` filters by local port.

Phase 2 leaves socket inode-to-PID resolution, service mapping, port
termination, and JSON output for later phases. Integration tests create their
own sockets and skip explicitly when the execution sandbox prohibits AF_INET.
