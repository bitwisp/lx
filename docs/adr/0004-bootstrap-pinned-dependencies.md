# ADR 0004: Bootstrap pinned development dependencies

## Status

Accepted for pre-packaging development.

## Context

Supported distributions ship different library versions, and the initial
development environment does not yet contain all required packages.

## Decision

CMake FetchContent retrieves immutable reviewed CLI11, fmt, JSON, FTXUI,
Catch2, and Google Benchmark commits corresponding to documented releases.
Third-party targets do not inherit LX warning settings. Before distribution
packaging begins, CMake will also support consuming compatible system packages.

## Consequences

The first configure requires Git and network access, while subsequent builds
reuse the build-tree checkout. Dependency upgrades are explicit reviewed
changes instead of tracking upstream branches.

