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
