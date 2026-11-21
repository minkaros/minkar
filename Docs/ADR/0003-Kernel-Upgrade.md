# ADR 0003: Moving the kernel to Darwin 25

Status: **accepted for x86-64**, 2026-11-20, with two conditions:

1. The corecrypto license report (7.6) is resolved by the maintainer before KX1 starts.
2. Questions 7.3, 7.4 and 7.10 are closed by KX1's experiments, as section 7 says.

It is milestone KX0 of lane KX in `Docs/ROADMAP.md`, and it changes nothing in the
kernel by itself. Spanish copy: [0003-Kernel-Upgrade.es.md](0003-Kernel-Upgrade.es.md).

**Frame (decided by the maintainer, 2026-11-20):**

- **Version:** xnu-12377.121.6 (Darwin 25, distribution `macos-265`). Approved.
- **Scope:** the kernel move is defined and validated on x86-64 (QEMU, PC 1 and PC 2).
  It does not depend on arm64.
- **Timing:** KX1 starts after Minkar 19.6.0 is published and after step R1 of the
  project's restructuring (the toolchain as its own artifact, with AArch64 in LLVM;
  [Structure.md](../Structure.md)).
- **Kernel Debug Kit:** never used, neither to build nor as a reference.
- **Version strings:** until KX3, `devel` stays on Darwin 19.6.0 and does not change
  `kern.osrelease`, `uname -r`, `DARWIN_VERSION` or `TARGET_TRIPLE`. The new kernel's
  work lives on its branch, where those values become Darwin 25's. The first release on
  the new kernel is Minkar 25.x; `ProductVersion` is independent of the kernel and
  rises by app sets.

## 1. Context

- Minkar runs on Darwin 19.6.0 (xnu-6153.141.1), x86-64 only. Its goal
  since 2026-11-20 is to run the reference platform's published app
  binaries one API level behind the current one, and to reach arm64
  (decisions D1, D4 and D5).
- Darwin 19's xnu has no arm64 platform that could boot outside the
  vendor's machines, and modern libpthread, libdispatch and dyld expect
  kernel interfaces it lacks. D5 replaces the earlier rule of keeping
  Darwin 19.6.0 for API 20 and API 21.
- Minkar 19.6.0 and its 19.6.x fixes stay on Darwin 19.6.0. The move is
  lane KX: a prototype on a branch (KX1), the whole system (KX2), then
  `devel` on Darwin 25 with Darwin 19 kept on a maintenance branch (KX3,
  Minkar 25.x). Until KX3, new framework and desktop work must build on
  both kernels.
- Clean room (principle 5): only the vendor's published open source
  (`github.com/apple-oss-distributions`) and other open source are read.
  No SDK, Kernel Debug Kit, binary or disassembly.

## 2. The exact version

**xnu-12377.121.6**, the latest published Darwin 25 kernel, listed by the
`distribution-macOS` tag `macos-265` (26.5, 2026-06-17).

How it was checked (2026-11-20):

- `git ls-remote --tags https://github.com/apple-oss-distributions/xnu`
  lists the `xnu-12377.*` series. Each `distribution-macOS` tag from
  `macos-260` to `macos-265` was cloned shallow and its `xnu` gitlink
  resolved to a tag: 26.0 → xnu-12377.1.9, 26.1 → .41.6, 26.2 → .61.12,
  26.3 → .81.4, 26.4 → .101.15, 26.5 → .121.6. `macos-265` is the newest.
- The tree at `xnu-12377.121.6` declares both architectures:
  `osfmk/x86_64` and `osfmk/arm64`; `config/MASTER.x86_64`,
  `MASTER.arm64` and `MASTER.arm64.MacOSX` with their export lists;
  `SUPPORTED_ARCH_CONFIGS := X86_64 X86_64H ARM64` in
  `makedefs/MakeInc.def`.

