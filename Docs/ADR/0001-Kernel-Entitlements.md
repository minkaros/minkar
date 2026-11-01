# ADR 0001: Kernel entitlement validation

Status: accepted, 2026-11-01; implemented the same day (see the end). Implements section 9.1 of
`Docs/WindowServer-Rewrite.md` (option C, decided 2026-10-31). Required
before WS4a, where WindowServer stops running as root.

## Context

XNU reads a process's entitlements only when the process is marked
`CS_SIGNED` (`cs_entitlements_dictionary_copy`, `cs_entitlements_blob_get`
in `bsd/kern/kern_cs.c`). That mark comes from the code integrity policy
module: `ubc_cs_blob_add` gives the blob `CS_VALID`, then calls
`mac_vnode_check_signature`, and the policy adds `CS_SIGNED` and, for system
code, `CS_PLATFORM_BINARY` to the flags it returns. Minkar has no such module,
so:

- no process is `CS_SIGNED`, and no process has entitlements;
- IOKit's entitlement checks (`IOUserClient::copyClientEntitlement`,
  `IOTaskHasEntitlement`) always fail, which is why root may open
  IOHIDSystem's server connection (`SHORTCUT(WS4a)` in `IOHIDUserClient.cpp`);
- most system binaries are not signed at all: only app bundles are, ad hoc,
  by `minkar.app.mk` with `Developer/Default.xctoolchain/codesign/codesign.py`.

On x86_64 XNU's own trust cache code is a stub (`trust_cache_init` and the
`pmap_*trust_cache*` functions in `osfmk/x86_64/pmap.c`). The format in
`osfmk/kern/trustcache.h` (version 1: entries sorted by CDHash, each with a
hash type and flags) is still usable.

## Decision

### 1. Every system binary is signed

- A program that needs entitlements is signed ad hoc with `codesign.py` when
  it is installed, with its entitlements: app bundles through
  `minkar.app.mk` (`ENTITLEMENTS`), other programs in their own Makefile
  (the new WindowServer, `hidevents`, `cstest`).
- Every other executable is signed by the image step below, ad hoc, without
  entitlements, with the identifier `org.minkar.<name>` (bundle code keeps
  its `CFBundleIdentifier`).

### 2. The image build writes a static trust cache

- A new tool, `tools/mktrustcache.py`, runs in `Image/Makefile` before the
  disk image is made. It walks the sysroot and signs every Mach-O
  executable (and dyld) that has no signature, or whose code changed after
  it was signed (keeping its identifier and entitlements). It reads each
  CodeDirectory, computes its CDHash (SHA-256 of the CodeDirectory, first
  20 bytes) and writes a version 1 trust cache, sorted, with a UUID derived
  from its entries (the same sysroot gives the same file). Libraries are not
  signed yet: only executables carry entitlements, and library validation is
  out of scope.
