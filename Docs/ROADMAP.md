# Minkar — Architectural Roadmap

2026-10-31

## Summary

Minkar's goal is a system on Darwin 19.6.0 that builds and runs Cocoa apps. The first release, **Minkar 19.6.0**, delivers API 19 with the Gienah desktop and its own visual language; Minkar 20.0 and 21.0 complete API 20 and API 21 without changing the kernel. The critical path goes strictly bottom-up: Darwin boot → launchd → userland → frameworks → WindowServer → Gienah → apps. Full Objective-C and Swift, DMG, binary compatibility and arm64 run in parallel or are deferred so they do not block that path.

The plan has 8 phases (0–7), each with deliverables and an exit gate that CI can check. No phase is closed while the one below it fails its gate. Since 2026-10-29 there is also a performance requirement: equal to or better than the reference system on the same physical machine, required from Minkar 21.0 on (see Performance requirements).

**Source of truth:** this file. [ROADMAP.es.md](ROADMAP.es.md) is the same plan in Spanish, and [Progress.md](Progress.md) shows its state as diagrams (critical path, Phase 3, the graphics stack). Verified against `devel` at `fb6e25b877` (2026-10-31), 95 commits ahead of `main`.

### Principles

1. **Verifiable gates, bottom-up.** Each phase ends with an automated test that boots the image in QEMU. No green test, no closed phase.
2. **The contract is the public Cocoa API, by API number.** API 19 first, then API 20 and API 21. Every symbol is classified as implemented, stub or missing, and measured.
3. **Noisy stubs.** Every stub logs its first call to the system log, so real apps tell us what is missing, in the order they use it.
4. **Reuse before writing.** Open source APSL code first (xnu, dyld, launchd, hfs), then swift-corelibs, then FreeBSD's userland. New code only for the gaps.
5. **Clean licenses.** New code under MIT or BSD-2, nothing GPL in base components (DMG, for one), and a clean-room rule: no one who has seen the reference platform's closed source contributes code. Third-party trademarks are not used in names, icons or public text; a single TRADEMARKS.md declares them.
6. **Reference apps as acceptance tests.** A fixed set of open source apps written for the Cocoa API is built at each milestone; the symbols they import set the order of the framework work.

## Version decision

Minkar separates three layers that go together on the reference platform. The kernel stays at Darwin 19.6.0; the API and the look move on their own.

| Layer | Decision | Note |
| --- | --- | --- |
| Kernel and BSD base | Darwin 19.6.0 | Touched only when a new API needs a kernel interface |
| API | API 19 first; then API 20 and API 21 | Each API driven by the reference apps' imports |
| Look | The Gienah visual language from Phase 5 | Our own design and guide; no third-party icons, fonts or artwork |

**API numbers (decided 2026-10-30).** "API N" names the public Cocoa API of Darwin N; it is not the kernel, which stays Darwin 19.6.0. Code, plists and availability macros use the version number in the table; prose says "API N".

| API | Version in code (`API_AVAILABLE`, `ProductVersion`) | Year |
| --- | --- | --- |
| API 19 | 10.15 | 2019 |
| API 20 | 11.0 | 2020 |
| API 21 | 12.0 | 2021 |
| API 22–26 | 13.0 to 27.0 | 2022–2026, out of scope |

**Minkar versions (decided 2026-10-30).** Minkar's major version is the API it delivers; the kernel does not change with it.

| Minkar | Delivers | Kernel (`uname -r`) | `ProductVersion` |
| --- | --- | --- | --- |
| 19.6.0 | API 19, the Gienah desktop and DMG; functional gates M0–M6 on physical hardware | 19.6.0 | 10.15 |
| 19.6.x | Fixes; may carry parts of API 20 | 19.6.0 | 10.15 |
| 20.0 | API 20 complete and tested | 19.6.0 | 11.0 |
| 21.0 | API 21 complete and tested, plus the performance gates | 19.6.0 | 12.0 |

- A gate decides a major version, not a date. If the kernel is ever upgraded, the version follows it.
- Equivalences: the former "1.0" is Minkar 19.6.0, and "2.0" becomes Minkar 21.0's performance requirement. The 0.x numbering (`PROD_VERSION = 0.7.1`) became `19.6.0` on 2026-10-31 (`d2b559759f`), with `ProductFamily` Pre Alpha → Alpha → Beta.

* **APIs that depend on the kernel.** Some new APIs expect interfaces XNU 19.x does not have; modern libpthread and libdispatch, for instance, use the workqueue and kevent interfaces of a newer XNU. Each case is solved with a narrow kernel patch or a userland implementation, and the decision is documented.
* **Reported version (decided 2026-10-25; extended 2026-10-30; done 2026-10-31).** `ProductVersion` in `SystemVersion.plist` reports the compatibility value of the last closed API (10.15, 11.0, 12.0), never 19.6.0; `ProductUserVisibleVersion` shows the Minkar release (19.6.0); `uname -r` still says 19.6.0. `@available`, `sw_vers -productVersion`, `NSProcessInfo` and LaunchServices read `ProductVersion`, and libSystem (`_availability_version_check`) takes the value from there.
  - It starts at 10.15 (API 19) and goes to 11.0 and 12.0 only when API 20 and API 21 close.
  - Each API's gate: a script checks that no symbol on its list is a stub, using Phase 0's symbol inventory and the noisy-stub log.
  - Compatibility: a `SystemVersionCompat.plist`-style mechanism and `SYSTEM_VERSION_COMPAT=1`. Binaries linked against an older SDK get the older version.
  - Rejected: raising `kern.osrelease` to 21.x, because it would lie about the kernel ABI; reporting 10.15 forever, because it blocks apps that require a higher API in `LSMinimumSystemVersion`; and putting 19.6.0 in `ProductVersion`, because `@available` would take missing APIs as present.
