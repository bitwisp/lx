# LX development guide

## Prerequisites

- Linux
- CMake 3.20 or newer
- Ninja
- GCC or Clang with C++17 support
- Git and network access for the first dependency fetch
- `libsystemd` development headers for the Phase 5 service backend

On Ubuntu or Debian, install the service backend dependencies with:

```bash
sudo apt install pkg-config libsystemd-dev
```

On RHEL, Rocky Linux, or AlmaLinux, use:

```bash
sudo dnf install pkgconf-pkg-config systemd-devel
```

The dependency remains optional. If headers or the library are unavailable,
the build reports `systemd sd-bus backend: unavailable`, keeps port/process
features, and wires a provider that returns a structured unavailable error.
Use `-DLX_ENABLE_SYSTEMD=OFF` to disable detection explicitly.

## Configure, build, and test

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Preset builds are written to `build/debug`, `build/release`, `build/asan`, and
`build/ubsan`. Run `./build/debug/lx`; a separate CLion
`cmake-build-debug-wsl` directory is not updated by these commands. Configure
CLion with the matching CMake preset or rebuild that directory before running
its executable.

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

Phase 2 deliberately stopped before socket inode-to-PID resolution. Integration
tests create their own sockets and skip explicitly when the execution sandbox
prohibits AF_INET.

## Phase 3 socket ownership

`PortService` now coordinates `ISocketProvider`, `ISocketOwnerResolver`, and
`IProcessProvider`. `SocketInodeResolver` gathers the requested non-zero inodes,
scans the numeric `/proc/<pid>/fd` directories once, and recognizes strict
`socket:[inode]` symbolic-link targets. It records every PID because file
descriptors can be inherited or shared.

`lx port` aggregates shared process names and PIDs on the socket row. A process
that exits or becomes inaccessible during the snapshot does not remove the
socket: its PID remains visible, its name is shown as `<unavailable>`, and a
warning is written to standard error. Sockets with no visible owner remain
`unresolved`.

Phase 3 deliberately stopped before process signals and port release, and it
still left systemd association, process lists, and JSON output for later work.

## Phase 4 process signals

The signal path follows `CLI -> ProcessService -> ISignalProvider ->
LinuxSignalProvider`. `PidFd` owns native process handles with RAII. The Linux
adapter probes pidfd support at runtime, uses `pidfd_send_signal` when possible,
and falls back to `kill(2)` only when pidfd is unsupported. `lx doctor` reports
which path is available.

`ProcessService` rejects PID 1, non-positive PIDs, and LX's own PID before a
provider is called. `lx process stop PID` sends SIGTERM. `lx process kill PID`
requires explicit confirmation by default.

`lx port free PORT` snapshots every socket and owner, displays the targets, and
revalidates the inode-to-PID relationship after confirmation. It sends SIGTERM
to all unique owners and waits up to three seconds. If the original owners
still hold the port, a second default-no confirmation allows SIGKILL. A new
inode or owner aborts the action with a conflict instead of signaling the new
process. Unresolved ownership also aborts the entire operation.

Signal integration tests fork their own child processes and always reap them.
They never target an unrelated host process. Phase 4 still leaves systemd-aware
stopping, configuration-driven confirmation, process lists, and JSON for later
phases.

## Phase 5 systemd services

The service path follows `CLI -> ServiceService -> IServiceProvider ->
SystemdServiceProvider`. The Linux adapter connects directly to the system bus
with sd-bus and uses `ListUnits`, `GetUnit`, `GetUnitByPID`, `StartUnit`,
`StopUnit`, and `RestartUnit`. It never parses service-manager command output.
All bus connections, messages, and errors have C++ RAII ownership.

Available commands are:

```bash
./build/debug/lx service
./build/debug/lx service nginx
./build/debug/lx service nginx start
./build/debug/lx service nginx stop
./build/debug/lx service nginx restart --yes
./build/debug/lx svc nginx
```

Missing suffixes are normalized to `.service`. Stop and restart require a
default-no confirmation unless `--yes` is present. Start submits immediately.
The commands report successful job submission; they do not claim that the
asynchronous systemd job has already completed.

Process inspection now uses `GetUnitByPID` and shows the associated service.
Port rows aggregate the services of their visible owners. `lx port free PORT`
recommends stopping a service only when every owner is readable and all owners
map to exactly the same `.service`. It revalidates socket inode and PID targets
before stopping. A service error or ownership change aborts the operation and
never silently falls back to process signals.