**What this does not settle for arm64.** The published arm64 side builds
only for the vendor's own SoCs (`T6000` to `T8142`) and `VMAPPLE`; there
is no generic arm64 machine and no QEMU `virt`. For any of those machine
configurations the kernel link force-loads a support library that ships
only in the Kernel Debug Kit, which is closed, and SPTM, TXM and Exclaves
need closed libraries and headers (`sptm/*`, `Tightbeam/*`, `xnuproxy/*`).
The x86-64 build needs none of them. So Darwin 25 can be used as decided
for x86-64 (KX1 to KX3), but booting it on arm64 (ARM1) needs a machine
configuration, a platform expert, an interrupt controller and a pmap mode
of Minkar's own, written from the published tree: see section 8.

## 3. What moves with the kernel

The base Minkar takes from Darwin 19 today, against Darwin 25's
distribution (`macos-265`). Minkar's versions were checked by comparing
its files with each upstream tag.

| Component | In Minkar now | Darwin 25 | License |
| --- | --- | --- | --- |
| xnu (and libsyscall) | xnu-6153.141.1 | xnu-12377.121.6 | APSL-2.0 |
| dyld | dyld-750.6 | dyld-1378 | APSL-2.0 |
| Libc | Libc-1353.100.2 | Libc-1752.120.2 | APSL-2.0, BSD files |
| libplatform | libplatform-220.100.1 | libplatform-375.120.2 | APSL-2.0 |
| libpthread (library and kext) | libpthread-416.100.3 | libpthread-539.100.4 | APSL-2.0 |
| libdispatch | libdispatch-1173.100.2 | libdispatch-1542.100.32 | Apache-2.0 |
| objc4 | objc4-787.1 | objc4-951.7 | APSL-2.0 |
| libmalloc | libmalloc-283.100.6 | libmalloc-812.100.31 | APSL-2.0 |
| Libsystem | Libsystem-1281.100.1 | Libsystem-1356 | APSL-2.0 |
| Libinfo | Libinfo-538 | Libinfo-600 | APSL-2.0 |
| Libnotify | Libnotify-241.100.2 | Libnotify-348.120.4 | APSL-2.0 |
| IOKitUser | IOKitUser-1726.140.1 | IOKitUser-100231.120.3 | APSL-2.0 |
| kext_tools | kext_tools-623.120.1 | kext_tools-779 | APSL-2.0 |
| configd | configd-1061.141.1 | configd-1405.120.5 | APSL-2.0 |
| ICU | ICU 78.3 with the platform's extensions | ICU-76142.5.1.200 | Unicode-3.0 |
| hfs | hfs-556.60.1 (Darwin 20's, adapted) | hfs-715.120.4 | APSL |
| IOPCIFamily | IOPCIFamily-370.141.1 | IOPCIFamily-726.100.6 | APSL-1.1, APSL-2.0 |
| IOHIDFamily | IOHIDFamily-1446.140.2 | IOHIDFamily-2238.120.5 | APSL-2.0 |
| IOGraphics | IOGraphics-576.1 | IOGraphics-600 | APSL-2.0 |
| IOStorageFamily | IOStorageFamily-238.120.1 | IOStorageFamily-337.100.1 | APSL-2.0 |
| IOATAFamily | IOATAFamily-256 | IOATAFamily-264 | APSL-2.0 |
| msdosfs | msdosfs-411.100.3 | msdosfs-788.100.31 | APSL-2.0 |
| AppleFileSystemDriver | AppleFileSystemDriver-23 | AppleFileSystemDriver-31 | APSL-2.0 |
| CommonCrypto | in Libsystem | CommonCrypto-600035 | APSL-2.0 |
| AvailabilityVersions | (generated headers) | AvailabilityVersions-157.2, needed to build xnu | APSL-2.0 |

**No Darwin 25 version published:**

| Component | In Minkar now | Note |
| --- | --- | --- |
| launchd | launchd-842 | The last published release is launchd-842.92.1 |
| CoreFoundation | CF-Lite, CF-1153.18 extended | The last published release is CF-1153.18 |
| libclosure | not used: `libsystem_blocks` is swift-corelibs' BlocksRuntime (Apache-2.0) | libclosure-96 is published, if wanted |
| libunwind, libc++, compiler-rt | from the toolchain (swiftlang LLVM 21.1.6) | Not published by the vendor; the toolchain keeps providing them |
| corecrypto | PureDarwin's corecrypto | Not published; its license is an open question (7.6) |
| IOUSBFamily, AppleAPIC, IOACPIFamily, AppleI386PCI, AppleIntelPIIXATA, ApplePS2Controller | old releases | Their last published releases; Minkar keeps carrying them |
| IOAudioFamily, IONetworkingFamily, exfat | Minkar's own (BSD-2) | Not affected by the vendor's versions |

## 4. What is missing from the published sources

Found by reading the whole `xnu-12377.121.6` tree and the distribution
list. Each item says what Minkar would have to write.

1. **Kernel collections.** Darwin 25 boots from a fileset kernel
   collection made by `kmutil`, which is not published (nor
   KernelManagement or kernelmanagerd). The x86-64 code still recognizes
   the old non-fileset prelinked kernel, the format Minkar's `plktool`
   writes and its EFI loader loads, but whether it still boots end to end
   is not known. Minkar would write either nothing (if the old format
   boots) or a fileset collection builder, and teach the loader to fill
   `KC_hdrs_vaddr`.
2. **Loading kexts at run time.** `config_kxld` is gone from
   `MASTER.x86_64` (the code is still in `libkern/kxld`). Without it, a
   kext outside the boot collection loads only from an auxiliary
   collection. Minkar would re-enable kxld, if it still builds, or write
   an auxiliary collection builder.
3. **Boot arguments.** `boot_args` version 2 gains revision 1:
   `KC_hdrs_vaddr` and root-hash fields. The EFI loader's structure is
   updated.
4. **Private headers.** Included by files an x86-64 build reaches, and not
   published: `CoreEntitlements/V2` (included by `amfi.h` unconditionally),
   `TrustCache/API.h`, `CodeSignature/Entitlements.h` and
   `image4/dlxk/interface.h`. Minkar would write clean-room headers, and
   the implementations that are called, from the call sites in the
   published tree only.
5. **Entitlements and code signing.** Darwin 25 asks an AMFI interface
   (`amfi_interface_register`, CoreEntitlements) that the closed AMFI kext
   normally provides. Minkar would provide that interface from
   MinkarCodeIntegrity (ADR 0001).
6. **MAC policies.** The policy operations table goes from version 62 to
   93: MinkarCodeIntegrity and MinkarSandbox are ported to it.
7. **corecrypto.** The kernel still links `libcc_kext` and registers a
   crypto function table that changed; corecrypto is not published, so
   Minkar's corecrypto kext is updated to the new table.
8. **Closed kexts at every version**, already replaced on Darwin 19 and
   still to be replaced: the ACPI platform, AppleSMC, AppleKeyStore,
   AppleImage4, AMFI, the sandbox kext, AppleFSCompression and APFS.

## 5. Minkar's kexts

All kexts are prelinked into one kernelcache today (`Kernel/Extensions/
Makefile`, `plktool`). Lines are `wc -l` of each kext's sources (C, C++,
headers, assembly; they include imported code). "Missing symbols" are the
kext's imports that no Darwin 25 export list has (names only, checked
against `config/*.exports` of `xnu-12377.121.6`; layouts are not
checked). "KX1" marks the minimum to reach gate M2 in QEMU.