* **Software that derives the platform version from Darwin** (19 → API 19) goes into Phase 7's compatibility matrix. Example: a native clang with the `darwin19.6.0` triple assumes API 19's deployment target (10.15); `MACOSX_DEPLOYMENT_TARGET` corrects it.

## Performance requirements and reference hardware

Minkar's performance must be equal to or better than the reference system's on the same physical machine (decided 2026-10-29). A solution that works but is slow does not close a requirement: it stays partial. Minkar is meant for physical machines; virtual machines are only a development environment.

**Decided (2026-10-30):** Minkar 19.6.0 closes the functional gates M0–M6; Minkar 21.0 also requires the performance gates in this table. The GPU track starts now, in parallel, because it is the longest piece and the requirement depends on it.

| Metric | Requirement, against the reference system on the same machine |
| --- | --- |
| Boot to the login window | ≤ reference |
| Input-to-photon latency | ≤ reference |
| Compositing several animated windows | The display's refresh rate without dropped frames; WindowServer CPU ≤ reference |
| App launch, cold and warm | ≤ reference |
| Idle memory | ≤ reference |
| 2D drawing and text (Core Graphics, Core Text) | Drawing benchmark ≥ reference |
| 3D (Metal, Vulkan, OpenGL) | ≥ reference with the same GPU |

| Environment | Use | Gates |
| --- | --- | --- |
| QEMU VM (TCG and KVM) | Development and CI | Functional; software paths (pixman, lavapipe, llvmpipe) allowed as scaffolding |
| PC 1: Gigabyte H270N-WIFI, i7-7700 | First physical machine; boots to login from SATA or NVMe | Functional and performance, against the reference system |
| PC 2 | To be chosen; to measure the reference, the vendor's original hardware (candidate: Mac Pro 6,1) | Same |

- **How it is measured:** Minkar and the reference system on the same machine (dual boot), with the same benchmarks run over SSH. The reference system is measured only on its vendor's original hardware (candidate: Mac Pro 6,1), as its license allows no other, and its values are kept out of the public repository.
- **Consequence:** if the reference is measured only on the Mac Pro 6,1, Minkar must be measured there too, and its GPU (FirePro D300, GCN 1.0, the oldest and least maturely supported) becomes the first high-performance driver's.
- **Open decision — the first GPU driver's family:** PC 1 uses Intel HD 630 (Gen9) integrated graphics unless it has a discrete card; another available machine has an AMD RX 6600 (RDNA2), useful as a Minkar test machine without publishing reference measurements on it. The choice is between starting with PC 1's GPU (faster development) and the Mac Pro 6,1's (where it can be compared).

## Baseline

Minkar boots on Darwin with its own EFI loader, the whole system builds as Mach-O, and it already runs on a physical PC with its eight CPU threads and from its NVMe SSD. Phase 2 is closed; the biggest gap is between the kernel and the desktop. State on 2026-10-31:

| Component | State | What is missing | Phase |
| --- | --- | --- | --- |
| EFI loader + Darwin kernel | Boots in QEMU (TCG and KVM) and on PC 1, from SATA or NVMe; SMP with every CPU; the power button shuts down in order | PC 2, AMD processors, MSI-X | 1 |
| System build | Full `world`; CI builds and boots it on `main` and by hand | — (M0 met) | 0 |
| libSystem | The reference apps find all their libSystem imports (308 of 308); `os_log` writes to syslogd; `@available` compares against `ProductVersion`; dyld's @rpath, @loader\_path and re-exports tested | Noisy stubs, `logd`, dyld's shared cache | 3 |
| launchd / launchctl | launchd-842 as PID 1; launchctl with kickstart; `KeepAlive PathState`, `WatchPaths` and `QueueDirectories` | Nothing | 2 (closed) |
| Userland and system tools | Every Phase 2 command (337 of the reference list's 354) | `open` and the commands of later phases | 2 (closed) |
| DiskArbitration | The original framework and `diskarbitrationd` (APSL); our own `diskutil` and `hdiutil`; mounts HFS+, FAT and exFAT | — | 2 (closed) |
| CoreFoundation | CF-Lite (APSL) | The APIs missing up to API 21; `CFArray`/`CFDictionary` bridging | 3 |
| Foundation | Three batches of fixes (2026-10-26); `NSProcessInfo` with `operatingSystemVersion` (2026-10-31) | `NSURLSession`; the API 19 audit | 3 |
| Swift 6 | Nothing in the tree | Compiler, stdlib, overlays | 3 |
| WindowServer | Builds on XNU with an IOFramebuffer backend; never run | Run it; input from IOHIDSystem | 4 |
| GPU acceleration | None; without it the display cannot be turned off either | The whole track (performance requirement) | 4 and Minkar 21.0 |
| SystemUIServer, Dock | Partial, not built | Almost everything | 5 |
| Kraz, System Preferences | Do not exist | New Cocoa apps | 5 |
| AppKit | Partial (Cocotron) | The API's recent pieces and the Gienah visual language | 5 |
| DMG, base apps | `hdiutil attach` already mounts through DiskArbitration | `DiskImages.framework` (UDIF), apps | 6 |
| Binary compatibility, arm64 | Future; install names already the same as the originals | The rest | 7 |

## Phase 0 — Build, toolchain and CI

Goal: any contributor gets a bootable image from scratch with one command, and every PR is tested by booting it in QEMU. Without this, every other phase works blind.

**Work**

- [x] Pin the toolchain: `tools/toolchain/MANIFEST` pins each component's upstream version, git tree and version line (clang 17.0.6 from `swift-6.0-RELEASE`, ld64-609, and since 2026-10-29 cctools with `ctf_insert` and dtrace's CTF tools); `manifest.sh check` verifies it in CI and checks that one entry builds, links and archives to the same bytes twice.
- [x] bmake targets per layer: `bmake <layer>` and `bmake <layer>-check`, `layers-check` and `image-check`. The third-party frameworks moved to Phase 3.
- [x] CI: `.github/workflows/build-boottest.yml` builds the toolchain (cached), the system and the image, and passes `boottest GATE=M2`. Since 2026-10-29 it runs on `main` and by hand, so as not to run out of a free account's Actions minutes.
- [x] Symbol inventory: `tools/symbols/`; Minkar covers 68.3% of the four reference apps' system imports (2026-10-31).
- [x] Document the build: `BUILDING.md` with host, packages, measured times and 17 frequent errors.