`lx doctor` distinguishes an available backend from an inaccessible bus or
missing systemd manager. On WSL/containers without a system service manager,
service commands are unavailable while process and port features continue to
start normally. The systemd integration test performs read-only queries and
skips explicitly when the manager cannot be reached; it never controls a real
host service.

Phase 5 deliberately defers reload, enable/disable, service process trees,
journal queries, and JSON output.

## Phase 6 journal logs

The log path follows `CLI -> LogService -> IJournalProvider ->
SystemdJournalProvider`. The Linux adapter opens the local journal directly
with sd-journal, applies structured `_SYSTEMD_UNIT` and `_PID` matches, and
reads `_COMM`, `MESSAGE`, `PRIORITY`, cursor, and realtime timestamp fields.
It does not invoke or parse an external log command.

Finite queries include:

```bash
./build/debug/lx log nginx
./build/debug/lx log nginx --lines 100
./build/debug/lx log nginx --since "2026-08-15 10:30:00"
./build/debug/lx log nginx --since 10m
./build/debug/lx log --pid 1234
./build/debug/lx log nginx --pid 1234
```

A service, PID, or both are required. Missing service suffixes are normalized
to `.service`; combined filters use AND semantics. `--lines` defaults to 50
and accepts 1 through 10000. Absolute timestamps are interpreted in local time.
Compact relative times support seconds, minutes, hours, and days (`30s`,
`10m`, `2h`, `3d`).

Follow mode prints the same initial recent entries, then waits through
`sd_journal_wait` rather than polling with sleeps:

```bash
./build/debug/lx log nginx --lines 20 --follow
```

Ctrl-C sets a signal-safe stop flag, releases the journal through RAII, restores
the previous signal handler, and returns exit code 130. Log messages are shown
as one terminal line; embedded newlines, tabs, escape sequences, and other
control bytes are escaped before output.

`lx doctor` probes journal access independently from the system service
manager. A container can therefore report systemd service management as
unavailable while still allowing local journal queries. Read-only integration
tests never add journal records and skip explicitly when journal files are
missing or inaccessible.

Phase 6 does not add global unfiltered logs, priority/boot filters, cursor
resume, complex natural-language dates, JSON output, or recent logs in
`inspect`; inspect integration begins in Phase 7.

## Phase 7 unified inspection and search

Process enumeration scans numeric procfs directories and treats processes that
exit or become unreadable during the scan as partial-result warnings. Lists can
be filtered with exact process names, user names or numeric UIDs, and normalized
systemd service names:

```bash
./build/debug/lx process
./build/debug/lx process --name nginx
./build/debug/lx process --user 1000
./build/debug/lx process --service nginx
```

`ResourceResolver` gives `inspect` predictable resource selection. Explicit
targets use `port:`, `pid:`, or `service:`. A bare number is probed as both a
port and PID and returns conflict exit code 7 when both exist. A bare name
prefers an exact normalized service and otherwise requires a unique exact
process name.

```bash
./build/debug/lx inspect port:8080
./build/debug/lx inspect pid:1234
./build/debug/lx inspect service:nginx
./build/debug/lx inspect nginx
```

`InspectService` builds a bounded, in-memory resource graph linking listening
ports, owning processes, services, and at most 20 recent journal entries.
Failure of the requested root resource is fatal; unavailable related data is
reported as a warning without hiding the root result.

`find` performs case-insensitive substring searches across observable service
units and descriptions, process names, users, arguments, executables, and port
numbers. Related ports are included when their process or service matches:

```bash
./build/debug/lx find nginx
./build/debug/lx find 8080
```

Phase 7 remains read-only except for the existing explicit process, port, and
service actions. CPU sampling, JSON output, quiet/no-color automation options,
and full-disk executable search remain outside this phase.

## Phase 8 JSON and automation

All major read-only commands accept `--json` before or after the subcommand.
They emit a versioned envelope to stdout and never mix human warnings into the
document:

```bash
./build/debug/lx process --json
./build/debug/lx port 8080 --json
./build/debug/lx inspect service:nginx --json
./build/debug/lx find nginx --json
```

Errors are also JSON and retain the documented exit codes. Destructive actions
reject JSON mode before performing an operation. Follow mode uses NDJSON so
every received line remains valid after interruption:

```bash
./build/debug/lx log nginx --follow --json
```