- It is installed at `/System/Library/Security/TrustCache` (root-owned,
  mode 0644), with every executable of the sysroot in it, except the paths
  given with `--exclude` (cstest's copies that must stay outside).

### 3. A MAC policy kext marks system binaries

- New kext `Kernel/Extensions/MinkarCodeIntegrity`
  (`org.minkar.driver.MinkarCodeIntegrity`), clean-room code, BSD-2. It is in
  the kernelcache with `AppleSecurityExtension`, so
  `bootstrapLoadSecurityExtensions` loads it before BSD starts and it
  registers before the first `exec` (launchd). It cannot be unloaded.
  `KLDBootstrap::loadSecurityExtensions` (`libsa/bootstrap.cpp`) loads only
  bundle IDs that start with `com.apple.`; Minkar changes it to accept
  `org.minkar.` as well, so the kext keeps a Minkar name.
- It implements `mpo_vnode_check_signature` only:
  - CDHash in the trust cache: adds `CS_SIGNED | CS_PLATFORM_BINARY` (the
    kernel then sets `csb_platform_binary`).
  - Any other valid signature, ad hoc included: adds `CS_SIGNED`, without
    `CS_PLATFORM_BINARY`.
  - Unrestricted and restricted entitlements: see section 4.
- The trust cache is read once, on the first call (the root volume is
  mounted by then), with `vnode_open` and `vn_rdwr`, checked (version 1,
  sorted, size matches `num_entries`) and kept in memory. Lookup is a binary
  search. If the file is missing or malformed the kext logs it once and no
  binary is a platform binary; the system still boots.
- A place is kept for certificate signatures verified with corecrypto
  (Phase 7): a signature with a CMS blob is treated as ad hoc until then.

### 4. Restricted entitlements

- Unrestricted: names that start with `com.apple.security.` (sandbox,
  hardened runtime exceptions, `get-task-allow`, file, network and device
  access). Any valid signature may carry them.
- Restricted: every other name, such as `com.apple.hid.system.server-access`
  and the private ones. Only platform binaries may carry them.
- A signature outside the trust cache that carries a restricted entitlement
  is refused, as on the reference platform: the policy returns an error and
  a fatal failure reason, the process is killed at exec (SIGKILL, with a
  code signing exit reason), and the console names the refused entitlement.
  A plain error is not enough: without enforcement the kernel would run the
  binary as unsigned. No entitlement reader, in the kernel or in user space
  (`SecTask` reads the raw blob through `csops`), can then see it, so XNU's
  readers need no change.

### 5. Tests

A test program, `Tests/codesign/cstest`, run by a local image test and by
a new boottest gate step:

1. The copy in the trust cache reports `CS_SIGNED | CS_PLATFORM_BINARY`
   through `csops(CS_OPS_STATUS)` and, as a non-root user, opens IOHIDSystem's
   server connection (restricted entitlement).
2. The same binary copied and re-signed (new CDHash, outside the cache) is
   refused at `exec`.
3. An ad-hoc signed binary with `com.apple.security.get-task-allow` keeps it
   (`csops(CS_OPS_ENTITLEMENTS_BLOB)`).
4. An unsigned binary still runs and is not `CS_SIGNED`.

When the tests pass, `SHORTCUT(WS4a)` in `IOHIDUserClient.cpp` is removed and
the WindowServer job (WS4a) carries `com.apple.hid.system.server-access`.

## Alternatives considered

- Running such a binary and hiding its restricted entitlements: it needs an
  XNU change so that both the entitlements dictionary and the raw blob
  returned by `csops` leave the restricted names out for non-platform code.
- A `com.apple.` bundle ID for the kext: no kernel change, but Minkar's own
  code would carry a foreign name.
- Delivering the trust cache through the EFI loader, as boot data: no
  file on the root volume, but it needs a loader change and a boot-data
  reader in the kernel. Possible later, with a sealed system volume.

## Not in scope

- Library validation, hardened runtime enforcement and the sandbox.
- Protecting the trust cache file beyond root ownership (a sealed system
  volume is a later topic).
- Certificate signatures and provisioning profiles (Phase 7).

## Implementation

Done on 2026-11-01 (`57c6d5734b`, `48ee7a4288`, `24412d4182`, `c1cda1f1ae`, `f77748bbb7`, `8366b8d0f6`):

- XNU loads security extensions with `org.minkar.` bundle IDs early
  (`libsa/bootstrap.cpp`).
- `Kernel/Extensions/MinkarCodeIntegrity`, in the kernelcache.
- `tools/mktrustcache.py` (host tests: `tools/check/test_mktrustcache.py`),
  run by `Image/Makefile`.
- `cstest` (`Kernel/Extensions/MinkarCodeIntegrity/tests`), checked by gate
  M2 and a local image test: the copy in the cache is a platform binary
  with its restricted entitlement and IOHIDSystem accepts it as server, as
  nobody; the re-signed copy is killed at exec; the ad-hoc copy keeps
  `com.apple.security.get-task-allow` and IOHIDSystem refuses it; the
  unsigned copy runs, unsigned.
- `SHORTCUT(WS4a)` removed from `IOHIDUserClient.cpp`: root no longer may
  open IOHIDSystem's server connection. Both window servers and
  `hidevents` (now installed in the image, for the input tests) are signed
  with `com.apple.hid.system.server-access`.
