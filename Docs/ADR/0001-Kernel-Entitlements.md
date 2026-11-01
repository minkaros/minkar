# ADR 0001: Kernel entitlement validation

Status: accepted, 2026-11-01. Implements section 9.1 of
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
  IOHIDSystem's server connection (`SHORTCUT(WS0)` in `IOHIDUserClient.cpp`);
- most system binaries are not signed at all: only app bundles are, ad hoc,
  by `minkar.app.mk` with `Developer/Default.xctoolchain/codesign/codesign.py`.

On x86_64 XNU's own trust cache code is a stub (`trust_cache_init` and the
`pmap_*trust_cache*` functions in `osfmk/x86_64/pmap.c`). The format in
`osfmk/kern/trustcache.h` (version 1: entries sorted by CDHash, each with a
hash type and flags) is still usable.

## Decision

### 1. Every system binary is signed

- `bsd.prog.mk` and `bsd.lib.mk` gain the `ENTITLEMENTS` variable that
  `minkar.app.mk` already has. Installed executables are signed ad hoc with
  `codesign.py`, with an identifier `org.minkar.<name>` (bundle code keeps its
  `CFBundleIdentifier`) and the entitlements, if any.
- Code not built by these rules (LLVM, third-party build systems) is signed
  by the image step below, ad hoc, without entitlements.

### 2. The image build writes a static trust cache

- A new tool, `tools/mktrustcache.py`, runs in `Image/Makefile` before the
  disk image is made. It walks the sysroot, signs every Mach-O executable
  and library that has no signature, reads each CodeDirectory, computes its
  CDHash (SHA-256 of the CodeDirectory, first 20 bytes) and writes a version 1
  trust cache, sorted, with a fresh UUID.
- It is installed at `/System/Library/Security/TrustCache` (root-owned,
  mode 0644). The image build checks that every Mach-O file in the sysroot
  is in it.

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
  is refused, as on the reference platform: the policy returns an error with
  a reason, the process does not start, and the console names the refused
  entitlement. No entitlement reader, in the kernel or in user space
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

When the tests pass, `SHORTCUT(WS0)` in `IOHIDUserClient.cpp` is removed and
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
