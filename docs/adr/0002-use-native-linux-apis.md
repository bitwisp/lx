# ADR 0002: Use native Linux APIs

## Status

Accepted.

## Context

Parsing output from traditional administration tools is locale-sensitive,
hard to test, and vulnerable to shell-injection mistakes.

## Decision

LX implements resource operations through procfs, Netlink SOCK_DIAG, Linux
system calls, systemd D-Bus, and sd-journal. Production resource paths must not
execute shell commands or wrap existing administration programs.

## Consequences

Adapters require careful protocol parsing, RAII, errno preservation, race
handling, and integration tests. A source gate enforces the decision while the
project matures.