| Kext | Lines | Depends on | Missing symbols | What changes on Darwin 25 | KX1 |
| --- | ---: | --- | --- | --- | --- |
| IOACPIFamily (with the generic platform) | 144,471 | kpi, IOPCIFamily, AppleAPIC, ApplePS2Controller | 3: `IOInterruptController::_RESERVED*` | Reserved slots of IOInterruptController now used | yes |
| AppleAPIC | 1,549 | kpi | the same 3 | As above | yes |
| IOPCIFamily | 23,592 | kpi | the same 3 | Upstream adds PCIDriverKit and VT-d | yes |
| AppleI386PCI | 1,422 | IOPCIFamily | 0 | — | yes |
| ApplePS2Controller | 3,370 | IOHIDFamily | 0 | — | yes (IOACPIFamily links it) |
| corecrypto | 11,733 | kpi | 0 | The crypto function table | yes |
| pthread | 4,466 | kpi, corecrypto | 0 | Replaced by libpthread-539's kext, with the kernel's shims | yes |
| IOStorageFamily | 22,088 | kpi | 0 | Upstream adds a DriverKit side | yes |
| MinkarAHCIPort | 1,993 | IOStorageFamily, IOPCIFamily | 0 | — | yes |
| MinkarNVMe | 1,283 | IOStorageFamily, IOPCIFamily | 0 | — | for `--disk nvme` |
| hfs | 161,988 | kpi | 2: `nspace_snapshot_event`, `resolve_nspace_item` | The namespace resolver is gone; hfs-715 is published | yes |
| IONetworkingFamily (own) | 4,834 | kpi | 0 | Legacy ifnet KPIs still exported | yes |
| MinkarE1000 | 38,517 | IONetworkingFamily, IOPCIFamily | 0 | — | yes |
| IOHIDFamily | 198,039 | kpi | 0 | Upstream adds HIDDriverKit | yes |
| MinkarCodeIntegrity | 529 | kpi | 0 | MAC table v93, the AMFI interface | yes |
| AppleFileSystemDriver | 565 | IOStorageFamily | 0 | — | open (7.10) |
| MinkarFSCompression | 510 | kpi | 0 | The decmpfs KPI reviewed | open (7.10) |
| MinkarSandbox | 574 | kpi | 0 | MAC table v93 | open (7.10) |
| msdosfs | 37,756 | kpi | 0 | Upstream adds an FSKit module | open (7.10) |
| IOGraphics (IOGOPFramebuffer) | 45,630 | IOPCIFamily | 0 | — | no |
| MinkarVirtio | 2,729 | IOStorageFamily, IONetworkingFamily, IOGOPFramebuffer, IOPCIFamily | 0 | — | no |
| MinkarDRM | 33,808 | IOGOPFramebuffer, IOPCIFamily | 0 | — | no |
| IOATAFamily | 9,776 | kpi | 0 | — | no |
| AppleIntelPIIXATA | 4,585 | IOATAFamily, IOPCIFamily | 0 | — | no |
| exfat (own) | 3,625 | kpi | 0 | — | no |
| MinkarDiskImage | 1,278 | IOStorageFamily | 0 | — | no |
| IOUSBFamily | 8,763 | kpi | 0 | — | no |
| MinkarXHCIPort | 3,675 | IOUSBFamily, IOHIDFamily, IOStorageFamily, IOPCIFamily | 0 | — | no |
| MinkarUSB | 1,566 | IOUSBFamily, IOHIDFamily | 0 | — | no |
| MinkarBGE | 4,999 | IONetworkingFamily, IOPCIFamily | 0 | — | no |
| IOAudioFamily (own) | 1,956 | kpi | 0 | — | no |
| MinkarHDA | 1,737 | IOAudioFamily, IOPCIFamily | 0 | — | no |

