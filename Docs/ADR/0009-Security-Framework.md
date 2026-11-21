# ADR 0009: Security.framework

How Minkar provides the Security framework, for anyone working on keychains, certificates,
authorization or code signing queries. Status: accepted, 2026-11-09; recorded 2026-11-21.

## Context

Every designated app imports Security. The reference platform publishes its Security
sources under the APSL, but they are large, need their own daemons and build system, and
were not needed all at once. The apps' first needs were few: keychain passwords,
certificates and keys, authorization and code signing queries.

## Decision

- **Now:** Minkar's own subset with the public API: authorization for administrators by
  rule (no authentication dialog yet); keychain passwords through `SecItem` and the older
  calls, kept in a file only the user can read and not encrypted yet; keys and
  certificates on libcrypto; digest and verify transforms; code signing queries that
  report code as not signed.
- **Later:** the APSL Security sources (in `Frameworks/Security`) replace the subset, with
  an encrypted keychain service ([Compatibility.md](../Compatibility.md#system-services-minkar-lacks)).

## Consequences

- Apps link and run now; secrets are protected only by file permissions until the APSL
  sources come in.
- The subset's interface is the public one, so replacing it does not change the apps.
