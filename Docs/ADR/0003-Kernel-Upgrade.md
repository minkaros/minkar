# ADR 0003: Moving the kernel to Darwin 25

Status: proposed, 2026-11-20. The version is decided (roadmap decision D5,
2026-11-20: Minkar moves to Darwin 25, after Minkar 19.6.0 ships); this
ADR says how. It is milestone KX0 of lane KX in `Docs/ROADMAP.md`, and it
changes nothing in the kernel by itself. Spanish copy:
[0003-Kernel-Upgrade.es.md](0003-Kernel-Upgrade.es.md).

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
of Minkar's own, written from the published tree: see 7.1.

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

## 7. Risks and open questions

For the maintainer. None of them is decided here.

1. **arm64 target.** The published arm64 kernel builds only for the
   vendor's SoCs and `VMAPPLE`, with a closed support library. Is writing
   Minkar's own arm64 machine configuration, platform expert, interrupt
   controller and pmap mode (without SPTM or PPL) in scope for ARM1, and
   on which machine: QEMU `virt` first? Whether PPL, or a pmap with no
   monitor, runs on cores other than the vendor's is not known.
2. **x86-64 after Darwin 25.** The vendor said publicly in June 2025 that
   its 26.x release is the last for its Intel machines. If Darwin 26 drops
   `osfmk/x86_64`, x86-64 Minkar stays on Darwin 25. Accepted?
3. **The old prelinked format on Darwin 25.** Does `plktool`'s output still
   boot, or is a fileset collection builder needed? A test boot (KX1)
   answers it.
4. **kxld.** Re-enable `config_kxld` on x86-64, or move to auxiliary
   collections?
5. **Clean-room headers.** Are stand-ins for CoreEntitlements V2,
   TrustCache, CodeSignature entitlements and image4 dlxk, written from
   the call sites of the published tree, acceptable?
6. **corecrypto's license.** Minkar's corecrypto (from PureDarwin) says
   "All rights reserved" with no license grant, and `Kernel/_PROVENANCE`
   lists none. Its origin and license should be confirmed before it is
   changed for the new table.
7. **hfs's license text.** Its `APPLE_LICENSE` file is the APSL 1.2 text,
   while `Kernel/_PROVENANCE` says APSL-2.0.
8. **Stale provenance.** `Libraries/_PROVENANCE` says dyld-1066.8, but the
   tree is dyld-750.6 (`47603124d4`), and it names a branch that the
   Libsystem components do not come from.
9. **The userland with the kernel.** Do dyld, libsyscall, Libc, libpthread,
   libplatform and libmalloc move to Darwin 25 together with the kernel
   (system call numbers, commpage, pthread shims, workqueues)? launchd-842
   and CF-1153 have no Darwin 25 versions; how launchd's bootstrap and
   XPC fit Darwin 25's libxpc was not checked.
10. **Gate M2's minimum.** Are AppleFileSystemDriver, MinkarFSCompression,
    MinkarSandbox and msdosfs needed for M2? Today all kexts are prelinked
    together; a test boot without them answers it.
11. **Version strings.** `uname -r`, `kern.osrelease`, `DARWIN_VERSION`
    and `TARGET_TRIPLE` (`x86_64-apple-darwin19`) change with the kernel at
    KX3, and with them every rule that assumed Darwin 19.6.0. Which value
    do they take on the KX branch before KX3?
12. **Toolchain.** The Darwin 25 build probes for flags of the vendor's
    compiler (`-fbounds-safety`, disabled on x86-64); whether the tree's
    LLVM 21.1.6 builds the whole kernel without others was not checked.

## Consequences

- KX1 starts after Minkar 19.6.0, on a branch, with the kexts marked
  "KX1" above and answers to 7.3, 7.4, 7.5, 7.9 and 7.10.
- ARM1 is not only a port of what is published: it needs Minkar's own
  arm64 platform (7.1).
- Until KX3, `devel` builds on Darwin 19.6.0, and new framework and
  desktop code must not depend on interfaces only one of the two kernels
  has.
