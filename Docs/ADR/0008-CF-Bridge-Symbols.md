# ADR 0008: Where the toll-free bridged classes live

Which library exports the toll-free bridged classes binaries import from CoreFoundation,
for anyone working on CF-Lite, Foundation or dyld. Status: accepted, 2026-11-09 (option A);
recorded 2026-11-21.

## Context

Binaries built for the reference platform import some Objective-C classes and constants of
the bridge between CoreFoundation and Foundation from CoreFoundation's install name. In
Minkar, CF-Lite is plain C and the bridged classes are Foundation's, so those imports did
not resolve and the apps did not load.

## Decision

Option A: the constants move to CoreFoundation, and the classes stay in Foundation, which
dyld searches when a binary imports them from CoreFoundation and CoreFoundation does not
have them (a fallback in Minkar's dyld, `8c06f071ed`). The same fallback finds in AppKit
what binaries import from Foundation and Minkar keeps in AppKit.

Rejected: moving the classes into CoreFoundation, which would make CF-Lite depend on the
Objective-C runtime and on Foundation's class hierarchy.

## Consequences

- Install names stay those of the reference platform; no symbol is duplicated.
- A new bridged symbol goes where the rule says: a constant to CoreFoundation, a class to
  Foundation.