The symbol drift is small (8 names, 3 causes). The larger changes are in
layouts and in the loading model: the MAC and crypto tables, the pthread
shims, OSMetaClass's reserved slots, and boot and auxiliary collections
instead of kxld.

## 6. Minkar's patches to the current kernel

`git log -- Kernel/xnu` has 101 commits (2025-11-02 to 2026-11-16). Against
upstream xnu-6153.141.1, 164 files changed and 37 were added; most of the
changed lines are a regenerated `AvailabilityInternal.h`, and the kernel
code changes are a few thousand lines. The EFI loader has 10 patches in
`Image/patches`. By group, with the commits that carry them:

| Group | Commits | On Darwin 25 |
| --- | --- | --- |
| Build on Linux with the tree's toolchain | `88bd140dde`, `6f2bbe85e3`, `4200306f93`, `51cd14312f`, `0d987b5f5c`, `9db0205544`, `3f619c9dfe`, `10f5a67837` | Redone against the new makefiles, not carried line by line |
| Entry from UEFI, long mode | `9b383d1974`, `d21f786ecc`, `88ada6d898` | Carried |
| Prelinked kexts and their start | `15f5d2b8a3`, `be65f9afa3`, `f6dc603d3e`, `5e0658b615`, `476276b734`, `5540b4abf7`, `cd28566bfe`, `2d8de3d75d`, `cd96179c26`, `57c6d5734b`, `b80c77c673` | Depends on the collection format (4.1, 4.2) |
| PC platform and processors (AMD, hybrid CPUs, KVM, MADT, IO-APIC, topology) | `97ceca6300`, `e5e8bdd0b7`, `b75d445e8d`, `8298b20fdf`, `bb0db8f21f`, `5345cde8e2`, `c858199650`, `de2b1998d8`, `4a3ac92b47`, `97e03bd7f0` | Carried, after checking which are already upstream |
| Console (video and serial) | `a547964e38`, `60ae142c5a`, `5550839382`, `288c5dad4f`, `917a98c0c1`, `c0de370b52`, `78a269266b` | Carried |
| BSD and POSIX fixes | `febbeed8ad`, `8cb55a33ea`, `1c82773307`, `b717c9c263`, `854fb2e231`, `19bee3dab6`, `10f482285b`, `efa0bdf1e3`, `2b0e88aef7`, `3035b06ad9`, `788965f768`, `39dcee2291` | Each checked against Darwin 25 first |
| File systems and the root volume | `aa6e66549c`, `54e969ab06` | Carried |
| os_log | `529bae7db4`, `f3de82c1c7`, `0709544df7`, `aca373f14f` | Checked against Darwin 25's logging |
| Code signing and corecrypto | `39e9807e2c`, `e716c9e478`, `82b86ab6a4` | Replaced by 4.4 to 4.7 |
| Names and licenses | `fbe3410dae`, `da1633edf3`, `a0874f9817`, `35a7fe13ec` | Carried |

