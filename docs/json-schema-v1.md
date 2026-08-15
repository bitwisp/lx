# LX JSON schema version 1

JSON output is a public interface. Compatible fields may be added in version 1,
but removing, renaming, or changing the type of a documented field requires a
new `schema_version`.

## Envelopes

Finite successful commands write one JSON document to standard output:

```json
{
  "schema_version": 1,
  "command": "process",
  "operation": "inspect",
  "data": {},
  "warnings": []
}
```

Failures replace `data` and `warnings` with an `error` object containing
`code`, `message`, `system_error`, `component`, and `operation`. The process
exit code remains authoritative. JSON errors are written to stdout; diagnostic
tracing remains on stderr.

`log --follow --json` writes newline-delimited JSON. Every line is a complete
envelope with `command: "log"`, `operation: "follow"`, and `data.entry`.

## Command data

| Command | Operation | Data fields |
|---|---|---|
| doctor | inspect | `checks` |
| status | read | `status` (`hostname`, `cpu_percent`, `memory_total_bytes`, `memory_used_bytes`, `uptime_milliseconds`) |
| process | list | `processes` |
| process | inspect | `process` |
| port | list / inspect | `ports` |
| service | list | `services` |
| service | inspect | `service` |
| log | read | `entries` |
| inspect | inspect | `root`, `ports`, `processes`, `services`, `recent_logs` |
| find | search | `services`, `processes`, `ports`, `executables` |

Optional scalar values are `null`, collections are arrays, byte sizes and Unix
microsecond timestamps are integers, and enum values are lowercase strings.
Process arguments use the same best-effort secret redaction as human output;
only `process PID --raw-command --json` disables it.

Process objects include `cpu_percent`. It is `null` when no valid pair of CPU
samples is available, and may exceed 100 when a process uses multiple logical
CPUs.

JSON is intentionally rejected for process signals, port release, and service
lifecycle actions so that a machine-output flag can never imply authorization
for a destructive operation.