`--quiet` suppresses non-essential human warnings and successful action
messages while retaining query data, errors, and confirmation prompts.
`--no-color` and a non-empty `NO_COLOR` environment variable disable ANSI
styling. JSON warnings remain part of the document even with `--quiet`.

The normative field contract is documented in
[`json-schema-v1.md`](json-schema-v1.md). Breaking field changes require a new
schema version.

## Phase 9 metrics and terminal dashboard

Host metrics are read directly from `/proc/stat`, `/proc/meminfo`, and
`/proc/uptime`; the hostname comes from `gethostname(2)`. CPU percentages use
two samples. Host idle time includes both idle and I/O wait. Process samples
are matched by PID and procfs start time so PID reuse cannot transfer CPU usage
to a new process. Process CPU follows `top` semantics and may exceed 100% on a
multi-core system.

The read-only commands work in every build:

```bash
./build/debug/lx status
./build/debug/lx status --json
./build/debug/lx process
```

The FTXUI dashboard is optional and does not affect the normal debug or release
targets. Configure and run it with:

```bash
cmake --preset tui-debug
cmake --build --preset tui-debug
ctest --preset tui-debug
./build/tui-debug/lx tui
```

When a TUI-enabled executable is run without arguments in an interactive
terminal, it opens the dashboard automatically. `lx tui` is always explicit;
a non-TUI build returns an unavailable error instead of silently changing
behavior. FTXUI is pinned to version 7.0.1.

The dashboard publishes immutable snapshots from a worker thread every 1.5
seconds. Host, process, and port data refresh on each cycle; systemd services
refresh at most every 3 seconds. A failing source keeps its previous data,
marks it stale, and exposes a warning instead of clearing unrelated panels.

Controls:

| Key | Action |
|---|---|
| `Tab`, Left, Right | Switch Services, Ports, and Processes panels |
| Up, Down | Select a resource |
| Enter | Inspect the selected resource graph |
| `/` | Find resources |
| `l` | Show recent logs for a selected service or process |
| `s`, `r` | Confirm stop or restart of the selected service |
| `k` | Confirm SIGTERM for the selected process |
| `K` | Double-confirm SIGKILL for the selected process |
| `F1` | Show keyboard help |
| Esc, `q` | Close a view or leave the dashboard |

The port panel deliberately has no release shortcut. Port release remains the
explicit `lx port free PORT` CLI workflow with ownership revalidation.

Thread-sensitive changes can be checked with the TUI-enabled sanitizer preset:

```bash
cmake --preset tsan
cmake --build --preset tsan
ctest --preset tsan
```

## Phase 10A installation and Debian packages

LX installs according to the standard GNU/Linux filesystem layout:

```text
/usr/bin/lx
/usr/share/man/man1/lx.1
/usr/share/bash-completion/completions/lx
/usr/share/zsh/site-functions/_lx
/usr/share/fish/vendor_completions.d/lx.fish
/usr/share/doc/lx/
```

The normal `debug` and `release` presets remain unchanged. The dedicated
`package-release` preset builds version `0.1.0` in Release mode with LTO and
TUI support enabled:

```bash
cmake --preset package-release
cmake --build --preset package-release
ctest --preset package-release
cpack --config build/package-release/CPackConfig.cmake -G DEB
```

The package is generated under `build/package-release/packages` together with
its SHA-256 checksum. CPack invokes `dpkg-shlibdeps` to derive the actual libc,
libstdc++, and libsystemd runtime constraints from the binary. FTXUI is linked
into LX and is not a separate runtime dependency.

Inspect or install the package on Debian or Ubuntu with:

```bash
dpkg-deb --info build/package-release/packages/lx-resource-manager_0.1.0_*.deb
dpkg-deb --contents build/package-release/packages/lx-resource-manager_0.1.0_*.deb
sudo apt install ./build/package-release/packages/lx-resource-manager_0.1.0_*.deb
```

`install_layout` installs into an isolated build directory and runs the staged
binary. In `package-release`, `deb_package` additionally generates, inspects,
extracts, and runs the DEB without modifying the host package database. CI
publishes the tested DEB and checksum as one artifact.

An unpackaged staging install can be tested with:

```bash
cmake --install build/package-release --prefix build/staging
./build/staging/bin/lx --version
```

RPM, package upgrade tests, benchmarks, release security review, configuration
files, and service reload/enable/disable remain Phase 10B work.
