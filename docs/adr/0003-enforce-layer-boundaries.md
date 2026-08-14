# ADR 0003: Enforce layer boundaries

## Status

Accepted.

## Context

LX will expose both CLI and TUI presentations over several Linux resource
providers. Direct presentation-to-kernel coupling would duplicate behavior and
make partial results difficult to test.

## Decision

Presentation calls application services, application services depend on domain
models and provider contracts, and Linux adapters implement those contracts.
Native handles and protocol structures never enter the application or domain
models.

## Consequences

`main` is the composition root. Application tests use fake providers, adapter
tests focus on Linux behavior, and CLI/TUI tests operate through application
interfaces.