* [x] **Exit gate (M0):** met in CI (`05fa75cb65`, and again in `9191fe8458`, 2026-10-29).

## Phase 1 — Darwin boot

Goal: XNU boots on a generic x86-64 PC up to PID 1 with the root disk mounted. It is official priority number two and blocks everything else.

**Work**

- [x] EFI loader: prelinked kernel, GOP framebuffer, memory map and ACPI tables; since 2026-10-30 also the configuration tables (SMBIOS: the machine's model and UUID).
- [x] XNU platform for generic PCs: ACPI, APIC, TSC/HPET, no SMC.
- [x] Minimal kexts:
  - [x] AHCI with MSI (`MinkarAHCIPort`).
  - [x] virtio-blk (`MinkarVirtio`).
  - [x] NVMe (`MinkarNVMe`, written from the specification): PC 1 boots from its SSD (about 1.4 GB/s reading and 1.1 GB/s writing). *Pending:* in QEMU, reads through the block device run at 1 MB/s.
  - [x] USB xHCI + HID (`MinkarXHCIPort`, `IOUSBFamily`, `MinkarUSB`), in QEMU and on PC 1.
  - [x] e1000 and e1000e (`MinkarE1000`), with Intel's shared code for the I219-V and I211.
  - [x] virtio-net (`MinkarVirtio`).
  - [x] GOP framebuffer as the console, at the native resolution from EDID (3440×1440 on PC 1).
- [x] HFS+ root (APFS and ZFS later); FAT (`msdosfs`); exFAT with our own driver, reading and writing, `mount_exfat`, `fsck_exfat` and `newfs_exfat` (2026-10-31).
- [x] Serial console for the gates.
- [x] Readable panic logs: `tools/panic-report.py` symbolicates each frame; tested on a real panic.
- [x] Boot in QEMU with KVM and TCG. *Partial:* CI runners still use TCG only.
- [x] SMP (2026-10-30): every CPU the MADT enables runs. On PC 1, eight threads (`hw.ncpu` 8, 4 physical); in QEMU tested with 2 to 64 processors and several topologies.
- [x] Power button (2026-10-31): IOACPIFamily takes ACPI's fixed event or the PNP0C0C device and `powerd` asks for an orderly shutdown; on PC 1 the machine powers off. `shutdown` and `reboot` take about 2 s (20 s before, because of diskarbitrationd), and the test images boot with `msgbuf=1048576`.
- [ ] Reference hardware list ([Hardware.md](Hardware.md)). *In progress:* PC 1 documented; PC 2 not chosen.
- [ ] More than 64 processors and x2APIC (after Minkar 19.6.0); a larger machine runs on its first 64 today.
- [ ] AMD processors: the kernel recognizes them, but Minkar has never run on one. First in QEMU with `-cpu EPYC`, then on an AMD PC.
- [ ] Boot without `pci_keep_console=1`; IOKit takes more than 30 s to go quiet.
- [ ] IPv6 on PC 1's network: RDNSS and DNS answers over IPv6, which QEMU's user network does not give (moved from Phase 2 on 2026-10-31).
- [ ] PC 1's Wi-Fi (Intel, M.2): no driver.

* [ ] **Exit gate (M1):** met in QEMU and on PC 1 (from SATA and from NVMe, with SMP). A second PC is missing.

## Phase 2 — Base userland

Goal: a complete multi-user Unix system on Darwin, managed by launchd, with the services Gienah and the apps will need later.

**libSystem and dyld**

- [x] Real dyld with `@rpath`, `@loader_path` and re-exports: it is the original dyld (APSL); a test builds a program, its libraries and a re-exporting one, and runs them (2026-10-31). The rest of libSystem, noisy stubs and the shared cache moved to Phase 3.

**launchd**

- [x] Port to Darwin the old XPC-based version used in the FreeBSD era.
- [x] `launchctl` with `load`, `bootstrap`, `list` and `kickstart` (`kickstart [-k] [-p]` since 2026-10-31, tested by restarting notifyd).
- [x] XML plists as well as JSON, to use `LaunchDaemons` and `LaunchAgents` as apps write them (sshd starts from `com.openssh.sshd.plist`).

**BSD userland, in batches**

- [x] Boot: `sh`, `ls`, `cp`, `mv`, `rm`, `mount`, `ifconfig`, `ps`, `kill`.
- [x] Generic Unix commands from FreeBSD `main` with our own compatibility layer: every BSD command on the reference list (2026-10-27). `grep` lacks `-Z` and `-J`.
- [x] Tools missing from the image: 337 of the reference list's 354 commands ([Commands.md](Commands.md)), among them `dtrace` with kernel types (CTF), `system_profiler`, `pmset` with `powerd`, `networksetup`, `kextload`/`kextstat`, `emond`, `fs_usage`, `leaks`, the NFS server with `rpcbind` and, since 2026-10-31, our own exFAT driver (reading and writing), `mount_exfat` and the `exfat.fs` bundle: diskarbitrationd mounts exFAT disks read-write. Every command of the phase is there.
- [x] Moved to later phases (2026-10-30): `pbcopy`, `pbpaste` and `screencapture` to Phase 4; `open`, `lsappinfo`, `say` and `mdfind`/`mdls`/`mdutil` to Phase 5; `osascript`, `opendiff` and `tmutil` to Phase 6.
- [x] Out until after Minkar 19.6.0 (decided 2026-10-30): APFS (no open implementation), SMB (Samba is GPL) and `fdesctl`.
- [x] Power user: `zsh`, `tar`, `ssh`, `curl`, `make` and `git` 2.56.0. git's Perl, Python and Tcl commands moved to Phase 6.
- [x] System tools: `plutil`, `defaults`, `sw_vers`, `ditto`, `hdiutil` and `diskutil`; `chmod` with the reference's ACL syntax, and the kernel applies allow entries as well as deny ones (2026-10-28).

**Support daemons** (needed by Phases 5 and 6)

- [x] `notifyd`, with the original `notifyutil` (APSL) since 2026-10-31: posts, waiting clients and states are tested, also after a restart.
- [x] DiskArbitration (2026-10-30): the original framework and `diskarbitrationd` (APSL) with the `hfs.fs`, `msdos.fs` and `exfat.fs` bundles, and our own `diskutil` and `autodiskmount`; `hdiutil attach` mounts through it. Without Security, the daemon decides permissions itself.
- [x] Our own `powerd` and `pmset`: settings, assertions, events and the power button; sleep is refused, as there is no ACPI S3 yet.
- [x] Moved on 2026-10-31: `logd` goes to Phase 3 with `os_log` (`syslogd` and `aslmanager` meanwhile); Directory Services–style user management goes to Phase 5, with the login window that needs it.

* [x] **Exit gate (M2):** boot with launchd as PID 1, network by DHCP, console login and SSH access.

**State (2026-10-31): Phase 2 closed.** M2 met: `boottest GATE=M2` sees its 13 markers in about 75 s under QEMU without KVM, including configd's DNS configuration (`scutil --dns`) and resolution through mDNSResponder (`dscacheutil -q host`). Every command of the phase is in the image, and what is not a command went to the phase that needs it.

- [x] launchd-842 starts as PID 1.
- [x] `dhcpclient` gets 10.0.2.15 for `en0`.
- [x] root logs in on the console through PAM.
- [x] root logs in over SSH from the host with a key.

Components:

- [x] **Our own IONetworkingFamily** (BSD-2), with the IOKit API.
- [x] **The `MinkarE1000` driver** for Intel e1000 and e1000e NICs.
- [x] **Our own `ifconfig`, `dhcpclient` and `ipconfig`.**
- [x] **OpenSSH 10.6p1 without OpenSSL**, with PAM.

Closed later:

- [x] MSI for disk and network.
- [x] IPv6: link-local and SLAAC addresses, `ping6` and DNS queries over IPv6, tested in QEMU since 2026-10-31. RDNSS and DNS answers over IPv6 are checked on PC 1 (Phase 1), as QEMU's user network does not give them.
- [x] DNS over IPv4 with `libresolv` and mDNSResponder (gate M2); over IPv6, as above.
- [x] `getaddrinfo`, `id` and `dscacheutil`.
- [x] Gate M2 also checks name resolution.
- [x] Fixed the AHCI driver's race with MSI that made launchd fail.

What was left of Phase 2 without being a command moved on 2026-10-31: the rest of libSystem, noisy stubs, dyld's shared cache and `logd` to Phase 3; Directory Services to Phase 5; git's Perl, Python and Tcl commands to Phase 6.

## Phase 3 — Base frameworks and Swift

Goal: a command-line app in Objective-C or Swift, written for API 19, builds and runs unchanged; then it moves up to API 20 and API 21. Phase 2 is closed, so this phase waits for nothing.

**Architecture decisions (taken first)**

- [x] **Objective-C runtime: objc4 (APSL), decided.** Already in the tree: `Libraries/Makefile` builds `objc4` before `Libsystem`, and `Minkar.sdk` takes its headers. It is the original ABI throughout the system, which Phase 7 needs. Only `__objc_classlist` and `__objc_imageinfo` in a built binary are left to confirm.
- [x] **CoreFoundation: CF-Lite (APSL), decided 2026-10-25.** It is the current CF in `Frameworks/CoreFoundation`, with its own toll-free bridging (`CFBridging.c`, `CFRuntime.c`) and CF ↔ XPC bridge. It is not replaced by Swift's CF. The public CF-Lite predates API 19: the CF APIs missing up to API 21 are added by hand, with swift-corelibs CF (Apache 2.0) as reference. `Frameworks/CoreFoundation.old` was removed on 2026-10-26.
- [x] **Foundation for Swift: Objective-C Foundation plus the `libswiftFoundation` overlay, decided 2026-10-25.** It is API 21's model. Base: the last open version of the Darwin overlay (Apache 2.0) in `swiftlang/swift`'s history, updated to Swift 6.0. swift-foundation is only a reference for new APIs.
- [x] **Swift runtime: Swift 6.0 in `/usr/lib/swift`, decided 2026-10-25.** The same version as the tree's LLVM (swiftlang's fork, branch `swift-6.0-release`). Starting point: no Swift compiler, stdlib or overlays in the tree; only the Dispatch overlay (`libdispatch/src/swift`) and the Swift annotations in the CF headers.
- [x] **Combine and SwiftUI, decided 2026-10-25.** Both belong to API 19. Combine enters API 19 on OpenCombine (MIT), renamed as the `Combine` module and installed as `Combine.framework`; it depends on the Swift toolchain gate. API 19, 20 and 21 close explicitly without SwiftUI. OpenSwiftUI (MIT, early development) is watched, and the decision is revisited by measuring how many reference apps need it.

