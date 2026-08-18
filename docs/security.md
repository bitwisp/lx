# Security model

LX reads Linux kernel and systemd interfaces directly and does not execute
shell commands. Its default process output redacts common secret-bearing
arguments; unredacted command lines require the explicit `--show-secrets`
option. LX never reads or prints full process environments.

Official release packages enable stack protection, `_FORTIFY_SOURCE=2`, PIE,
full RELRO, immediate symbol binding, and a non-executable stack. The
`elf_hardening` test verifies the resulting ELF properties. CI also runs
AddressSanitizer, UndefinedBehaviorSanitizer, ThreadSanitizer, CodeQL, the
dependency review action, and the source-level no-shell policy gate.

Third-party source dependencies and CI actions are pinned to immutable commit
identifiers. Dependency upgrades must be explicit reviewable changes that run
the complete test and packaging matrix.

## Release review checklist

- Run all debug, release, sanitizer, package, and hardening tests.
- Inspect DEB and RPM dependency metadata and verify their SHA-256 files.
- Complete package install, upgrade, and removal tests in disposable images.
- Confirm JSON output remains free of diagnostics and secrets by default.
- Review CodeQL and dependency-review results before publishing artifacts.
- Compare benchmark JSON with previous releases and investigate material
  regressions; Phase 10B does not enforce a numeric performance threshold.

Report suspected vulnerabilities privately to the project maintainers. Do not
include credentials, tokens, private process command lines, or other secrets in
reports.
