# ADR 0007: The release and the reported version

How Minkar names its releases and what version it reports to apps, for anyone touching
`SystemVersion.plist`, availability checks or gate BC. Status: accepted, 2026-10-25
(extended 2026-10-30; the rise rule changed 2026-11-20 by D1 and D2); recorded 2026-11-21.

## Context

Apps compare the system's version against what they need: `@available`,
`__builtin_available`, `NSProcessInfo.operatingSystemVersion`, `LSMinimumSystemVersion` and
scripts. Minkar's kernel is Darwin 19.6.0 while the API it implements grows by the symbols
the designated apps use, at any API level.

## Decision

- **The release follows the kernel:** Minkar 19.6.0 on Darwin 19.6.0, 25.x on Darwin 25.
  `ProductUserVisibleVersion` and `sw_vers` show it; `uname -r` and `kern.osrelease` say the
  kernel's version.
- **`ProductVersion` is the compatibility value** (10.15 today), never the release.
  `@available`, `NSProcessInfo`, LaunchServices and libSystem's
  `_availability_version_check` read it.
- **It rises by app sets (D2):** to the minimum version a group of designated apps requires
  when that group passes gate BC, not when an API level is complete (D1).
- **Every missing symbol is a noisy stub**, so `@available` taking a missing API as present
  shows in the log instead of failing silently.
- **Old binaries:** a `SystemVersionCompat`-style mechanism and `SYSTEM_VERSION_COMPAT=1`
  give binaries linked against an older SDK the older version (still to do).
- **Each dylib's versions** follow the API level `ProductVersion` reports, not the release.

Rejected: raising `kern.osrelease` (it would lie about the kernel's ABI), reporting 10.15
forever (it blocks apps that require more), and putting the release in `ProductVersion`
(`@available` would take missing APIs as present).

## Consequences

- `MACOSX_DEPLOYMENT_TARGET` stays 10.15 while `ProductVersion` is 10.15.
- Software that derives the platform version from Darwin's number is recorded in the
  compatibility matrix ([Compatibility.md](../Compatibility.md)).