## 7. Decisions on the open questions

The numbering is that of the proposed version's questions.

1. **arm64 target hardware.** arm64 is not dropped, and the kernel version does not
   change for it, but booting it is outside this ADR and not decided: see section 8.
   ARM0 (compiling only) goes on. Writing an arm64 machine configuration, a pexpert
   platform, an interrupt controller or a pmap mode is not in scope for now, and ARM1
   does not start.
2. **x86-64 after Darwin 25.** Accepted as a consequence, not an open risk: if Darwin 26
   stops publishing x86-64, Minkar's x86-64 PCs stay on a Darwin 25 branch, the last one
   for that architecture. Continuity beyond Darwin 25 depends on arm64.
3. **The old prelinked format.** Not decided on paper: it is KX1's first experiment, to
   boot Darwin 25 with `plktool`'s current output. If it boots, that format stays; if
   not, a fileset kernel collection builder is written and the EFI loader learns to load
   it. Either way the loader moves to revision 1 of `boot_args` (`KC_hdrs_vaddr` and the
   new fields).
4. **kxld.** Minkar prelinks every kext into the kernelcache, so loading at run time is
   needed by no gate. In KX1 and KX2 every kext goes in the boot collection. Turning
   `config_kxld` back on, or building an auxiliary collection, is decided after KX2, and
   only if the need appears.
5. **Missing headers** (CoreEntitlements V2, TrustCache, `CodeSignature/Entitlements.h`,
   image4 dlxk) and the AMFI interface's provider. Clean-room stand-ins are accepted:
   written only from the call sites in the published xnu, under BSD-2-Clause, each with a
   provenance note citing the xnu files its interface was deduced from. No other source.
6. **corecrypto's license.** A priority, independent of the kernel move, because it
   concerns today's public tree. A report on its origin and every file's license is
   written (2026-11-20); corecrypto is not changed, nor moved to Darwin 25's function
   table, until the maintainer decides on it. **KX1 does not start before that.**