Decision order: CoreFoundation constrains Foundation for Swift, which constrains the Swift runtime.

**Work**

0. Bring in the remaining third-party frameworks (moved from Phase 0): 13 frameworks build and install; CFNetwork is commented out, and Security and OpenDirectory are not built.
1. Complete CF-Lite up to API 21, with bridging tests on every change. Pending: `CFArray` and `CFDictionary` are not bridged to `NSArray` and `NSDictionary` yet.
2. Audit Foundation against API 19, starting with what the reference apps import. *Done (2026-10-25/26):* `NSException`, `NSUUID`, `NSSecureCoding`, `NSRegularExpression`, `NSJSONSerialization` (with JSON5), `NSCache`, `NSInvocation` with the System V calling convention, weak `NSHashTable` and `NSMapTable`, binary property lists byte for byte as the reference writes them, `NSProgress` and the modern `NSString` API, plus dozens of bugs found while testing. *Done (2026-10-31):* `NSProcessInfo` with `operatingSystemVersion` (10.15.0), `isOperatingSystemAtLeastVersion:`, `operatingSystemVersionString`, processors, memory and `systemUptime` (`fb6e25b877`). *Left from the symbol report:* `NSURLSession`. *Known bug:* `NSKeyedUnarchiver` cannot read the XML property list `NSKeyedArchiver` writes. Untested: locale-dependent behaviour (UTF-8 locale data is in the image since 2026-10-27).
3. Security (APSL) for certificates and a basic keychain, needed for TLS in `NSURLSession`.
4. LaunchServices: bundle database and UTI types; `open` and Kraz depend on it.
5. CoreText and CoreData: a minimal implementation driven by real use (see the graphics stack's technical guide).
6. Swift 6: full stdlib and runtime, with concurrency in the system, and module maps for the frameworks.
7. API 19 → 20 → 21, raising `ProductVersion` (10.15 → 11.0 → 12.0) as each one closes; each close is a Minkar release (19.6.0, 20.0, 21.0).

From Phase 2 (2026-10-31):

- [ ] Complete libc, libm, pthread, libdispatch, libxpc, notify and os\_log. *Partial:* since 2026-10-31 the reference apps find all their libSystem imports (308 of 308): `os_log` writes its messages to syslogd, with `%{public}`/`%{private}` and `%@`, and to stderr in DT mode (it wrote nothing before, and the SDK's `<os/log.h>` had no `os_log()`); `syslog(3)` reaches syslogd again; `os_log_error`/`os_log_fault` under the reference's names; `os_activity`; `_availability_version_check` for `@available`; `launch_activate_socket` (`0709544df7`, `c0923c1bc7`, `5051c781b4`). `math.h` matches the reference since 2026-10-26; `vis(3)` lacks the newer flags.
- [ ] Noisy stubs (log on the first call). *Partial (2026-10-31):* `<minkar/stub.h>` logs a stub's first call in a process to syslogd (`org.minkar.stubs`), with the image that called it, and to stderr in DT mode; the 875 unimplemented methods and functions of Foundation, AppKit, CoreData and QuartzCore use it (they printed on stderr at every call), and libxpc's 12 silent stubs say so now. `tools/symbols/trivial-functions.py` lists the exported functions that do nothing (404 on 2026-10-31, many of them third-party and empty by design): the rest are reviewed from that list.
- [ ] dyld's shared cache (optional at first).
- [ ] `logd`, with `os_log` (`syslogd` and `aslmanager` meanwhile: `os_log` messages go to syslogd until logd keeps them).

**Swift toolchain** (the prototype gates the rest of the Swift work)

- [ ] Prototype 1: build `swiftc` (`swiftlang/swift`, branch `release/6.0`) on the Ubuntu host as a cross compiler, against the tree's `swift-6.0-release` LLVM.
- [ ] Prototype 2: build `libswiftCore` for `x86_64-apple-macosx10.15` against `Minkar.sdk` and run a (non-Embedded) Swift "hello world" in QEMU. If it fails, reconsider building the stdlib on an x86-64 host of the reference platform.
- [ ] `libswift_Concurrency` and the system overlays: `Darwin`, `ObjectiveC`, `CoreFoundation` and `Dispatch` (from `libdispatch/src/swift`).
- [ ] The `libswiftFoundation` overlay from its last open version, updated to Swift 6.0.
- [ ] Install everything in `/usr/lib/swift` in the image and add a `boottest` gate that runs a Swift binary.

**Exit gate (M3):** a fixed set of open source command-line tools written for the Cocoa API (ObjC and Swift) builds and passes its tests on Minkar.

## Phase 4 — Graphics and WindowServer

Goal: WindowServer draws AppKit windows and receives keyboard and mouse. In the VM the software path is enough; on physical machines the goal is GPU compositing, as the performance requirement demands it.

**Proposed architecture**

- A display backend interface inside WindowServer: `GOP` (software) → `virtio-gpu` (VMs) → accelerated. AppKit and Quartz never see the backend.
- Compositing with Onyx2D and QuartzCore, as today; the port changes the output, not the drawing model.
- Events: kernel HID → WindowServer → each app's event queue, with the same `NSEvent` model.
- App ↔ WindowServer communication over Mach/XPC, not ad hoc sockets.

**Backend decision (2026-10-25)**

- **First output:** `IOGOPFramebuffer.kext` with `IOFramebufferDisplay.m` and software compositing. Enough for Minkar 19.6.0 and the VM; on physical machines only the starting point.
- **Compositor (changed 2026-10-29):** designed for the GPU from the start, with Vulkan as the main backend behind our own interface. pixman stays for bring-up and for VMs without a GPU.
- **VMs:** a virtio-gpu 2D driver as an `IOFramebuffer` subclass; virtio-gpu's 3D modes (virgl, Venus) are optional, to test accelerated paths.
- **Input:** IOHIDFamily, with `ApplePS2Controller` in QEMU and xHCI + HID (`MinkarUSB`) on real hardware. libinput, libevdev and mtdev are no longer built.
- **Keyboard:** libxkbcommon for now; in the long term `UCKeyTranslate` and Text Input Sources (Phase 5).
- **GPU (changed 2026-10-29):** no longer deferred. It is a parallel track from now: a kernel driver for one family, Mesa Vulkan/OpenGL, a GPU compositor and, in the long term, native Metal on the driver. The first family is an open decision (see Performance requirements).

**Work**

- [ ] Output backend on IOFramebuffer. *Partial:* the kext loads and WindowServer builds with the backend, but it has never run.
- [ ] WindowServer starts and draws in QEMU. Its job (`org.minkar.WindowServer.plist`) is in the tree but not installed.
- [ ] IOHIDSystem event delivery to user space: the dispatch loop is under an undefined `NEW_HID` and the event queues are never created.
- [ ] An IOHIDSystem input reader in WindowServer (`kIOHIDServerConnectType`, `EvGlobals`, Mach port).
  - [ ] PS/2 keyboard. *Partial:* its keys reach the console, not WindowServer.
  - [ ] An `ApplePS2Mouse` driver on `ApplePS2MouseDevice`; `IOHIPointing` builds since `90c96e8862`.
  - [x] `IOKit/hidsystem` headers in the SDK.
  - [x] xHCI + HID (`MinkarUSB`), tested in QEMU and on PC 1.
- [ ] Remove `libinput`, `libevdev` and `mtdev` from the tree.
- [ ] virtio-gpu 2D as an `IOFramebuffer` subclass.
- [ ] `pbcopy` and `pbpaste` on the pasteboard server, and `screencapture` on WindowServer's capture (moved from Phase 2).
- [ ] Multiple displays and resolution changes, after M4.
- [ ] GPU ADR: the family, licenses (only drm-kmod's MIT/BSD parts), a LinuxKPI-style layer for XNU and a KMS prototype on the reference machine.
- [ ] Compositing performance gate on the reference PC (a Minkar 21.0 requirement).

**Exit gate (M4):** in QEMU and on a reference PC, several AppKit apps open windows, are dragged, take keyboard input and redraw without artifacts.

## Graphics stack: components and sequence

Of the graphical environment, the drawing base exists (Core Graphics with FreeType, and pixman, libpng, libjpeg and libtiff in `BSD/lib`), as do the GOP console and USB input. The compositing server, IOSurface and everything that depends on the GPU are missing. State verified on `devel` (`fb6e25b877`, 2026-10-31): the graphics stack has not changed since 2026-10-29.

Components, by state:

- **There:** Core Graphics with FreeType; pixman, libpng, libjpeg and libtiff (`BSD/lib`); the GOP console (`IOGOPFramebuffer`); USB input (xHCI + HID, `MinkarUSB`); the `IOKit/hidsystem` headers.
- **Partial:** WindowServer (builds, never run); the PS/2 keyboard (reaches the console, not WindowServer); QuartzCore and Onyx2D; AppKit (Cocotron).
- **Missing:** IOHIDSystem event delivery; `ApplePS2Mouse`; virtio-gpu 2D; IOSurface; the Core Animation compositor; Core Text on HarfBuzz/FreeType; giflib and libwebp; ColorSync on Little CMS; and the GPU track (kernel driver, Mesa, Metal).

Each piece's technical detail is in the graphics stack's technical guide; [Progress.md](Progress.md) draws them by state.

Development sequence:

1. IOHIDSystem event delivery and the PS/2 mouse.
2. WindowServer on the GOP framebuffer.
3. virtio-gpu 2D (ported from FreeBSD, BSD-2).
4. IOSurface (Mach shared memory).
5. A Core Animation compositor designed for the GPU (Vulkan backend; pixman only for bring-up and VMs).
6. HarfBuzz/FreeType for Core Text, giflib/libwebp, Little CMS for ColorSync.
7. A modern AppKit with layer-backed views.

In parallel, the GPU track G1–G5: from the first driver's family and the KMS prototype to Mesa (lavapipe/llvmpipe in VMs) and Metal (darling-metal and Indium, MPL-2.0).

Step 1 is next: without input, WindowServer cannot close M4. G1 starts as soon as the GPU family is decided, and the GPU track reuses steps 4 and 5.

## Phase 5 — Gienah desktop

Goal: a complete graphical session in the Gienah visual language, with login, a global menu bar, Dock, desktop icons and preferences. The order follows what the user sees at boot.

1. **LoginWindow:** authentication against this phase's user service (Directory Services, below) and session start (the user's `LaunchAgents`).
2. **SystemUIServer and the menu bar:** the active app's menu through WindowServer, plus a public Menu Extras and `NSStatusItem` API (official priority).
3. **Basic Menu Extras:** volume, displays, network, clock, notifications. Each one a loadable bundle, not built into SystemUIServer.
4. **Dock:** launcher, open apps, minimizing and wallpaper, on LaunchServices.
5. **Kraz:** a new pure Cocoa app. Browsing with LaunchServices, mounting through DiskArbitration, desktop icons, Trash and "Open With".
6. **System Preferences:** host app + `.prefPane` bundle loader (`NSPreferencePane`). First panes: Displays, Keyboard, Users, Network, Date & Time.
7. **AppKit:** complete the missing controls and give the whole system the Gienah visual language (our own design, no third-party artwork), prioritized by what Kraz and System Preferences use.

Across all of it: these are ordinary Cocoa apps or bundles, so they double as a continuous AppKit test.

Commands that come with this phase (moved from Phase 2):

- [ ] `open` and `lsappinfo`, on LaunchServices.
- [ ] `mdfind`, `mdls` and `mdutil`, with a metadata index for Kraz's search.
- [ ] Directory Services–style user management, for LoginWindow and the Users pane (moved from Phase 2 on 2026-10-31).
- [ ] `say`, with an audio driver and speech synthesis.

**Exit gate (M5):** from boot, a graphical session starts, an app opens from the Dock, Kraz browses, and the resolution changes from Preferences.

## Phase 6 — Applications and distribution

Goal: install software by dragging the app from a disk image to `/Applications`, and have the basic everyday apps. This phase is the first real use of the system and the best source of bugs.

**DMG**

1. Our own `DiskImages.framework` (MIT/BSD): read the UDIF format with its usual compressions, then write it.
2. Mount through DiskArbitration at `/Volumes/<label>`, using Phase 1's HFS+ driver.
3. A minimal `hdiutil` (`attach`, `detach`, `create`) on the framework, and double-click in Kraz.

**Applications**

| App | Recommended option | Why | Critical dependency |
| --- | --- | --- | --- |
| Terminal | Write in Cocoa | Exercises `NSTextView`, pty and fonts | AppKit text, CoreText |
| TextEdit | Write in Cocoa | Exercises documents, RTF and the Cocoa Text System | `NSDocument`, CoreText |
| Browser | Ladybird (BSD-2) | Already has its own AppKit UI | Foundation networking, AppKit |
| IDE | CodeEdit, in the long term | Depends on SwiftUI, excluded from API 19–21 | SwiftUI |

A risk to flag: many current Cocoa apps use SwiftUI, which is not on the roadmap. It is better tracked as an open decision than found out late.

Commands that come with this phase (moved from Phase 2):

- [ ] `osascript`, with an Open Scripting Architecture for the apps.
- [ ] `opendiff`, with a file comparison app.
- [ ] `tmutil`, with backups.
- [ ] git's Perl, Python and Tcl commands (`git send-email`, `gitk`…), with those languages (moved from Phase 2 on 2026-10-31).

Ahead of time: `hdiutil attach` already mounts images in `/Volumes` through DiskArbitration (2026-10-30); reading the DMGs' UDIF format is missing.

**Exit gate (M6):** a DMG is downloaded, mounted, the app copied to `/Applications` and run; Terminal, TextEdit and the browser are used daily on the development machine itself.

## Phase 7 — Binary compatibility and arm64

Goal after Minkar 19.6.0: run x86-64 binaries built for the reference platform without recompiling, and port the system to arm64. It does not block the critical path, but some decisions must be taken now so as not to close it off.

**Decisions that protect this phase from today**

- Dylib and framework install names identical to the originals (`/System/Library/Frameworks/...`). *Done 2026-10-25* for every framework (AppKit in `Versions/C`), `libobjc.A.dylib` and `libbz2.1.0.dylib`, with the original compatibility versions, checked with dyld in the image; `libicucore` exports ICU's plain names (`u_strlen`).
- Each dylib's compatibility and current versions aligned with the closed API (API 19 → 20 → 21), not with the Minkar release: since 2026-10-31 `libsystem_info` and the pthread kext keep their own versions instead of `PROD_VERSION` (`d2b559759f`).
- No changes to the layout of public ivars or to the sizes of structs exposed in headers.
- `ProductVersion` and `uname -r` are documented together; the compatibility matrix records which apps fail by deriving the platform version from Darwin.

**Work (after M6)**

1. Load binaries built for the reference platform in dyld: code signatures ignored or verified without the original certificate chain, hardened runtime tolerated.
2. A compatibility matrix with real x86-64 apps: starts, works, fails and why.
3. arm64: bootloader, XNU arm64 platform and cross toolchain; start in QEMU `virt`.

**Exit gate (M7):** a set of x86-64 apps downloaded from third parties runs without recompiling; the arm64 image boots to M2 in QEMU.

## Milestones, dependencies and critical path

Critical path: Phase 0 (M0) → Phase 1 (M1) → Phase 2 (M2) → Phase 3 (M3) → Phase 4 (M4) → Phase 5 (M5) → Phase 6 (M6, Minkar 19.6.0); Phase 7 (M7) and Minkar 21.0's performance run apart.

M0 and M2 are met, M1 waits only for the second PC, and Phase 2 is closed (2026-10-31). Active work is in Phases 3 and 4, which must close before Gienah. The GPU track starts in parallel and feeds Minkar 21.0's performance requirement: equal to or better than the reference system on the same machine. Phase 7 comes after Minkar 19.6.0.

| Gate | Phase | State on 2026-10-31 |
| --- | --- | --- |
| M0 | Build, toolchain, CI | Met (CI green since `05fa75cb65`) |
| M1 | Darwin boot | Met in QEMU and on PC 1 (SATA, NVMe, SMP); PC 2 missing |
| M2 | Base userland | Met (`boottest GATE=M2`, 13 markers); Phase 2 closed on 2026-10-31 |
| M3 | Frameworks and Swift | Not started; prior work in Foundation and libSystem |
| M4 | Graphics and WindowServer | In progress: WindowServer does not run yet |
| M5 | Gienah desktop | Not started |
| M6 (Minkar 19.6.0) | Apps and distribution | Not started; `hdiutil attach` already mounts |
| Minkar 21.0 | API 21 complete and performance ≥ the reference system on a physical machine | Not started; depends on the GPU track |
| M7 | Binary compatibility, arm64 | After Minkar 19.6.0 |

## Risks and mitigations

The dominant risk is people, not technology: a small team with many layers open at once. The general mitigation is to close gates in order and limit work in progress.

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Few developers experienced with XNU and kexts | Phase 1 drags on and blocks everything | Document the boot step by step; only QEMU hardware until M2 |
| The public CF-Lite predates API 19 | CF APIs missing for API 20 and 21 | Add them by hand with swift-corelibs CF as reference; bridging tests on every change |
| Heterogeneous real hardware | Bugs that cannot be reproduced | A short reference hardware list; the rest "best effort" |
| GPU drivers: the longest block, and now a requirement | Minkar 21.0 does not match the reference system | A parallel track from now; one GPU family; a KMS prototype before committing; a compositor designed for the GPU |
| SwiftUI off the roadmap | Many modern apps do not build | Decided: API 19–21 close without SwiftUI; OpenSwiftUI watched; measure how many reference apps use it |
| Licenses and clean room | Code that has to be removed | A license review on every third-party import; a clean-room statement on every PR |
| Work scattered across many layers | Nothing becomes usable | A public board per phase; at most two active phases at once |
| Out-of-date documentation (archived wiki) | Contributors work from old information | The roadmap and state in `Docs/` in the repo, updated at each milestone |

## Next steps

In critical-path order:

- [x] **PC 1 (Phase 1):** NVMe (boots from the SSD) and SMP (all eight threads run), 2026-10-29 and 30; power button and ACPI S5 power-off, 2026-10-31.
- [ ] **PC 1 (Phase 1):** boot without `pci_keep_console`, IPv6 on the network (RDNSS, DNS over IPv6) and ACPI S3 sleep; then a second, different PC (gate M1; to measure the reference, the Mac Pro 6,1; the machine with the RX 6600, as a test machine).
- [x] **Userland (Phase 2):** closed on 2026-10-31 (exFAT reading and writing, `launchctl kickstart`, dyld paths, notifyd and IPv6 tested; the rest went to Phases 3, 5 and 6).
- [ ] **Phase 4:** IOHIDSystem event delivery in the kernel, the `ApplePS2Mouse` driver and WindowServer's input reader.
- [ ] **Phase 4:** install WindowServer's job and get a first boot that draws on `IOGOPFramebuffer`.
- [ ] **Phase 3:** the Foundation classes the symbol report misses most (`NSURLSession`), and what came from Phase 2 (libSystem, noisy stubs, `logd`).
- [ ] **Phase 3:** the Swift toolchain prototypes.

What this plan adds, and its state:

- [x] **Versions (2026-10-31):** `PROD_VERSION = 19.6.0` for `ProductUserVisibleVersion`, `PROD_COMPAT_VERSION = 10.15` for `ProductVersion`, and `sw_vers` showing "Minkar 19.6.0 (kernel Darwin 19.6.0)", with its manual page (`d2b559759f`). Tested in QEMU: `@available(10.15)` true and `(11.0)` false, `operatingSystemVersion` 10.15.0, `uname -r` 19.6.0. `libsystem_info` and the pthread kext keep their own versions. In the same work: `launch_activate_socket` (`5051c781b4`), `os_log` and `os_activity` (`0709544df7`), `_availability_version_check` (`c0923c1bc7`) and `NSProcessInfo` (`fb6e25b877`).
- [x] **This roadmap up to date (2026-10-31):** rewritten from the plan, with Gienah and Kraz, the API 19/20/21 → Minkar 19.6.0/20.0/21.0 scheme, the GPU track and the performance requirement.
- [ ] **Version compatibility:** a `SystemVersionCompat.plist`-style mechanism and `SYSTEM_VERSION_COMPAT=1` for binaries linked against an older SDK.
- [ ] **Decide the first GPU driver's family** and write the ADR with the KMS prototype. It is also needed to turn the display off, which the GOP framebuffer cannot do.
- [ ] **Measurement:** measure the reference system on original hardware (Mac Pro 6,1) and keep the values out of the public repository, for Minkar 21.0's performance gates.
- [ ] **NVMe performance in QEMU:** reads through the block device run at 1 MB/s.

## Sources

- [The minkaros/minkar repository](https://github.com/minkaros/minkar), branch `devel`, verified at `fb6e25b877` (2026-10-31).
- [Hardware.md](Hardware.md): reference machines.
- [Commands.md](Commands.md): Minkar's commands against the reference list.
- [Userland.md](Userland.md): the history of ported commands (exFAT, `notifyutil`, `launchctl kickstart`).

The state comes from the repository; the architecture decisions, performance requirements and graphics stack sequence are this plan's proposals, and the ones that are confirmed are recorded here.
