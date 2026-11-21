# ADR 0004: The app sandbox

Why Minkar has its own app sandbox and how it is built, for anyone working on the sandbox,
launchd or an app that limits itself. Status: accepted, 2026-11-13; recorded 2026-11-21.

## Context

Apps written for the reference platform limit themselves with SBPL profiles
(`sandbox_init_with_parameters`); Firefox's content, GPU and media processes refuse to run
without one. The reference platform's sandbox kext and its profile compiler are closed
source, so there is nothing to port.

## Decision

Minkar writes its own sandbox, in three phases, with the public interface the apps call:

1. **The language.** `libsystem_sandbox` compiles SBPL (a small Scheme: `define`, `if`,
   `param`, `string-append`, regex literals, `require-*`, the path, name, address and
   number filters) to a blob, with an evaluator and a regex engine shared with the kernel.
2. **Enforcement.** The `MinkarSandbox` MAC policy keeps the profile with the process's
   credential (inherited by its children and across exec, never replaced) and checks
   files, the network, sysctls and signals. launchd checks Mach service lookups and
   registrations through `sandbox_check`. Sandboxed processes get users and groups from
   `directoryd` over `xpc_pipe`, and their own folders under `/var/folders` from
   `dirhelper`, so they never read `/etc`.
3. **The rest.** IOKit user clients, file mappings, process information, preferences, and
   the reference platform's service names in profiles mapped to Minkar's. Partly done:
   the IOSurface user client and the distributed notification service.

A denial is logged unless the rule says `(with no-log)`, both for the kernel's own checks
and for those a service asks through `sandbox_check`.

## Consequences

- The profiles the apps ship run unchanged; a missing operation or filter shows up as a
  denial in the log, not as a crash.
- Phase 3 is open work ([ROADMAP.md](../ROADMAP.md#closed-phases)).
- A sandboxed helper started outside a user's launchd context cannot reach the per-user
  services; gate BC starts apps with `launchctl asuser` for that reason.