7. **hfs's license text.** Each file's header rules. `Kernel/_PROVENANCE` and `NOTICE`
   now say what the headers say (2026-11-20): the kext is APSL 2.0; `hfs_util` and
   `hfs_japanese` are APSL 1.2; hfs's `APPLE_LICENSE` is the APSL 1.2 text, and is not
   touched. The files under an APSL older than 2.0, in hfs and in the old families, are
   listed for the maintainer (section 10).
8. **Stale provenance.** `Libraries/_PROVENANCE` corrected (2026-11-20): dyld-750.6 and
   each component's real Darwin 19 tag.
9. **The userland moves with the kernel.** dyld, libsyscall, Libc, libplatform,
   libpthread (library and kext), libmalloc, libdispatch, objc4, Libinfo, Libnotify,
   IOKitUser, kext_tools and configd move to their Darwin 25 versions together with the
   kernel. launchd-842 and CF-Lite have no Darwin 25 version: they stay and are adapted.
   Checking launchd against Darwin 25's libxpc and boot interfaces is part of KX1.
   libunwind, libc++ and compiler-rt still come from the tree's toolchain.
10. **Gate M2's minimum.** Determined in KX1 by test boots, taking kexts out one at a
    time. Section 5's "KX1" column is the starting point.
11. **Kernel version policy.** Resolved by the frame above.
12. **Toolchain.** A compile-only check of xnu-12377.121.6's x86-64 tree with the tree's
    clang, outside the tree, is part of KX0: section 9.

When arm64 arrives: clang sets `minos 11.0` on arm64 objects by itself, so dyld and the
load checks must accept it while `ProductVersion` is 10.15.

## 8. arm64 boot target (open)

Not decided, and not part of this ADR's scope. The facts (2026-11-20, from the
published sources only):

- The published arm64 kernel builds only for the vendor's SoCs (`T6000` to `T8142`) and
  `VMAPPLE`. For any of them the link force-loads `lib<machine>.os.<config>.a`, which
  ships only in the closed Kernel Debug Kit; SPTM, TXM and Exclaves need closed libraries
  and headers. There is no generic arm64 machine and no QEMU `virt`.

The two candidates, with no recommendation:

| Candidate | What it needs | Where it runs |
| --- | --- | --- |
| VMAPPLE | Replacing `lib<machine>.os.<config>.a` (Kernel Debug Kit only), a boot loader of Minkar's own; VMAPPLE is not on the list of SoCs with SPTM, so it would use the PPL pmap | Only on a host with the vendor's arm64 processors: not on the build server, not in the CI |
| Minkar's own platform for QEMU `virt` | A new arm64 platform inside XNU (machine configuration, pexpert, interrupt controller, pmap mode) | On the build server and in the CI; depends on nothing closed |

Research to do together with KX1, not now: measure the real size of that library by
linking VMAPPLE's arm64 kernel without it and listing the undefined symbols (from the
published sources only), and find out whether the PPL pmap, or one with no monitor,
works on cores other than the vendor's.

## 9. Toolchain check (KX0)

A compile-only check of xnu-12377.121.6's x86-64 RELEASE kernel with the tree's clang
21.1.6, outside the tree (2026-11-20): Minkar's Linux build-file changes ported to the
new `SETUP/` and `makedefs/` (14 files), `setup` and `exporthdrs` run, then every object
compiled with `gmake -k`. Nothing linked or booted.

- **685 of 1085 objects compile (63 %).** With a stand-in for three builtins of the
  vendor's compiler (wrong semantics on purpose, only to see past them), **1016 (93.6 %)**.

| Component | Objects | Compile | Vendor builtins | Missing headers | Generators | Other |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| osfmk | 369 | 300 | 54 | 15 | — | 0 |
| bsd | 501 | 276 | 198 | 26 | — | 1 |
| libkern | 76 | 53 | 22 | 1 | 1 | 1 |
| iokit | 105 | 25 | 78 | 2 | 22 | 0 |
| pexpert | 10 | 9 | 1 | 0 | — | 0 |
| libsa | 1 | 0 | 1 | 0 | — | 0 |
| security | 23 | 22 | 1 | 0 | — | 0 |

(The generator failures are counted after the stand-in; in the first pass they hide behind
the builtins.)

What fails, and what a full compile needs:

1. **The vendor compiler's type builtins** (355 objects): `__builtin_xnu_type_signature`,
   `__builtin_xnu_type_summary` and `__builtin_xnu_types_compatible`, used by `kalloc.h`
   and `IOLib.h` with no fallback. No vendor-only compiler *option* broke anything on
   x86-64 (bounds safety and pointer authentication are off there).
2. **Headers absent from the published sources** (44 objects): `TrustCache/API.h` (38,
   through `osfmk/vm/pmap_cs.h`, in the VM, the loader and the file systems),
   `CoreEntitlements/V2/API.h` (5), `CodeSignature/Entitlements.h` (1). The clean-room
   stand-ins of 7.5.
3. **Generators:** the tree's MIG does not know `-mach_msg2` (without it osfmk's stubs
   compile, but in the old message format); the tree's IIG passes `.iig` files through
   untranslated (23 objects).
4. **Other** (2 objects): the SDK's `os/firehose_buffer_private.h` is the old one; Darwin
   25's libdispatch has the one xnu needs.

The report, its build-file patch and the per-object results are kept outside the
repository with the lane's other reports. Linking, CTF, the kexts' exports and the DEBUG
and DEVELOPMENT configurations were not checked.

## 10. New questions

Found while writing this version. None is answered here; each has a recommendation.

1. **GPL-3 code in the corecrypto kext.** The corecrypto report found 13 headers identical
   to Darling's corecrypto (GPL-3); one, `ccrsa.h`, is compiled into the kext that ships in
   the image, the other 12 are unused. *Recommendation:* decide it with the corecrypto
   report, before KX1 and independently of it: point the kext at xnu's own published
   `EXTERNAL_HEADERS/corecrypto/ccrsa.h` and remove the unused ones.
2. **APSL 1.1 files built today.** 45 sources under the APSL 1.1, in AppleAPIC,
   AppleI386PCI, AppleIntelPIIXATA, ApplePS2Controller, IOACPIFamily and IOPCIFamily, are
   compiled into the image now; the rule is that the APSL 1.1 does not come into Minkar.
   *Recommendation:* list them as pending replacements; in KX, take IOPCIFamily from
   Darwin 25's release if its files are APSL 2.0 there (not checked yet), and replace the old PC kexts (APIC,
   PCI host, PS/2) with Minkar's own; drop AppleIntelPIIXATA from the image if no supported
   machine needs it.
3. **APSL 1.2 in hfs_util.** `/sbin/hfs.util` is built from two APSL 1.2 files.
   *Recommendation:* replace it with the hfs_util of the hfs release KX adopts, once its
   headers are checked.
4. **The type builtins.** *Recommendation:* for KX1, a fallback in the headers (no typed
   kalloc isolation); then implement the three builtins in the tree's clang, clean room,
   from their uses in the published xnu.
5. **MIG and IIG.** *Recommendation:* take MIG from the published release that knows
   `-mach_msg2` (bootstrap_cmds), if Darwin 25's is published; for IIG, find out where the
   tree's pre-generated DriverKit sources (`10f5a67837`) came from before deciding between
   generated outputs and a clean-room generator.
6. **firehose headers.** *Recommendation:* install Darwin 25's libdispatch firehose headers
   in the SDK when libdispatch moves (7.9).

## Consequences

- KX1 starts after Minkar 19.6.0 ships, after R1, and once the maintainer has decided on
  the corecrypto report; on a branch, with the kexts marked "KX1" in section 5, and with
  7.3, 7.4 and 7.10 as its experiments.
- arm64's boot is outside this ADR (section 8); ARM0, compiling only, goes on.
- Until KX3, `devel` builds on Darwin 19.6.0, keeps its version strings, and new framework
  and desktop code must not depend on interfaces only one of the two kernels has.
- If Darwin 26 drops x86-64, x86-64 Minkar stays on a Darwin 25 branch (7.2).
