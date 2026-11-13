# Minkar — Architectural Roadmap

2026-10-31

## Summary

Minkar's goal is a system on Darwin 19.6.0 that builds and runs Cocoa apps. The first release, **Minkar 19.6.0**, delivers API 19 with the Atlas desktop and its own visual language; Minkar 20.0 and 21.0 complete API 20 and API 21 without changing the kernel. The critical path goes strictly bottom-up: Darwin boot → launchd → userland → frameworks → WindowServer → Atlas → apps. Full Objective-C and Swift, DMG, binary compatibility and arm64 run in parallel or are deferred so they do not block that path.

The plan has 8 phases (0–7), each with deliverables and an exit gate that CI can check. No phase is closed while the one below it fails its gate. Since 2026-10-29 there is also a performance requirement: equal to or better than the reference system on the same physical machine, required from Minkar 21.0 on (see Performance requirements).

**Source of truth:** this file. [ROADMAP.es.md](ROADMAP.es.md) is the same plan in Spanish, and [Progress.md](Progress.md) summarizes its state: a table, a metro map of milestones by plan and a work board, generated from `progress/status.toml`. Verified against `devel` at `82c5f3e8f8` (2026-10-31), 140 commits ahead of `main`.

### Principles

1. **Verifiable gates, bottom-up.** Each phase ends with an automated test that boots the image in QEMU. No green test, no closed phase.
2. **The contract is the public Cocoa API, by API number.** API 19 first, then API 20 and API 21. Every symbol is classified as implemented, stub or missing, and measured.
3. **Noisy stubs.** Every stub logs its first call to the system log, so real apps tell us what is missing, in the order they use it.
4. **Reuse before writing.** Open source APSL code first (xnu, dyld, launchd, hfs), then swift-corelibs, then FreeBSD's userland. New code only for the gaps.
5. **Clean licenses.** Code written from scratch for Minkar is under BSD-2-Clause or MIT; the third-party libraries and frameworks we modify keep their original licenses. A clean-room rule: no one who has seen the reference platform's closed source contributes code. Third-party trademarks are not used in names, icons or public text; a single TRADEMARKS.md declares them. One exception, accepted by the maintainer because no icon set of the same scope exists under BSD or MIT: Atlas's icons are the Faience and Faenza themes (GPL-3), separate data files in `SystemLibrary/Icons` that keep their GPL-3 license; no code links with them, and the rest of this rule stands.
6. **Reference apps as acceptance tests.** A fixed set of open source apps written for the Cocoa API is built at each milestone; the symbols they import set the order of the framework work.

## Version decision

Minkar separates three layers that go together on the reference platform. The kernel stays at Darwin 19.6.0; the API and the look move on their own.

| Layer | Decision | Note |
| --- | --- | --- |
| Kernel and BSD base | Darwin 19.6.0 | Touched only when a new API needs a kernel interface |
| API | API 19 first; then API 20 and API 21 | Each API driven by the reference apps' imports |
| Look | The Atlas visual language from Phase 5 | Our own design and guide; no third-party icons, fonts or artwork |

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
| 19.6.0 | API 19, the Atlas desktop complete for daily use (MB2–MB3, D2–D3, K2–K3, apps A0–A10), WebKit (W0–W5), audio and media (AU1–AU6) and DMG; functional gates M0–M6 on physical hardware | 19.6.0 | 10.15 |
| 19.6.x | Fixes, and MB4, D4 and K4, and professional audio (AU7–AU10); may carry parts of API 20 | 19.6.0 | 10.15 |
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
| PC 2 | Intel Xeon E5-2697 v2 (12 cores, 24 threads), 40 GB, Samsung NVMe, Fresco Logic FL1100 xHCI, Broadcom BCM57762, two AMD FirePro D500 (Tahiti); the vendor's own hardware (2026-11-01): Minkar's second PC (gate M1), and the vendor's original hardware to measure the reference | Same |

- **How it is measured:** Minkar and the reference system on the same machine (dual boot), with the same benchmarks run over SSH. The reference system is measured only on its vendor's original hardware (candidate: PC 2), as its license allows no other, and its values are kept out of the public repository.
- **Consequence:** if the reference is measured only on PC 2, Minkar must be measured there too, and its GPUs (two AMD FirePro D500: Tahiti, GCN 1.0, PCI 1002:679e; the oldest family we have and the least maturely supported) become the first high-performance driver's. The model's other configuration, the FirePro D300, is Pitcairn: another chip with its own firmware, never tested (the driver knows its tables and takes its microcode, `pitcairn_*.bin`; pending until a D300 is at hand).
- **GPU families (decided 2026-11-01, [ADR 0002](ADR/0002-GPU-First-Family.md)):** first GCN 1.0, PC 2's FirePro D500 (Tahiti), where the reference is measured; then RDNA2, PC 1's RX 6600 card (Navi 23, which drives its display today through the firmware's framebuffer; its steps R0–R5 in [GPU-RDNA2.md](GPU-RDNA2.md), R0 done); then Intel Gen9, the HD 630 integrated in PC 1's i7-7700.

## Baseline

Minkar boots on Darwin with its own EFI loader, the whole system builds as Mach-O, and it already runs on a physical PC with its eight CPU threads and from its NVMe SSD. Phase 2 is closed; the biggest gap is between the kernel and the desktop. State on 2026-10-31:

| Component | State | What is missing | Phase |
| --- | --- | --- | --- |
| EFI loader + Darwin kernel | Boots in QEMU (TCG and KVM), on PC 1 from SATA or NVMe, and on PC 2 from NVMe; SMP with every CPU (24 threads on PC 2); the power button shuts down in order | AMD processors, MSI-X | 1 |
| System build | Full `world`; CI builds and boots it on `main` and by hand | — (M0 met) | 0 |
| libSystem | The reference apps find all their libSystem imports (308 of 308); `os_log` writes to logd (`log show`); `@available` compares against `ProductVersion`; dyld's @rpath, @loader\_path and re-exports tested | Noisy stubs, `logd`, dyld's shared cache | 3 |
| launchd / launchctl | launchd-842 as PID 1; launchctl with kickstart; `KeepAlive PathState`, `WatchPaths` and `QueueDirectories` | Nothing | 2 (closed) |
| Userland and system tools | Every Phase 2 command (337 of the reference list's 354) | `open` and the commands of later phases | 2 (closed) |
| DiskArbitration | The original framework and `diskarbitrationd` (APSL); our own `diskutil` and `hdiutil`; mounts HFS+, FAT and exFAT | — | 2 (closed) |
| CoreFoundation | CF-Lite (APSL) | The APIs missing up to API 21 | 3 |
| Foundation | Three batches of fixes (2026-10-26); `NSProcessInfo` with `operatingSystemVersion`; re-exports CoreFoundation; `NSURLSession` on libcurl, with cookies; `NSOperationQueue` on dispatch; the main run loop runs dispatch's main queue; `NSDateFormatter` on ICU (2026-10-31); `NSXPCConnection` over libxpc, with the XPC services inside app bundles (2026-10-31) | URL cache and task metrics in `NSURLSession`; the API 19 audit | 3 |
| Swift 6 | The Swift 6.0 runtime and standard library in `/usr/lib/swift`, built from `swift-6.0-RELEASE` by the toolchain's own Swift compiler; the overlays (Foundation, Dispatch, ObjectiveC...), OpenCombine as Combine and XCTest; gate M3 met (5 Nov 2026: seven open source command-line tools pass their own tests) | Swift Concurrency on the main actor in apps, SwiftUI (out of scope until API 21) | 3 (M3 met) |
| WindowServer | The new core is the window server (WS4b done 2026-11-02, gate `M4`): its interface in `SkyLight.framework`, its server started at boot as `_windowserver` with the minimal `loginwindow`; it composites AppKit windows with pixman, routes input from IOHIDSystem with event taps, and each app draws its own menus (MB0). The legacy server is gone. WS5 done (2026-11-02): IOSurface with its registry `iosurfaced`, IOSurface windows, a virtio-gpu 2D driver flushed per frame with mode changes, permanent display configurations, `CGDisplayStream` | The GPU (WS6): G1 done on PC 2 (display driver); G2 under way (its gate met) | 4 |
| GPU acceleration | On PC 2's AMD FirePro D500 (Tahiti, GCN 1.0, PCI 1002:679e): the display at its native mode, DMA and the command processor, Mesa's RADV (Vulkan 1.3, a CTS subset with no failure), and the window server composites on the GPU (G1 to G4) | The GPU's clocks, Pitcairn (FirePro D300) and other families, OpenGL, the measurements against the reference | 4 and Minkar 21.0 |
| loginwindow | partial: the graphical login box and the desktop picture, a session per user | Authentication through Directory Services, the user's LaunchAgents, the full Phase 5 LoginWindow | 5 |
| SystemUIServer (menu bar) | partial: each app draws its menu bar (MB0); SystemUIServer is built and shows the clock | Menu Extras and the full status item API (MB1), standard menus (MB2), the full system menu (MB3), Services (MB4, 19.6.x) | 5 |
| Dock | partial: the new core with the reference's preference keys, launching and the running apps (D0, D1) | Interaction: reorder, add, remove, context menus (D2); tile service, minimized windows, app switcher (D3); stacks and Trash (D4, 19.6.x) | 5 |
| Files | partial: browsing with sidebar, icon, list and column views, opening files and apps, basic Get Info (K1) | The rest of K0 (AppKit views checked), file operations (K2), desktop icons and volumes (K3); search, tags and previews (K4, 19.6.x) | 5 |
| Text Editor | partial: plain text documents, open and save (through the save panel), `open -e` | RTF, find and replace, encodings, line endings, printing (A2) | 5 and 6 |
| System Preferences | partial: the app, `PreferencePanes.framework` and five panes (Displays, Dock, Date & Time, Keyboard, Network) | The Users pane (waits for Directory Services), the rest of the panes | 5 |
| A0 App foundations | done (8 Nov: About, help books, documents with versions, the template; Atlas.md 5.3) | The app template: About and Preferences windows, Help menu, Spanish and English, `NSDocument` | 6 |
| Terminal (A1) | done (8 Nov: libvterm, tabs, Preferences; `vttest` not automated yet) | The whole app: a VT100/xterm emulator over a pty, tabs, scrollback, Agave | 6 |
| System Monitor (A3) | done but energy (8 Nov; per-process energy waits for the power driver's data) | The whole app: processes, CPU, memory, energy, disk, network, quit and force quit | 6 |
| Logs (A4) | done but writing crash reports (8 Nov; the app shows them, a crash reporter is still to come) | The whole app, on `logd` and `os_log`; crash and panic reports | 6 |
| Screenshot (A5) | missing | The whole app, on `screencapture` and the window list; the system shortcuts | 6 |
| Image Viewer (A6a) | missing | The whole app, and ImageIO | 6 |
| Document Viewer (A6b) | missing | The whole app, and PDFKit | 6 |
| Disks (A7) | missing | The whole app, on DiskArbitration, `diskutil` and `hdiutil` | 6 |
| System Information (A8) | missing | The whole app, as `system_profiler` | 6 |
| Calculator (A9) | missing | The whole app: basic, scientific and programmer modes | 6 |
| Archive Manager (A10) | missing | The whole app, on libarchive and `ditto` | 6 |
| Fonts (19.6.x) | missing | The whole app | 19.6.x |
| Passwords and Keys (19.6.x) | missing | The whole app, and the Keychain it shows | 19.6.x |
| Color Picker (19.6.x) | missing | The whole app, and `NSColorPanel`'s pickers | 19.6.x |
| Script Editor (19.6.x) | missing | The whole app, and a scripting runtime | 19.6.x |
| Help (19.6.x) | missing | The whole app, and the Help menu's search | 19.6.x |
| Characters (19.6.x) | missing | The whole app, and the character palette | 19.6.x |
| WebKit (W0–W5) | partial: W0's libraries built and their own tests passing on PC 2 (21 of 23); JavaScriptCore with its JIT, gate W1 running (es6 605 of 605); WebCore and the processes on WPE, gate W2 passed; `WKWebView` (W3) and the older `WebView` (W4) on WPE | FreeType's and WOFF2's own tests; W1's other suites; the reference apps' web views (W5) | 3 and Minkar 19.6.0 |
| AppKit | Partial (Cocotron) | The API's recent pieces and the Atlas visual language | 5 |
| DMG, base apps | `hdiutil attach` already mounts through DiskArbitration | `DiskImages.framework` (UDIF), apps | 6 |
| Binary compatibility, arm64 | Future; install names already the same as the originals | The rest | 7 |

## Phase 0 — Build, toolchain and CI

Goal: any contributor gets a bootable image from scratch with one command, and every PR is tested by booting it in QEMU. Without this, every other phase works blind.

**Work**

- [x] Pin the toolchain: `tools/toolchain/MANIFEST` pins each component's upstream version, git tree and version line (clang 17.0.6 from `swift-6.0-RELEASE`, ld64-609, and since 2026-10-29 cctools with `ctf_insert` and dtrace's CTF tools); `manifest.sh check` verifies it in CI and checks that one entry builds, links and archives to the same bytes twice.
- [x] Move the whole toolchain to `swift-6.4.2-RELEASE` (LLVM 21.1.6; decided 9 Nov 2026): WPE WebKit needs clang 18 or later, and the pinned clang 17.0.6 (`swift-6.0-RELEASE`) stops it. Compiler, Swift runtime and standard library, libc++, libunwind and compiler-rt move together; Minkar's changes to LLVM carried over (`__MINKAR__`, the `minkar` triple, libSystem without static initializers in compiler-rt and libc++), tapi-1600 adapted to LLVM 21 (there is no later tapi). Done when the whole system builds with it and the full suite and gates M2 and M3 pass. *Done (2026-11-10, `38d3af4398`..`3a3052ab07`):* a build from scratch with no step by hand, `manifest.sh check` (the same bytes twice), batch 1966/0 with gates M2 and M3, full suite with no failure of its own (three fail the same with clang 17: `display-reconfigure` twice and the console's line wrap). On the way: libc++ 21's headers in the SDK, the bootstrap stubs by `llvm-readtapi`, the Darwin overlay and `dispatch_async_swift_job` weakly imported for Swift 6.4, the system version read without CoreFoundation while libraries initialize (every Combine program crashed), Foundation's `isNSString__` and the other class tests Swift 6.4's runtime sends, CoreServices' Unicode collation (GLib uses it once Carbon is there), and an autotools tree's generated files dated after their sources.
- [x] bmake targets per layer: `bmake <layer>` and `bmake <layer>-check`, `layers-check` and `image-check`. The third-party frameworks moved to Phase 3.
- [x] CI: `.github/workflows/build-boottest.yml` builds the toolchain (cached), the system and the image, and passes `boottest GATE=M2`. Since 2026-10-29 it runs on `main` and by hand, so as not to run out of a free account's Actions minutes.
- [x] Symbol inventory: `tools/symbols/`; Minkar covers 72.0% of the four reference apps' system imports (68.3% on the morning of 2026-10-31).
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
- [x] Reference hardware list ([Hardware.md](Hardware.md)): PC 1 and PC 2 (2026-11-01) documented; AMD below.
- [ ] More than 64 processors and x2APIC (after Minkar 19.6.0); a larger machine runs on its first 64 today.
- [ ] AMD processors: the kernel recognizes them, but Minkar has never run on one. First in QEMU with `-cpu EPYC`, then on an AMD PC.
- [ ] Boot without `pci_keep_console=1`; IOKit takes more than 30 s to go quiet.
- [ ] IPv6 on PC 1's network: RDNSS and DNS answers over IPv6, which QEMU's user network does not give (moved from Phase 2 on 2026-10-31).
- [ ] PC 1's Wi-Fi (Intel, M.2): no driver.

* [x] **Exit gate (M1):** met in QEMU, on PC 1 (from SATA and from NVMe, with SMP) and on PC 2 (2026-11-01: from NVMe, 24 threads under load, 40 GB, storage and a reboot pass in `minkar-hwcheck`; `9c3dbe9873`, `c129adf018`, `e9a4446222`).

## Phase 2 — Base userland

Goal: a complete multi-user Unix system on Darwin, managed by launchd, with the services Atlas and the apps will need later.

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
1. Complete CF-Lite up to API 21, with bridging tests on every change.
   - [x] CF's string functions take Foundation's own strings (`NSMutableString`'s were read as garbage by `CFLocaleCreate`, `CFStringGetCString`...) (`42a40ffab0`, test `foundation-cf-string-bridging`, full suite 2888/0).
   - [x] `CFArray` and `CFDictionary` are toll-free bridged to `NSArray` and `NSDictionary`, both ways: CF's objects take Objective-C messages (`__NSCFArray`, `__NSCFDictionary`) and CF's functions take Foundation's collections. Evidence: commit "CoreFoundation and Foundation: arrays and dictionaries bridged both ways", test `foundation-cf-bridging` (13/0), full suite 2761/0, M2 PASS.
2. Audit Foundation against API 19, starting with what the reference apps import. *Done (2026-10-25/26):* `NSException`, `NSUUID`, `NSSecureCoding`, `NSRegularExpression`, `NSJSONSerialization` (with JSON5), `NSCache`, `NSInvocation` with the System V calling convention, weak `NSHashTable` and `NSMapTable`, binary property lists byte for byte as the reference writes them, `NSProgress` and the modern `NSString` API, plus dozens of bugs found while testing. *Done (2026-10-31):* `NSProcessInfo` with `operatingSystemVersion` (10.15.0), `isOperatingSystemAtLeastVersion:`, `operatingSystemVersionString`, processors, memory and `systemUptime` (`fb6e25b877`). *Done (2026-10-31):* `NSURLSession` on libcurl: configuration, data, upload and download tasks, delegate and completion handlers, redirects and authentication challenges through the delegate, resume data, `file:` and `data:` URLs (`00617d5f6c`); the main thread's run loop runs dispatch's main queue, which never ran in a program driven by `NSRunLoop` (`c88f713757`); `NSOperation`, `NSBlockOperation` and `NSOperationQueue` rewritten on dispatch, with `mainQueue`, `currentQueue`, concurrency limits, asynchronous operations and barriers (`3f886f812e`); `NSData` Base64 and `rangeOfData:options:range:`, `NSHTTPURLResponse` (MIME type, charset, unknown length), four `NSError` keys that did not link, relative URLs that lost their port, `MIN`/`MAX`/`ABS` (`81d2593fb7`). Tests `foundation-url-session` (30/0, HTTPS included), `foundation-operation-queue`, `foundation-main-queue`, `foundation-url-support`; full suite 2826/0, M2 PASS. Then (`b48794b0dc`, `dc214ec2ce`, `852676bdeb`, `5671610ec9`): HTTP cookies (`NSHTTPCookie` and `NSHTTPCookieStorage` were empty), sent and stored by `NSURLSession`; `pathComponents` kept no `/`, so `createDirectoryAtPath:withIntermediateDirectories:` made absolute paths relative; `addObserverForName:object:queue:usingBlock:`; `NSDateFormatter`'s default behavior on CFDateFormatter (Unicode patterns, styles, locales; only `%Y` formats worked). Tests `foundation-cookies`, `foundation-paths-blocks`, `foundation-date-formatter`; full suite 2871/1 (an intermittent test, passed again), M2 PASS. Then the URL API of files (`8b9592659f`, `4738af30ea`, `e2ebc901c5`): every standard directory in `NSSearchPathForDirectoriesInDomains` (it knew three), file URLs as the API makes them (`file:///a%20b/`, `path` decoded), NSURL's resource values and bookmarks, NSFileManager's and NSBundle's URL methods; test `foundation-file-urls`; full suite 2907/0, M2 PASS. `tools/symbols/selector-inventory.py` (`093bf4113e`) lists the Objective-C messages the reference apps send that no Minkar class implements: 80.0% of 3305 are there on 2026-10-31 (77.9% before the file URLs). Then processes, files and formatters (`2ebf423f7b` to `15c3a8fd96`, 9 commits): NSTask on posix_spawn with its modern API (it closed the child's standard output when not set and reaped every child of the process); NSFileHandle's error methods and readability handlers; block timers; the file manager freed while in use (its isa was written by hand) and writes that did not truncate; NSURLComponents and NSURLQueryItem; NSDateInterval; NSByteCountFormatter, NSDateComponentsFormatter, and on ICU NSRelativeDateTimeFormatter, NSDateIntervalFormatter and NSListFormatter; NSDateComponentUndefined as NSIntegerMax. Tests `foundation-process-io`, `foundation-components-formatters`; full suite 2943/0, M2 PASS; selectors 81.2%. *Left in `NSURLSession`:* the URL cache, task metrics (needs `NSDateInterval`), stream and WebSocket tasks, server trust challenges (needs Security). *Known bug:* `NSKeyedUnarchiver` cannot read the XML property list `NSKeyedArchiver` writes. Untested: locale-dependent behaviour (UTF-8 locale data is in the image since 2026-10-27).
3. Security (APSL) for certificates and a basic keychain. TLS in `NSURLSession` works without it (libcurl with mbedTLS and `/etc/ssl/cert.pem`); `SecTrust` server trust challenges and client certificates need it.
4. LaunchServices: bundle database and UTI types; `open` and Files depend on it. *Done in WS4a:* the session service, which keeps the running applications (`launchservicesd` and the session client in `LaunchServices.framework`, `996178d949`). *Done (4 Nov 2026, `0331f57894`, `345636f2f5`):* launchservicesd keeps the applications installed, their document types and declared types, says which applications open a file and hands documents to an application (at once, or after its launch); `LSOpenCFURLRef`, `LSOpenFromURLSpec`, `LSCopyDefaultApplicationURLForURL`, `LSCopyApplicationURLsForURL`, `LSRegisterURL`, `LSRevealInFiles`; AppKit delivers documents to `application:openURLs:` and NSWorkspace opens through it. Local test `launchservices` 24/0. ravynOS's SQLite database code is gone (`e7ae74a2e5`).
5. CoreText and CoreData: a minimal implementation driven by real use (see the graphics stack's technical guide).
6. Swift 6: full stdlib and runtime, with concurrency in the system, and module maps for the frameworks.
7. API 19 → 20 → 21, raising `ProductVersion` (10.15 → 11.0 → 12.0) as each one closes; each close is a Minkar release (19.6.0, 20.0, 21.0).

From Phase 2 (2026-10-31):

- [ ] Complete libc, libm, pthread, libdispatch, libxpc, notify and os\_log. *Partial:* since 2026-10-31 the reference apps find all their libSystem imports (308 of 308): `os_log` writes its messages to syslogd, with `%{public}`/`%{private}` and `%@`, and to stderr in DT mode (it wrote nothing before, and the SDK's `<os/log.h>` had no `os_log()`); `syslog(3)` reaches syslogd again; `os_log_error`/`os_log_fault` under the reference's names; `os_activity`; `_availability_version_check` for `@available`; `launch_activate_socket` (`0709544df7`, `c0923c1bc7`, `5051c781b4`). The other libraries in `/usr/lib` they import are there too: `libsqlite3` (SQLite 3.54.1, with the `sqlite3` shell, `bbe573a8a5`), `libxar` (xar-503) and our own `libcompression` (ZLIB, LZMA, LZFSE) (`cae9a9c4f4`); CommonCrypto has `CCDigest` (`88e4800b96`). libxpc has real connections since 2026-10-31 (`b2a0bee6ce`): anonymous listeners and endpoints, Mach services through launchd with interruption and relaunch, replies, descriptors, ports and shared memory in messages, the sender's credentials; arrays work (they could hold nothing before). Test `libxpc-connection` (14 checks). Since 2026-11-03: XML property lists (`xpc_create_from_plist`), the peer's entitlements (`xpc_connection_copy_entitlement_value`, from the kernel's copy of its signature) and transactions on liblaunch's (`219ac6ca38`, `4709c75447`; tests `libxpc-plist`, `libxpc-transactions`). `xpc_set_event_stream_handler` is still a stub: launchd's side is there, no event provider yet. `NSObject` is `isa` alone, as in the reference ABI (it carried CF's header fields; the bridged classes declare them themselves, `a3a9d005fc`); what Swift's runtime needs of the SDK: `<os/signpost.h>`, `os_system_version_get_current_version`, `_dyld_is_objc_constant` (`2f0c5038af`). Foundation re-exports CoreFoundation, as the reference system's does (`55ac68b1f0`). `math.h` matches the reference since 2026-10-26; `vis(3)` lacks the newer flags.
- [ ] Noisy stubs (log on the first call). *Partial (2026-10-31):* `<minkar/stub.h>` logs a stub's first call in a process (`org.minkar.stubs`; to logd since 2026-11-03, read with `log show`), with the image that called it, and to stderr in DT mode; the 875 unimplemented methods and functions of Foundation, AppKit, CoreData and QuartzCore use it (they printed on stderr at every call), and libxpc's 12 silent stubs say so now. `tools/symbols/trivial-functions.py` lists the exported functions that do nothing (404 on 2026-10-31, many of them third-party and empty by design): the rest are reviewed from that list.
- [ ] dyld's shared cache (optional at first).
- [x] `logd`, with `os_log` (2026-11-03, `5d587b3d44`, `6e7a02196d`): `os_log` sends its messages to logd, which keeps default, error and fault on disk and info (debug on request) in memory; the `log` command shows, streams, configures and erases them, with NSPredicate predicates; each user sees their own. Test `logd` 23/0. syslogd gets them only when there is no logd to take them; `log show` and `log stream` show `syslog(3)`'s messages too (from syslogd's store).

**Swift toolchain** (the prototype gates the rest of the Swift work)

- [x] Prototype 1: build `swiftc` (`swiftlang/swift`, branch `release/6.0`) on the Ubuntu host as a cross compiler, against the tree's `swift-6.0-release` LLVM. *Done (2026-11-03, `1d2b7e8d9d`):* `Developer/Default.xctoolchain/swift` builds `swift-frontend` (`swiftc`, `swift`) for the host against the toolchain's LLVM and clang, from Swift's release tarball pinned by SHA-256 (one patch: no ASTGen without swift-syntax); no macros yet. Since 4 Nov 2026 with swift-syntax and macros (`bf2464179d`).
- [x] Prototype 2: build `libswiftCore` for `x86_64-apple-macosx10.15` against `Minkar.sdk` and run a (non-Embedded) Swift "hello world" in QEMU. *Done (2026-11-03, `1d2b7e8d9d`):* `Libraries/swift` cross-compiles the runtime and standard library from Linux into `/usr/lib/swift`; it needed NSObject at the reference size (`a3a9d005fc`) and a few SDK pieces (`2f0c5038af`). Test `swift` 10/0: strings, protocols, classes, generics, errors and collections.
- [x] `libswift_Concurrency` and the system overlays: `Darwin`, `ObjectiveC`, `CoreFoundation` and `Dispatch` (from `libdispatch/src/swift`). *Partial (4 Nov 2026, `283d9da3e1`):* Concurrency on libdispatch and the `Darwin` overlay; test `swift` 19/0 (async main, task groups, actors, cancellation, AsyncStream). *Done (4 Nov 2026, `30f8226a8d`):* the `ObjectiveC`, `Dispatch` and `CoreFoundation` overlays from Swift 5.4, built with Swift 6.0 (not from `libdispatch/src/swift`, decided 4 Nov: the Darwin one is the ABI of the apps built for the reference platform); CoreFoundation makes blocks and its objects Objective-C objects without Foundation (`bc807ce599`). Test `swift` 26/0.
- [x] The `CoreGraphics` overlay. *Done (4 Nov 2026, `dc69c6b38f`):* from Swift 5.4 like the others, with `CGFloat` generated by its gyb template. CoreGraphics first got the reference types: `CGFloat` is `double` on 64-bit and `CGContextRef` a CF type (`29f392e725`), counts are `size_t`, the enums are typed, and the API the overlay uses was added with its Swift names in the headers (`f61dfd5b34`). Test `swift` 34/0 (geometry, transforms, colors, paths, drawing into a bitmap). Still to do: Swift names for the rest of CG and the headers' nullability.
- [x] The `libswiftFoundation` overlay from its last open version, updated to Swift 6.0. *Done (5 Nov 2026, `7093882f38`):* the Swift 5.4 overlay built with Swift 6.0 against Minkar's Foundation, with its Combine schedulers and publishers (run loop, operation queue, timer, notifications, KVO, URL session; DispatchQueue's in the Dispatch overlay). Foundation first got what Swift reads in it and what it was missing (`a3b073f23b`): the reference types (`NSStringEncoding`, `NSComparisonResult`, `NSRectEdge`), typed string enums, properties, generics and nullability in the headers, and API 19 the overlay calls: NSCalendar and NSLocale on CF (`16ab71b9a3`), NSDecimal's arithmetic and NSDecimalNumber (`e25e40ac29`), NSOrderedSet, the collection differences, units and measurements, NSPersonNameComponents, NSLinguisticTagger, NSISO8601DateFormatter and NSItemProvider (`df5cbb9610`). KVO recursed forever on an observed setter and had no initial notifications (`7233b00fcd`). Tests `swift` 71/0 (Foundation and Combine from Swift), `foundation-api19` 49/0; full suite 3783/1 (a flaky cursor test, passed alone), M2 PASS. *Left:* URLError's peer trust and server trust (need Security); `URLSessionWebSocketTask` is a stub that fails as unsupported.
- [x] Combine (API 19) from OpenCombine. *Done (5 Nov 2026, `7b305aa383`):* OpenCombine 0.14.0 (MIT) built as `Combine.framework`, a library evolution module that programs link as a framework; `Optional.Publisher` and `Result.Publisher` nested as in Combine. Tested with subjects, operators, `@Published`/`ObservableObject`, `Future`, `Zip`, decoding, and the Foundation and Dispatch schedulers and publishers.
- [x] XCTest, for the gate's test suites. *Done (5 Nov 2026, `78caf199df`):* swift-corelibs-xctest (Apache-2.0) at the toolchain's release, built on Minkar's Foundation as `XCTest.framework` (the platform's `Developer/Library/Frameworks` for building, `/Library/Developer/Frameworks` in the image). Without a package manager, `xctest-discover` writes a test program's main from its sources (subclasses, inherited tests, extensions, async tests). It needed `NSProcessInfo`, `NSFileHandle` and `NSDateFormatter` in the reference's shape, and `getCFRunLoop`, which returned the NSRunLoop itself (`bf327957e7`). Local test `xctest` 15/0.
- [x] Install everything in `/usr/lib/swift` in the image and add a `boottest` gate that runs a Swift binary. *Done (`ea8f2964f1`):* gate M3-swift runs `swift-hello` on `/usr/lib/swift`'s runtime, and gate M3 runs the tools' Swift test programs.

**Exit gate (M3):** a fixed set of open source command-line tools written for the Cocoa API (ObjC and Swift) builds and passes its tests on Minkar. The set (fixed 5 Nov 2026), cross-built on the host, with their tests run in the image (`boottest` gate M3):

| Tool | Language | License | What it exercises |
|---|---|---|---|
| swift-argument-parser, with its examples | Swift | Apache-2.0 | The base of most Swift command-line tools |
| SwiftFormat | Swift | MIT | Foundation at length; a large test suite |
| xcbeautify | Swift | MIT | Regular expressions, strings, Foundation |
| Yams | Swift | MIT | Codable, and a C library linked in |
| Rainbow and Files | Swift | MIT | Files, paths and the terminal |
| mogenerator | Objective-C | MIT | CoreData and Foundation |
| tag | Objective-C | MIT | NSURL's resource values and file tags |

Left out: tools without a license or with an unclear one, and tools that use private API.

*Progress (5 Nov 2026):* the gate runs (`bmake m3-tools`, then `bmake m3-check`: `Tests/M3` builds each tool and its tests without the package manager, `/usr/local/tests/m3/run.sh` runs them, `96f5f65249`). swift-argument-parser passes its five suites, 493 tests. What it needed: `NSTask`, `NSPipe` and `NSFileManager` as Swift reads them, a command-line tool's bundle (it was two directories up), regular expression templates, command-line values that are not property lists without a parse error (`f21472a91d`); character sets bridged to CF both ways (`13a6cebd3d`); the ISO 8601 calendar (`5694846076`); FileHandle's throwing API, and API 21's `FileHandle.bytes`, `lines` and URLSession's async loads (`89b9375fa7`). Local tests `swift` 81/0. Then Rainbow (139 tests) and Files (66 tests) pass (`7d07cc60c5`): AppKit could not be imported from Swift, as CoreVideo, which QuartzCore's headers import, was never built (`d3ee485fac`); NSFileManager could not be subclassed (its operations were in a private subclass that every allocation returned), its header lacked `NSFileAttributeKey`, a relative file URL kept its `.` segments, and `CFFIXED_USER_HOME` was ignored (`987a49fad0`). Local tests `swift` 86/0. Then Yams passes, 192 tests (`11604bfcb9`; C targets in `Tests/M3`): an empty range's line was the one before it, so walking lines never moved on; encoding names were numbers; NSNumberFormatter's scientific, currency and spelled-out styles were unimplemented and now go through CFNumberFormatter (`288eb57cd4`); coding with a configuration, API 21 (`6b0d862c24`); XCTest's metrics API, and test classes available from later systems run only there (`7dbfad469a`). Local tests `swift` and `xctest` 105/0. Then xcbeautify passes, 643 tests (`d2eda99167`; packages that depend on packages in `Tests/M3`): a flat bundle's resources were looked for in a Resources folder it lacks, `\` split file names as `/`, text files without a byte order mark were read as NEXTSTEP, not UTF-8, and NSXMLParser's delegate methods were required (`d1c81abeff`); ListFormatter and RelativeDateTimeFormatter had no Swift names (`de216ae5c0`). Local tests `swift` and `xctest` 110/0. Then mogenerator, the first Objective-C tool, passes its Objective-C and Swift tests (`0c036fd389`; 11 checks: the classes it generates are the expected ones, and its programs save objects): CoreData's model classes could not be built in code nor archived, @dynamic properties had no primitive accessors, the XML store lost dates, and its headers were not in Swift's shape (`484067beb4`); predicates had no CONTAINS or BETWEEN operators, IN tested substrings, LIKE and MATCHES were unimplemented, and they could not be archived (`9bf58d789b`); NSKeyedArchiver dropped conditional objects encoded later, a tool's Info.plist in its executable was not read, TMPDIR was ignored, and KVC had no set or ordered set proxies (`3b7e4f8c30`). Local tests `predicates`, `coredata` and `foundation-tools` 92/0; full suite 3933/0. Then tag passes its 30 checks and SwiftFormat its 4900 tests (`22a0e67f0d`; tag has no tests of its own, so `tagTests` runs each of its commands): NSMetadataQuery and its classes were empty files (`036fdb2fcf`); a relative path made an absolute file URL, `~` made a nil URL, NSTimeZone's known names were nil and errors bridged from Swift crashed NSError (`a4875a6be4`); the kMDItem names (`708e61e7a7`). SwiftFormat's suite showed the release kernel was built at `-O0`: at `-O2` a system call or a thread switch costs a sixth, and the suite runs in 28 minutes instead of 53 (`9db0205544`). *Met (5 Nov 2026):* gate M3 passes with all seven tools (22 markers); full suite 3931/5 on the `-O2` kernel (the five fixed or known flaky).

- [ ] Rainbow's string generator test asks for 50,000 strings a second; on 7 Nov 2026 the gate's VM made about 44,500 on the same Xeon E5-2697 v2 where it passed on 5 Nov, so the gate leaves it out (`SKIP_RainbowTests` in `Tests/M3/rainbow`). Find what made it slower (libSystem still at `-O0`, the VM's Ivy Bridge CPU model since 7 Nov, the Ubuntu 24.04 host's kernel and QEMU), measure it and put the test back.

**Next: Cocoa coverage.** M3's tools are command-line tools and hardly use AppKit. Measured against the four reference apps (5 Nov 2026): selectors 83.5%, imported symbols 74.3%, AppKit 70.2%, CoreFoundation 60.3%. The block works from the inventory's lists of what is missing, targeting selectors ≥ 90%, AppKit ≥ 85% and CoreFoundation ≥ 80%, and adds more open source Cocoa apps to measure against. First: constraint-based layout (`NSLayoutConstraint`, anchors, layout guides, the visual format language, on the kiwi solver) and `NSStackView`, which all four apps use; then WebKit, required for Minkar 19.6.0 (decided 5 Nov 2026): WPE WebKit (LGPL and BSD, the port made for embedding) under Minkar's own `WebKit.framework` with the Cocoa API (`WKWebView`, user scripts and content controllers, navigation and UI delegates, preferences, and the older `WebView`), drawing into AppKit views; the web browser is Firefox (decided 11 Nov 2026: Ladybird removed its AppKit interface on 29 Sep 2026 and is built with Qt on the reference platform, while Firefox's interface is native AppKit and asks for API 19). Then view-based tables, `NSVisualEffectView`, the CF bridge class symbols and Security.

*Progress (6 Nov 2026):* constraint-based layout and `NSStackView` in AppKit (`3d987fdf45`): constraints from items, anchors and the visual format language, layout guides, intrinsic sizes with their priorities, `fittingSize`, solved by kiwi 1.4.8 (Modified BSD, pinned); `NSEdgeInsets` in Foundation (`187850779e`); performing a selector on a main thread that had not run its loop yet raised (`c692e35c64`). Local test `autolayout` 28/0; gates M2 and M3 pass.

*Progress (9 Nov 2026):* measured again against the four reference apps: imported symbols 85.5 % (74.3 %), AppKit 89.6 % (70.2 %), CoreFoundation 97.8 % (60.3 %), CoreGraphics 100 %, Foundation 89.7 %, selectors 88.5 % (83.5 %). AppKit's methods take and return the API's types: 531 declarations had Cocotron's `float`, `int` and `unsigned` where the API has `CGFloat`, `NSInteger`, `NSUInteger` and named option types, so Swift saw `Float` and `Int32` and binaries for the reference passed doubles where AppKit read floats; callers that passed the address of a narrower variable were fixed with them (`4748eba480`, `dd3915777d`). `NSEvent` with the API's numbering and names, CGFloat deltas, local monitors and CG events (`88ef154c9d`); tags and states as `NSInteger` and controls made in one call (`78f0e89df9`); text measured in lines and the system font's sizes, 13, 11 and 10 (`7929fdd9b4`); `NSColor` and `NSColorSpace` of API 19, CoreGraphics' geometry exported with the reference's rules and color spaces by name (`f2e2c89459`, `1bf753c030`); view-based tables (`2164fd35bb`, `9e64c1274d`); view controllers with children and presentation, `NSSplitViewController` and `NSTabViewController` (`0be9785692`); `NSAppearance`, `NSVisualEffectView`, views cached in bitmaps (`6c251265bb`); font weights and the monospaced system fonts (`5afc0696f0`); the binding names (`fc45d9647f`); `NSWorkspace` opening applications with a configuration and the volume notifications (`30acc8f2ef`). The CF bridge class symbols, as decided on 9 Nov 2026 (option A): dyld looks in Foundation for what binaries import from CoreFoundation and Minkar keeps in Foundation (`8c06f071ed`); CoreFoundation gains `CFAttributedString`, `CFStringTransform`, URL bookmarks and alias files (`b828d839c7`, `059e7bc40e`). Foundation: suite defaults and preferences written as XML property lists (the old text format lost numbers and data), process activities, `NSBackgroundActivityScheduler`, path completion and the mounted volumes (`f232ca582f`, `0e6eeaecc9`, `dd161c6ec8`). Local tests `appkit-graphics-api`, `appkit-controls-api`, `appkit-view-tables`, `appkit-view-controllers`, `appkit-appearance-api`, `cf-api19-additions` and `foundation-api19-more`; gates M2 and M3 pass.

*Progress (9 Nov 2026, second round):* imported symbols 95.2 % (85.5 %), selectors 93.9 % (88.5 %); AppKit, CoreFoundation, CoreGraphics, Foundation, CoreServices, CoreText, QuartzCore and ApplicationServices import nothing missing (100 %). Still absent: Security (54 symbols, next), WebKit (10, its own plan above), and the API 20 and API 22 classes `UTType` and `SMAppService`. dyld finds in AppKit what binaries import from Foundation and Minkar keeps in AppKit (`b57ccbd06e`). Foundation: the constant literal classes the reference platform's compiler lays out in binaries (`NSConstantArray`, `NSConstantDictionary`, the constant numbers and the empty collections), `AppleEvent` descriptors and handlers (`NSAppleEventDescriptor`, `NSAppleEventManager`; `NSAppleScript` says there is no scripting component), home directories by user, HFS type strings, attribute run enumeration (`e03ced3524`, `852e608474`). CoreServices: an AE framework (descriptors, coercions, handlers in the same process), handles, resources, `Gestalt`, `FSRef`, icon references, `MDItem`, item info and default handlers (`5a5488ac29`, `65bb14584c`). CoreGraphics: the default byte order of bitmaps is the components' order in memory, as on the reference platform (RGBA was stored A, B, G, R, so programs reading a bitmap context's pixels saw the channels swapped), opaque formats, and CF type IDs for its types (`4a567351b9`, `8a4e9afeda`). CoreText: lines, runs, paragraph styles, fonts by name and traits, `CTFontRef` as the API's CF type (`c64e917b51`). QuartzCore: layers draw (`display` did nothing), `renderInContext:`, `CAShapeLayer` (`46d7ca1a20`). ApplicationServices: process serial numbers and `UAZoom` (`dbada9a0a8`). AppKit: `NSAnimationContext` (it was unimplemented), `NSTextInputContext` and marked text in `NSTextView`, images drawn by a handler and as CGImages, window rectangles on the screen and full screen, and about 45 more calls of tables, menus, panels and scroll views (`1c8186c1c0`, `4d18786847`). New frameworks: UserNotifications, ServiceManagement, AddressBook, AudioToolbox, OSAKit, Carbon and Quartz (Quick Look) (`3d2be6b070`); where Minkar has no service yet (notification banners, audio output, a contacts database, a script component), they say so in their headers and behave as when it is off. Local tests `coreservices-api19`, `foundation-literals-moved`, `coregraphics-byte-order`, `coretext-api19`, `quartzcore-layers-api`, `appkit-api19-more`, `frameworks-api19-small`, `imageio-api`, `appkit-more-api`; gates M2 and M3 pass. Security (decided on 9 Nov 2026: Minkar's own subset now, the APSL Security sources in `Frameworks/Security` later): authorization for administrators by rule (no authentication dialog yet), keychain passwords through `SecItem` and the older calls (kept in a file readable by the user only, not encrypted yet), keys and certificates on libcrypto, digest and verify transforms, and code signing queries that report code as not signed (`d47c6ef6e3`, test `security-api19`). Imported symbols are then at 99.0 %: what is left is WebKit (10), and `UTType` (API 20) and `SMAppService` (API 22). WebKit's API classes then come in with pages shown as text until WPE WebKit is their engine: `WKWebView` with its configuration, user scripts (kept, not run: there is no JavaScript yet) and delegates, and the older `WebView` (see `webkit-utt-api`); and `UTType` of API 20 on LaunchServices' declarations. The block's targets are met: imported symbols 99.9 % (1414 of 1415; only `SMAppService`, API 22, is missing, outside the planned scope), selectors 97.6 % (what remains is mostly the reference apps' own updater delegates), and AppKit, CoreFoundation, CoreGraphics, Foundation, CoreServices, CoreText, QuartzCore, ApplicationServices and Security at 100 %.

**WebKit (Cocoa coverage, required for Minkar 19.6.0; decided 5 Nov 2026).** WPE WebKit 2.54 (LGPL and BSD, the port made for embedding) built for Minkar as `PLATFORM(WPE)` on the real Darwin services WTF already knows (`OS(DARWIN)`: Mach virtual memory, threads, libdispatch), with a Minkar platform of its WPE Platform API, and Minkar's own `WebKit.framework` on top with the Cocoa API. Its dependencies are fetched at pinned versions with their SHA-256 and built as dynamic libraries (glib, libsoup and the LGPL parts as libraries, not linked statically). The web browser is Firefox (decided 11 Nov 2026; Ladybird dropped its AppKit interface). *11 Nov 2026:* the reference apps grow with native Cocoa apps that cover the system more widely: Firefox and Thunderbird 160 (minimum 10.15, API 19) and Hex Fiend 2.18.1, measured from their published disk images (`fetch-reference-apps.py` reads UDIF images, their HFS+ volume through 7z); iTerm2, Skim and Transmission follow, and a music and a video player once CoreAudio and the media frameworks are there. First measurement: Firefox 77.3 % of its imported symbols and 63.4 % of its selectors, Hex Fiend 91.4 % and 93.5 %; most of what Firefox misses is the media stack (CoreVideo, VideoToolbox, CoreMedia, AudioToolbox, AVFoundation), then AuthenticationServices, printing (`libcups`), Metal, and parts of CoreText, Carbon and Security.
- [ ] W0 — The dependencies in `/usr/lib`, as WPE 2.54.2 requires them (`Source/cmake/OptionsWPE.cmake`; it draws with the Skia it carries, so no cairo): glib (with pcre2 and libffi), libsoup 3 (with nghttp2, libpsl and brotli), glib-networking on OpenSSL, HarfBuzz with ICU, FreeType and fontconfig, libpng, libjpeg, libwebp, woff2, libxslt, libgcrypt (with libgpg-error), libtasn1, libxkbcommon and libepoxy (on Mesa); ICU and libxml2 are Minkar's. Gate: each library's own tests that run without a network. *Partial (6 Nov 2026):* `minkar.extlib.mk` builds a pinned project with Meson, CMake or autotools as dynamic libraries in the SDK and `/usr/lib` (`c3129f586c`); PCRE2 10.45, libffi 3.5.2 and GLib 2.86.1 are in. GLib needed API 19 that was missing: UTType and UTCoreTypes (`f8a4e037d9`), LaunchServices by content type and URL scheme, with URLs that are not files opened by their scheme's application (`a25d499340`, `5767db4ce3`), ApplicationServices bringing in CoreServices (`cec5667897`), `NSUserNotification` (`cb38c88317`), and CFURL/NSURL toll-free bridging both ways (`165dd272db`). Local test `glib-uti` 23/0 (main loop, PCRE2, GObject, GIO, spawning, UTType, LaunchServices, GIO's content types and default applications); `foundation-cf-bridging` 20/0 with URLs. Then GLib's build tools for the build host, with Meson's subprojects pinned too and no downloads of its own (`53ade2e20d`); nghttp2 1.66.0, Brotli 1.1.0, libpsl 0.21.5, libsoup 3.6.5 and glib-networking 2.80.1 (`2aadfd6d11`), which trusts OpenSSL's store while Minkar has no Security framework. Local test `libsoup` 9/0: a loopback server answers a session in HTTP and over TLS (self-signed certificate), the Public Suffix List, the content decoder. Then the rest (6 Nov 2026): ICU's full headers for the external libraries (`f394c90119`); HarfBuzz with ICU, libwebp, WOFF2 and Expat fetched, and libpng, libjpeg-turbo, FreeType and Fontconfig built as dynamic libraries from the copies Onyx2D and CoreText already build statically, Fontconfig configured as CoreText's so both share `/etc/fonts` and the caches (`8cb321eac2`); Libgcrypt and libgpg-error, libtasn1, libxkbcommon with xkeyboard-config, libepoxy, and libxml2 and libxslt as `/usr/lib/libxml2.2.dylib` and `libxslt.1.dylib`, as the reference platform has them (`739fbc4e9a`). Local tests `fonts-images` 9/0 and `crypto-xml` 7/0. Every library W0 lists is in; what remains is the gate itself: each library's own tests, built for Minkar and run in the image. *Gate, partial (7 Nov 2026):* `bmake w0-check W0_HOST=root@address` copies the libraries' own tests (Tests/W0) to a physical machine running Minkar and runs them there as a user without root's rights (`f9d9076530`, `be499de2d1`); in VMs nested in another hypervisor a thread's wake-up costs about a millisecond and GLib's lock tests do not finish, so the gate runs on hardware. On PC 2, 16 libraries pass every test they have that runs without a network: PCRE2, Expat, GLib (274 programs), glib-networking, libsoup (38, the Apache ones too, with an Apache httpd built for Minkar, `6b43706af4`), nghttp2, libpsl, Brotli, HarfBuzz (374, its Python runners in sh), libxml2, libpng, Fontconfig, libxkbcommon (1.13.2, `367f638cd6`), Libgcrypt, libgpg-error and libtasn1; what each leaves out is in its skip.txt, with the reason. Fixed on the way: pthread_cond_wait's busy mutex read twice (`109687faa2`), a closed descriptor in GLib's g_poll (`f20105085b`), getsockopt(SO_ACCEPTCONN) (`b717c9c263`), /etc/hosts read after DNS (`c99393d4b9`), a race in glib-networking 2.80 (now 2.90.1, `d416e917f9`). Then (7 Nov 2026) five more pass on PC 2 (`89905161de`): libxslt (746 cases, in the gate's run, now 17 libraries), libffi (726 programs of its DejaGnu suite, built and checked without DejaGnu), libjpeg-turbo (211), libepoxy (4; its CGL test that draws needs OpenGL, not there yet) and libwebp (its test vectors, as it has no unit tests: every vector decoded to five formats, with and without threads, against MD5 sums). Remaining: FreeType and WOFF2, which have no tests that run without downloads (FreeType's only test needs a font of unclear license); both are used by HarfBuzz's and Fontconfig's tests, which pass.
- [ ] W1 — JavaScriptCore: the `jsc` shell and the JavaScript test suites that do not need a browser, with the JIT. *Progress (7 Nov 2026):* JavaScriptCore from WebKit's repository at WPE 2.54.2's commit, built as its JSCOnly port with the JIT, a private framework in `/usr/lib/webkit` with `jsc` (`3eba0388e3`); it needed CFNotificationCenter (`18735971e8`), `KERN_NOT_FOUND` (`854fb2e231`), `pthread_self_is_exiting_np` (`26838cbada`) and codesign for bundles (`9a6968b48a`). On PC 2: the ES6 suite passes, 605 of 605; the stress suite ran 13,967 of its 20,174 tests before the disk filled, and only two failed (8 runs), on libm's `pow`, a whole ULP off: it is now Arm's (from musl, MIT), within 0.52 ULP (`600343c267`), and they pass. Remaining: the rest of the stress suite and the other suites, on PC 1.
- [x] W2 W2 — WebCore and WebKit's processes (UI, web content, network) on a Minkar WPE platform: frames in shared memory buffers (IOSurface later), input events in. Gate: an HTML page rendered to an image, compared with a reference rendering. *Done (10 Nov 2026):* WPE WebKit 2.54.2 built by the toolchain (clang 21) in `BSD/lib/wpewebkit` (WebKit's sources at JavaScriptCore's commit, its six patches and eleven of its own: the WPE port on Darwin without the Cocoa ports' bundles, IPC over stream sockets framed by message, since Darwin's Unix sockets have no `SOCK_SEQPACKET`, and GLib's content types taken as MIME types); headless WPE platform, Skia on Mesa's EGL, no GStreamer until AU6. Gate passed: `bmake w2-check` (`Tests/W2`) draws `page.html` (block layout, flexbox, grid, borders, alpha, a box added by JavaScript, text) at 800x600 in the VM in 2 s, 0 pixels off the reference made from the page's geometry, 3184 pixels of text ink. Input events are tested with W3's views.
- [x] W3 — `WebKit.framework`, the modern API: `WKWebView` drawing in its AppKit view, `loadHTMLString:baseURL:`, `loadRequest:`, `evaluateJavaScript:completionHandler:`, `WKWebViewConfiguration`, `WKPreferences`, `WKUserContentController` with `WKUserScript` and script message handlers, `WKNavigationDelegate` and `WKUIDelegate`; mouse, keyboard, scrolling, focus, copy and paste. *Done (10 Nov 2026, `e5e516dade`):* `WKWebView` is a WebKitWebView on Minkar's WPE platform (a display with a toplevel per view, frames from shared memory copied to the view at up to 60 a second, the clipboard on the general pasteboard, AppKit's cursors); GLib's main context runs on the main thread's run loops; AppKit's mouse, scroll and key events go to WebKit (virtual key codes to Linux input codes, symbols from AppKit's characters). `WebKit.framework` is built in a stage of its own after BSD. Local tests `webkit-wpe` 9/0 (a page drawn upright through `drawRect:`, JavaScript's values and errors, a user script and a script message, a click, typing, copy and paste through the pasteboard, the scroll wheel, alert, confirm, prompt and `window.open`/`close`) and `webkit-utt-api` 7/0. Found on the way: Foundation's `NSRunLoop` was not built on CFRunLoop (CF observers and sources on the main run loop did not run under AppKit); it is now, as on the reference platform, with `CFFileDescriptor` added to CoreFoundation (`2697cb9c54`, `9d085055d6`, `8756f51435`; image tests 3877 of 3882, the rest failing as before or only under load); `NSNull` described as `<null>` (`d1bd8f9cc9`). *Left:* IOSurfaces instead of copies, input methods and dead keys, drag and drop, printing.
- [x] W4 — The older API on the same engine: `WebView`, `WebFrame`, `WebPreferences`, and the frame load, policy and UI delegates. *Done (10 Nov 2026, `86ed0b5d33`):* a `WebView` shows a `WKWebView` of its own and turns its calls into the older delegates': frame load (with the window object cleared at each page), policy through `WebPolicyDecisionListener`, the UI delegate's synchronous panels, new windows and closing; `WebScriptObject` crosses values through JavaScript (objects kept in a table of the page's); `stringByEvaluatingJavaScriptFromString:`, `searchFor:`, the data source's response and bytes, `WebPreferences` as WebKit's settings. The API's synchronous calls run GLib nested when made from inside WebKit's callbacks (a script from `webView:didFinishLoadForFrame:` works). The text engine is gone. Local tests `webkit-legacy` 7/0, `webkit-utt-api` 7/0, `webkit-wpe` 9/0. *Left:* the DOM API (`DOMDocument` and the rest), frames other than the main one, downloads, and Objective-C objects exposed to scripts.
- [ ] W5 — The reference apps' web views work (their HTML views, user scripts and settings). Local tests for each of W0–W5; WebKit's API tests where they run. *Progress (10 Nov 2026, `ac148eb478`):* what the reference apps' code uses works, reproduced in the local test `webkit-reference-apps` 5/0: Sparkle's release notes (KeepingYouAwake, Platypus, Sloth; page scripts off but user scripts on, `WKContentRuleListStore` blocking external resources, no background) and TextMate's HTML output (an Objective-C object in the page under the WebScripting protocol, through a web process extension and a synchronous channel, with `webView:didClearWindowObject:forFrame:` before the page's scripts; the selection copied through the DOM API, added as a subset). Local tests now cover W0–W5. NSBundle finds a framework's bundle, executable and Info.plist as the reference platform (`f1959c9a49`). For the apps' windows (11 Nov, `e1d404ea77`): binary nibs (NIBArchive, what Interface Builder compiles) load, 165 of 165 compiled by the reference tools, with Auto Layout constraints and runtime attributes; local test `appkit-nib-archive` 12/0. Minkar's own xib compiler (`ibtool` in the toolchain, `3bfb3d775f`): TextMate's 14 xibs compile, and of 165 documents with the nibs the reference tools make of them, 69 load into the same windows, views and menus; host test `ibtool`. The apps built from their Xcode projects (12 Nov): KeepingYouAwake 1.6.8 builds from its sources with the tests repository's `pbxbuild` (the project's targets, local Swift packages, interfaces, string catalogs, its login item helper) and runs on Minkar: its status item shows and switches the system awake (`caffeinate`). What it asked for, added: grids laid out by constraints, `SMAppService` (API 22, ahead of Minkar's API because the app uses it), `-[NSApplication activate]`, system symbols, the power sources' keys; local test `appkit-newer-api` 12/0. Xcode projects built by the toolchain (12 Nov): Swift Build (Apache-2.0, swift-6.4.2-RELEASE, `626166872b`) is in the toolchain and plans and runs the build from the project's PIF, which `xcodebuild -dumpPIF` writes (`431732360c`); KeepingYouAwake's login item helper builds from the unmodified `.xcodeproj` this way (Mach-O, nib, `Assets.car`, Info.plist). *Left:* an `xcodebuild` that builds through Swift Build (signing, local Swift packages in the PIF) to replace `pbxbuild`, AppKit reading asset catalogs (`Assets.car`; apps shipped built have their images only there), Icon Composer icons, the Direct target with Sparkle, Platypus and Sloth, more of Interface Builder's elements (text, split, tab and stack views, segmented controls, steppers), WebKit's API tests.

**Audio and media (AU1–AU10; decided 9 Nov 2026).** No GTK, Qt or GStreamer: WebKit's audio and video go through Minkar's own frameworks. Professional from the start, so the core is not redone later: a real-time path with no extra copies (clients write into shared memory and the audio service mixes in the same cycle, on the kernel's real-time threads, with locked memory and no locks or allocations on the audio path); channels, sample rates (44.1 to 192 kHz) and formats with no fixed values, several devices at once; a clock per device (sample time, host time and rate) for drift correction and aggregate devices; buffers down to 32 frames; and a component registry that loads outside bundles. Decoding with FFmpeg (LGPL 2.1, a dynamic library, without its GPL or non-free parts, cut down to decoders and demuxers) and dav1d (BSD-2) for AV1, pinned with their SHA-256 and run in a separate process. AU1–AU6 are required for Minkar 19.6.0; AU7–AU10 follow in 19.6.x.
- [ ] AU1 — Kernel: IOAudioFamily (APSL) and an HDA driver (the codec handling ported from FreeBSD, BSD-2) for QEMU's `intel-hda`, PC 1, the Mac Pro and HDMI audio. Gate: a sine wave played in QEMU, recorded to WAV and compared sample by sample; then heard on PC 1.
- [ ] AU2 — An audio service (Mach/MIG) that owns the devices and mixes, and `CoreAudio.framework`: `CoreAudioTypes.h`, `AudioObject` properties and listeners, `AudioDeviceCreateIOProcID`/`Start`/`Stop`, host time; volume in Atlas's menu bar. Gate: two apps playing at once, `AudioObject` tests, round-trip latency and a long run counting glitches.
- [ ] AU3 — AudioToolbox: AudioComponent and the default output unit, AudioQueue, AudioConverter (rates through speexdsp, BSD), AudioFile and ExtAudioFile (WAV, AIFF, CAF), system sounds that play. Gate: a command-line player and AudioToolbox's API 19 tests.
- [ ] AU4 — FFmpeg and dav1d in `BSD/lib`. Gate: a test file of each format decoded to its checksum.
- [ ] AU5 — CoreMedia (`CMTime`, `CMSampleBuffer`, format descriptions) and AVFoundation (`AVAsset`, `AVPlayer`, `AVPlayerItem`, `AVPlayerLayer` on Core Animation and IOSurface, `AVAudioPlayer`), and AVKit's `AVPlayerView`. Gate: a reference video played, its frames compared and audio–video sync measured.
- [ ] AU6 — WebKit's media on them: a media player for WPE on Minkar's AVFoundation (`<audio>`, `<video>`) and Web Audio out through CoreAudio. Gate: a page with `<video>` shows the expected frames and plays sound; a Web Audio test.
- [ ] AU7 — USB Audio Class 2.0 driver (most studio interfaces). *19.6.x.*
- [ ] AU8 — CoreMIDI, framework and service. *19.6.x.*
- [ ] AU9 — Hosting Audio Units, and Accelerate (vDSP; BLAS and LAPACK on OpenBLAS, BSD-3). *19.6.x.*
- [ ] AU10 — Aggregate devices and user-space device plug-ins. *19.6.x.* Gates measured on the same physical machine as the reference system: round-trip latency at the smallest buffer, glitches in a long run under load, clock error and drift, the audio service's CPU use.


## Phase 4 — Graphics and WindowServer

Goal: WindowServer draws AppKit windows and receives keyboard and mouse. In the VM the software path is enough; on physical machines the goal is GPU compositing, as the performance requirement demands it.

**Proposed architecture**

- A display backend interface inside WindowServer: `GOP` (software) → `virtio-gpu` (VMs) → accelerated. AppKit and Quartz never see the backend.
- Compositing with Onyx2D and QuartzCore, as today; the port changes the output, not the drawing model.
- Events: kernel HID → WindowServer → each app's event queue, with the same `NSEvent` model.
- App ↔ WindowServer communication over Mach/XPC, not ad hoc sockets.

**Backend decision (2026-10-25)**

The WindowServer itself is rewritten in milestones WS0 to WS6, as decided on 2026-10-31: see the [WindowServer rewrite plan](WindowServer-Rewrite.md). The output, compositor and input choices below are the ones that plan builds on.

- **First output:** `IOGOPFramebuffer.kext` with `IOFramebufferDisplay.m` and software compositing. Enough for Minkar 19.6.0 and the VM; on physical machines only the starting point.
- **Compositor (changed 2026-10-29):** designed for the GPU from the start, with Vulkan as the main backend behind our own interface. pixman stays for bring-up and for VMs without a GPU.
- **VMs:** a virtio-gpu 2D driver as an `IOFramebuffer` subclass; virtio-gpu's 3D modes (virgl, Venus) are optional, to test accelerated paths.
- **Input:** IOHIDFamily, with `ApplePS2Controller` in QEMU and xHCI + HID (`MinkarUSB`) on real hardware. libinput, libevdev and mtdev are no longer built.
- **Keyboard:** libxkbcommon for now; in the long term `UCKeyTranslate` and Text Input Sources (Phase 5).
- **GPU (changed 2026-10-29):** no longer deferred. It is a parallel track from now: a kernel driver for one family, Mesa Vulkan/OpenGL, a GPU compositor and, in the long term, native Metal on the driver. The first family is an open decision (see Performance requirements).

**Work**

- [x] Output backend on IOFramebuffer: WindowServer opens `IOGOPFramebuffer` and draws on it (2026-10-31, `f10f778c78`: a depth is an index; test `windowserver` 6/0).
- [x] WindowServer starts and draws in QEMU (WS0, 2026-10-31): its launchd job is installed disabled and, enabled, starts it and restarts it if it dies (`f56c70bb88`, `9f46a761ac`); a demo AppKit window is drawn and gets a click and a key as `NSEvent`s (`6888034ad2`, `70b62fe8bc`, `0e3d06ec35`, `dc931eeeb0`). Gate `M4-draw` (`cce7da92bc`) passes with 0 of 786432 pixels differing from the reference.
- [x] The new core's skeleton and interface (WS1, 2026-10-31): `WindowServer.defs` (MIG), the client in `SkyLight.framework` and the new server in its `Resources`, next to the current one in `CoreServices/WindowServer/Legacy` (`9e48d1f62a`, `f9028e286a`). Display queries, modes and configurations, transfer tables, capture, the cursor, fades and the window list are served over MIG, and a client's exit releases what it held (`0e4d7293ff`, `8dab4060cc`, `37996817e4`, `41e37b7441`, `030dba37c7`). `skylight-test` 115/0.
- [x] Windows, surfaces and the software compositor (WS2, 2026-11-01): surfaces as Mach memory entries with two buffers and damage, a pixman compositor per display driven by damage and the refresh rate, the cursor drawn last, `wsctl` (`6ca5e31aae`, `1aa140cdab`); AppKit windows drawn through SkyLight (`b957c09fac`). Gate `M4-compose` (`8512fe60ba`) passes with 0 of 786432 pixels differing, three times.
- [x] Input (WS3, 2026-11-01): IOHIDSystem read by the new server and routed to windows, with key repeat, click counts, capture during drags, the key window and server-side title-bar moves (`c3763a06a8`, `03bb7ed773`); `CGEventTapCreate`, `CGEventPost` and `CGGetEventTapList` (`fa7aae5e38`); AppKit through it (`13f99802d5`, `d0c63753c2`). Gate `M4-compose` with a click, a key and a drag (`98237bf14e`); `eventtap-test` 15/0.
- [x] The new core's server behavior (WS4a, 2026-11-01; `996178d949` to `e31dd0ef53`): its own user (`_windowserver`), drag regions, the keymap in SkyLight, symbolic hotkeys, running applications through LaunchServices' session, the front app, the menu bar's place, pointer capture and status items (what MB0 needs), minimized windows and window images (what the Dock needs), window lists, child windows and sheets, cursors and cursor rects, shadows, display capture, `CGEventSource`'s state, transfer tables and fades on the screen, and input on its own queue over a snapshot of the model. Gate `M4-compose` (40 markers); the server's local tests (117 passed); no `SHORTCUT(WS4a)` left.
- [x] The switch (WS4b, 2026-11-02; `6c3b719a32` to `9d510ec6c2`): each app draws its own menus (MB0: its menu bar in its own window, full tracking, key equivalents and Control-F2, context menus and pop-up buttons, status items as windows of their app, the system menu); the minimal `loginwindow` owns the session (Log Out, Restart, Shut Down); the new server is the default, started at boot (`-textconsole` keeps the text console); CoreGraphics and AppKit speak only to it (Quartz Display Services through SkyLight); the legacy server, libinput, libevdev and mtdev are gone. Gate `M4` (54 markers, M4-draw and M4-compose folded into it); full local suite 3192 passed; no `SHORTCUT(WS4b)` left.
- [x] Kernel entitlement validation (ADR 0001, 2026-11-01; `57c6d5734b`, `48ee7a4288`, `24412d4182`, `c1cda1f1ae`, `f77748bbb7`, `8366b8d0f6`): the image build signs the sysroot's executables and writes a static trust cache; the `MinkarCodeIntegrity` policy kext marks signatures `CS_SIGNED`, and platform binaries from the cache, and kills at exec a binary outside it that carries a restricted entitlement. Root no longer opens IOHIDSystem's server connection without the entitlement. Gate M2 checks it (17 markers); local test `codesign` 13/0; full local image suite 2773 passed.
- [ ] Third-party apps' profile entitlements validated. *Now (13 Nov 2026, decided):* with a valid signature, the identity and provisioning-profile entitlements (`com.apple.application-identifier`, `com.apple.developer.*`, beside `com.apple.security.*`) let an app start but give it no privilege: no Minkar service grants access on them; the entitlements that give power stay for the trust cache. Firefox 160 needs it (`application-identifier`, `web-browser.public-key-credential`). *Later, for more security:* check `embedded.provisionprofile` (its CMS signature and certificate chain) and accept only the entitlements the profile grants, as the reference platform does.
- [ ] App sandbox (decided 13 Nov 2026: Minkar's own, in phases). Apps limit themselves with SBPL profiles (`sandbox_init_with_parameters`); Firefox's processes need it. *Phase 1:* `libsystem_sandbox` compiles SBPL (a small Scheme: `define`, `if`, `param`, `string-append`, regex literals, `require-*`, the path, name, address and number filters) to a blob, with an evaluator and a regex engine shared with the kernel. *Phase 2:* the `MinkarSandbox` policy keeps the profile with the process's credential (kept by its children and across exec, not replaceable) and enforces files, the network, sysctls and signals; launchd checks Mach service lookups and registrations through `sandbox_check`. Users and groups come from `directoryd` through libinfo's directory module and a real `xpc_pipe`, so sandboxed processes resolve them without reading `/etc` (password hashes only for root outside a sandbox, as with the files). *Phase 3:* IOKit user clients, file mappings, process information, preferences, and the reference platform's service names in profiles mapped to Minkar's.
- [x] IOHIDSystem event delivery to user space (2026-10-31, `e2df542919`): the event server connection maps the shared `EvGlobals` queue and is told through its Mach port; pointing devices reach it too. Test `hid-events` 9/0. The `NEW_HID` dispatch loop stays off: it feeds the event system queues, which are not built.
- [x] An IOHIDSystem input reader in WindowServer (`kIOHIDServerConnectType`, `EvGlobals`, Mach port), 2026-10-31, `ab71ce75a4`: moves, clicks and keys injected through QMP reach it (test `windowserver` 6/0).
  - [x] PS/2 keyboard: its keys reach WindowServer and an AppKit window (gate `M4-draw` types through it, 2026-10-31).
  - [x] An `ApplePS2Mouse` driver on `ApplePS2MouseDevice` (2026-10-31, `2aa9a1da6e`): moves, three buttons and the wheel (IntelliMouse); test `ps2-mouse` 12/0.
  - [x] `IOKit/hidsystem` headers in the SDK.
  - [x] xHCI + HID (`MinkarUSB`), tested in QEMU and on PC 1.
- [x] Remove `libinput`, `libevdev` and `mtdev` from the tree (with the legacy server, WS4b, `c192d0dc42`).
- [x] virtio-gpu 2D as an `IOFramebuffer` subclass (`MinkarVirtioGPU`, 2026-11-02, `3e73fce129`, `1f58b3fb44`).
- [x] `pbcopy` and `pbpaste` on the pasteboard server, and `screencapture` on WindowServer's capture (moved from Phase 2; 2026-11-03): the pasteboard server `pboard` (`dd06e82755`), AppKit's `NSPasteboard` on it with the API of 10.6 and later (`fec8a72732`), `pbcopy` and `pbpaste` (`a5ced9833e`), `screencapture` to PNG, TIFF or the pasteboard (`c536c7dbe9`). Left for later: promised data across processes, interactive selection, the cursor and JPEG in `screencapture`.
- [x] Multiple displays and resolution changes (2026-11-03; tested in QEMU with two displays, no second monitor on the reference machines): mode changes reach the apps (WS5, `display-reconfigure`); one desktop across the displays, the boot display's first by PCI place (`e5720024f7`); the pointer moves over all of them and never off them, IOHIDSystem getting the displays from the window server (`e5720024f7`); each display captured on its own (`93f2e9249a`); arrangement and mirroring, an unmirrored display back beside the others (`9b6a9a3a2d`). Local test `multi-display` 19/0. Left: marking the device that holds the firmware's framebuffer as the boot display, so a boot GPU that is not first on the bus stays the main display.
- [x] GPU ADR: the family, licenses (only drm-kmod's MIT/BSD parts), a LinuxKPI-style layer for XNU and a KMS prototype on the reference machine (ADR 0002 and G1, 2026-11-02, `a42a1800b1`, `050acdb1ee`).
- [ ] Compositing performance gate on the reference PC (a Minkar 21.0 requirement).

**Exit gate (M4):** in QEMU and on a reference PC, several AppKit apps open windows, are dragged, take keyboard input and redraw without artifacts.

**Status (2026-11-03): M4 met.** In QEMU, `boottest GATE=M4` sees its 54 markers with both the standard and the virtio VGA. On PC 2, with today's tree (`c6d74fc756`) at 3440x1440 and 60 Hz, composited by the Vulkan backend: the demo's two AppKit windows took clicks and keys from the USB keyboard and mouse (through a KVM) and were dragged by their title bars, with no flicker or leftovers seen by the maintainer and none in the screen's capture (frames p95 17.3 ms); a second app launched, came to the front and quit, seen by the first through NSWorkspace. Left for Minkar 21.0: the compositing performance gate.

## Graphics stack: components and sequence

Of the graphical environment, the drawing base exists (Core Graphics with FreeType, and pixman, libpng, libjpeg and libtiff in `BSD/lib`), as do the GOP console and USB input. The compositing server, IOSurface and everything that depends on the GPU are missing. State verified on `devel` (`cce7da92bc`, 2026-10-31): WS0 is done, the current WindowServer draws AppKit windows with input (gate `M4-draw`); WS1 too (`030dba37c7`): the new core's interface and server, tested by `skylight-test`; WS2 (`8512fe60ba`): it composites AppKit windows (gate `M4-compose`); and WS3 (`98237bf14e`): input and event taps.

Components, by state:

- **There:** Core Graphics with FreeType; pixman, libpng, libjpeg and libtiff (`BSD/lib`); the GOP console (`IOGOPFramebuffer`); USB input (xHCI + HID, `MinkarUSB`); the `IOKit/hidsystem` headers; IOHIDSystem event delivery; `ApplePS2Mouse` and the PS/2 keyboard.
- **Partial:** WindowServer (WS0 to WS5 done: the new core is the window server, with IOSurface and virtio-gpu, in QEMU; WS6 to do); QuartzCore and Onyx2D; AppKit (Cocotron; the fixes a first window needed, 2026-10-31).
- **Missing:** the Core Animation compositor; Core Text on HarfBuzz/FreeType; giflib and libwebp; ColorSync on Little CMS; and the GPU track (kernel driver, Mesa, Metal).

Each piece's technical detail is in the graphics stack's technical guide.

Development sequence:

1. IOHIDSystem event delivery and the PS/2 mouse.
2. WindowServer on the GOP framebuffer.
3. virtio-gpu 2D (our own driver, BSD-2; done 2026-11-02).
4. IOSurface (Mach shared memory; done 2026-11-02).
5. A Core Animation compositor designed for the GPU (Vulkan backend; pixman only for bring-up and VMs).
6. HarfBuzz/FreeType for Core Text, giflib/libwebp, Little CMS for ColorSync.
7. A modern AppKit with layer-backed views.

In parallel, the GPU track G1–G5: from the first driver's family and the KMS prototype to Mesa (lavapipe/llvmpipe in VMs) and Metal (darling-metal and Indium, MPL-2.0).

Step 1 was done on 2026-10-31 (`e2df542919`, `2aa9a1da6e`), and step 2's first milestones, WS0 (gate `M4-draw`, `cce7da92bc`) WS1 (the new core's interface, `030dba37c7`) WS2 (its compositor, gate `M4-compose`, `8512fe60ba`) and WS3 (input and event taps, `98237bf14e`); ADR 0001's entitlement check is done (`8366b8d0f6`), WS4a too (the server's behavior, 2026-11-01, `e31dd0ef53`), and WS4b too (the switch, gate `M4`, 2026-11-02, `9d510ec6c2`); and WS5 too (2026-11-02, `9a472063c1`): steps 3 and 4 are done (virtio-gpu 2D, `3e73fce129` and `1f58b3fb44`; IOSurface, `b28eb5f413`). WS6 is next. G1 is done (2026-11-02, ADR 0002, `a42a1800b1` and `050acdb1ee`): on PC 2, `MinkarAMDFramebuffer` takes the FirePro's display from the firmware at the display's native mode (3440x1440 at 50 Hz), scans out its own framebuffer in VRAM, and `wsctl mode` changes the mode through the scaler, with the VBIOS's tables run by the AtomBIOS interpreter. Modes with timings of their own (pixel clock and DisplayPort link set by the driver) come with G2. G2 is under way, and its gate is met on PC 2 (2026-11-02): the DMA engine runs a command buffer and its fence signals, and it copies between surfaces in VRAM (`6be0dcb84c`, `b9ac2e1f36`); a VRAM allocator (`dfee0c87f3`), a user client with `minkar-gputest` (`d405ae35bf`), and the GART with system buffers on both GPUs, the display's too (`34f7974d13`, `2b11571625`). Since then (2026-11-02): IOSurfaces reach the GPU through the GART (`e92fe4b7ff`), interrupts with fence waits that sleep (`7e6621a639`), the window server presents its frames by the GPU's DMA engine (`38e8875af1`), IOSurfaces in VRAM shared across processes (`f13053f237`); and the modes with their own timings over HDMI: the EDID over DDC (`4dcbddbf47`, `de0233a02d`), the mode set through the VBIOS's tables (`8677a3bb9b`), the native size at its fastest refresh rate at start, 60 Hz on PC 2 (`59e0165082`), each refresh rate a mode of its own (`e14a3fc4b6`, `e86385a4f3`), and the display back by itself after its cable is unplugged and plugged in (`6999c389ad`). Left of G2: the GFX ring (the command processor, with AMD's microcode); the GPU's clocks (deferred by the maintainer: the boot clocks are kept); and DisplayPort link training, waiting for a DisplayPort display to test it on. Since then G2 also gives every program a GPU address space of its own (VM contexts 1 to 15, `1910d34651`, `78d6f046a3`): any program may use the GPU there, and what reaches past it stays for administrators or the window server (`84a8e4139b`); a command buffer that never finishes resets the command processor. G3 has started (2026-11-03): the graphics block's start (tiling, render backends, compute units, `ee1c6210c9`) and the RLC with the clear state (`76e14432ee`), on both D500s; and Mesa's RADV runs on it (2026-11-03): our libdrm takes its contexts, submissions, GPU mappings and sync objects to the user client (`3ef2fd2de9`), and Mesa 26.2.6 with four patches of ours (`8d911e7e1e`) finds both GPUs as Vulkan 1.3 devices and runs buffer updates, DMA fills and copies and compute shaders on them (`minkar-vktest`, 17 of 17). Since then Mesa is built in the tree with its tools pinned (`f31988bb0b`), and RADV draws: a triangle through a graphics pipeline with SPIR-V shaders, off the screen, checked pixel by pixel (`minkar-vkdraw`, `7b62c1005e`). And it presents in windows (2026-11-03): `VK_EXT_metal_surface` over our own `CAMetalLayer`, whose drawables are IOSurfaces, as Vulkan apps on the reference system present (`66db104abc`, `197aff60d2`, `80f271c9b8`, `1be99a691e`); the triangle was seen in its window on PC 2's display. That presentation still copies through the CPU and has no vertical sync: drawables in VRAM shown by the GPU compositor without copies, paced by the display, are WS6's. G3 is done (2026-11-03): its gate is met with the triangle presented in a window and Khronos's conformance tests (CTS 1.4.7.1, through Khronos's Vulkan loader, `18629db65a`, `b9eacfef5d`) passing a subset of 1,992 cases of the mustpass list on PC 2 with no failure (1,887 passed, 105 not supported); on the way they found what the driver and libdrm lacked: page-sized buffers, VRAM beyond the CPU's window, room for thousands of buffers and mappings, fence timeouts and sync files (`f96d6be9cb`, `9981d713a5`). OpenGL comes later. G4 has started (2026-11-03): the compositor draws through a backend interface (`4979a11f03`), and `WSVulkanBackend` composes with a compute shader into a frame in VRAM shared with the display's GPU, presented by its DMA engine; on PC 2 its frames match pixman's bit for bit (G4.2, `c32fe22c13`). IOSurfaces an app makes in VRAM are composited in place, without copies (G4.3, `7d973f778a`). And frames are shown by page flips at the display's vertical blank, one per refresh, without tearing (G4.4, `f103270509`, `1601b4934e`). CAMetalLayer's drawables are IOSurfaces in VRAM shown as their window's layer, composited where they are, and Vulkan apps present at the display's rate; the CPU no longer copies frames (G4.5, `9e74fdc214`, `58ab118eed`, `0822411404`). One copy is left, by the GPU inside VRAM, from the swapchain image to the drawable, as Mesa's Metal path has it: 2.65 ms a frame at 3440x1440 as measured in G4.6 with the GPU at its boot clocks (about 0.3 ms expected at its full clocks), kept for now; removing it is pending work for Minkar 21.0's performance gate. G4 is done (2026-11-03): G4.6 measured the window server on both backends (wsctl stats, `bfb9e26e4d`; minkar-vkcopy, `e487b84770`; the figures in [Measurements.md](Measurements.md)): Vulkan apps are composited 14 times cheaper, with a tenth of the server's CPU, at the display's rate; page flips cost one refresh of latency (28 ms from the mouse to the screen). The GPU still runs at the clocks the firmware leaves it (memory at 150 MHz of 1,270), so these figures are a baseline. PC 2's USB mouse and keyboard work, through a KVM switch, since the xHCI driver encodes interrupt intervals for the device's speed (`8f22e24b9a`). Next on the GPU track: the GPU's clocks (si_dpm and its SMC), then the measurements again; then G5. The GPU track reuses steps 4 and 5.

## Phase 5 — Atlas desktop

Goal: a complete graphical session in the Atlas visual language, with login, a global menu bar, Dock, desktop icons and preferences. The order follows what the user sees at boot. Plan for the desktop and its apps: [Atlas.md](Atlas.md).

1. **LoginWindow:** authentication against this phase's user service (Directory Services, below) and session start (the user's `LaunchAgents`).
2. **Menu bar and SystemUIServer:** each app's AppKit draws its own menu bar and menus; status items are windows of their apps; SystemUIServer hosts the Menu Extras. Plan: [Atlas.md](Atlas.md), chapter 3 (MB0 in WS4b, MB1 to MB4 here).
3. **Basic Menu Extras:** volume, displays, network, clock, notifications. Each one a loadable bundle, not built into SystemUIServer. Plan: [Atlas.md](Atlas.md), chapter 3 (MB1).
4. **Dock:** launcher, open apps, minimizing and wallpaper, on LaunchServices. Plan: [Atlas.md](Atlas.md), chapter 4 (D0–D4).
5. **Files:** a new pure Cocoa app. Browsing with LaunchServices, mounting through DiskArbitration, desktop icons, Trash and "Open With". Plan: [Atlas.md](Atlas.md), chapter 4 (K0–K4).
6. **System Preferences:** host app + `.prefPane` bundle loader (`NSPreferencePane`). First panes: Displays, Keyboard, Users, Network, Dock, Date & Time. *Partial (4 Nov 2026):* the app, `PreferencePanes.framework` and five panes, Displays, Dock, Date & Time, Keyboard and Network (`948e15445f`, `f5f26fc193`, `7c5f6cda8b`; local test `system-preferences` 12/0); Users waits for Directory Services.
7. **AppKit:** complete the missing controls and give the whole system the Atlas visual language (our own design, no third-party artwork), prioritized by what Files and System Preferences use. *4 Nov 2026:* a save panel (`1e4869b895`); the system font, Inter, is also the text editor's until a fixed-pitch font is chosen (to review). *5 Nov 2026:* the fixed-pitch font is Agave (SIL OFL 1.1), fetched at a pinned version and installed unchanged; `userFixedPitchFont` follows `NSFixedPitchFont` and Text Editor shows plain text in it; fonts whose family has spaces are found by PostScript name (`642e6a1cda`), and a font's covered characters lost the last of each run, which text then drew from another font (`5e51bdc6ba`). Files has a test icon, not the final one (`2194ff5695`); images drawn at less than half their size are averaged, not sampled (`f474cc75a0`). Local test `fixed-pitch` 14/0. *11 Nov 2026:* fonts that applications ask for by name and Minkar does not have (the reference platform's) are drawn in free faces installed under their own names, which take the same room: Liberation Sans, Serif and Mono (OFL 1.1), DejaVu Sans and Sans Mono (Bitstream Vera), Gelasio (OFL 1.1) and TeX Gyre Pagella (GUST), fetched at pinned versions; a table in AppKit leads each name there in the style asked for, unknown names go by kind (fixed-pitch to Agave, serif to Liberation Serif, the rest to Inter), and `fontName` is the face drawn (approved on 11 Nov 2026 as the rule for any font an application needs). `isFixedPitch` answers for fixed-pitch faces; a font asked for with size 0 has the default size. Local tests `appkit-font-substitutes` and `appkit-font-size-zero`.

Across all of it: these are ordinary Cocoa apps or bundles, so they double as a continuous AppKit test.

Commands that come with this phase (moved from Phase 2):

- [ ] `open` and `lsappinfo`, on LaunchServices. *Partial (4 Nov 2026):* `open` with `-a`, `-b`, `-e`, `-t`, `-R` and `-n` (`906f12eae4`); `lsappinfo` is missing. With it came the text editor, Text Editor (`b8b8906ab9`).
- [ ] `mdfind`, `mdls` and `mdutil`, with a metadata index for Files' search.
- [ ] Directory Services–style user management, for LoginWindow and the Users pane (moved from Phase 2 on 2026-10-31).
- [ ] `say`, with an audio driver and speech synthesis.

**Exit gate (M5):** from boot, a graphical session starts, an app opens from the Dock, Files browses, and the resolution changes from Preferences, with the Atlas visual language K1 needs ([Atlas.md](Atlas.md), chapter 4).

## Phase 6 — Applications and distribution

Goal: install software by dragging the app from a disk image to `/Applications`, and have the basic everyday apps. This phase is the first real use of the system and the best source of bugs.

**DMG**

1. Our own `DiskImages.framework` (MIT/BSD): read the UDIF format with its usual compressions, then write it.
2. Mount through DiskArbitration at `/Volumes/<label>`, using Phase 1's HFS+ driver.
3. A minimal `hdiutil` (`attach`, `detach`, `create`) on the framework, and double-click in Files.
4. HFS+ compression (decmpfs) in the hfs kext: the reference platform stores many apps' and the system's files compressed, and Minkar's hfs (Darwin 20's hfs-556.60.1, adapted to Darwin 19's xnu) is built without it, so those files read as empty. Gate: an app copied from a reference-platform DMG runs.
5. The rest of hfs's adaptation (found on 2026-11-10, see `Kernel/_PROVENANCE`): the loop that `hfs_vfsutils.c` breaks out of by spotting duplicates (a hack by its own comment) found and fixed, and `panic_on_assert` taken from the kernel's configuration again instead of always on.

**Applications**

The everyday apps are planned in [Atlas.md](Atlas.md), chapter 5: an app foundation (A0), then Terminal, Text Editor, System Monitor, Logs, Screenshot, Image Viewer and Document Viewer, Disks, System Information, Calculator and Archive Manager (A1–A10), required for Minkar 19.6.0; Fonts, Passwords and Keys, Color Picker, Script Editor, Help and Characters in 19.6.x; the browser (Firefox) and an IDE in the long term.

A risk to flag: many current Cocoa apps use SwiftUI, which is not on the roadmap. It is better tracked as an open decision than found out late.

Commands that come with this phase (moved from Phase 2):

- [ ] `osascript`, with an Open Scripting Architecture for the apps.
- [ ] `opendiff`, with a file comparison app.
- [ ] `tmutil`, with backups.
- [ ] git's Perl, Python and Tcl commands (`git send-email`, `gitk`…), with those languages (moved from Phase 2 on 2026-10-31). *Partial (11 Nov 2026):* the languages are in the image, as the reference system ships them (the maintainer's choice, with Ruby): Ruby 3.3.9 (`9931a36e28`), Python 3.13.18 (`e6f45dd6a6`) and Perl 5.40.5 (`392f302d94`), each fetched pinned and cross-built for Minkar with no compiler in the image. Local tests `bsd-ruby` 14/0, `bsd-python` 12/0, `bsd-perl` 14/0. Remaining: Tcl/Tk, then git's commands that use them.

Ahead of time: `hdiutil attach` already mounts images in `/Volumes` through DiskArbitration (2026-10-30); reading the DMGs' UDIF format is missing.

**Exit gate (M6):** a DMG is downloaded, mounted, the app copied to `/Applications` and run; and the Atlas desktop is complete for daily use, as section 6 of [Atlas.md](Atlas.md) sets it: MB2 and MB3, D2 and D3, K2 and K3, and the essential apps A0–A10, used daily on the development machine itself; WebKit through W5, the reference apps' web views working; and audio and media through AU6, sound in the system and `<video>` in WebKit. MB4, D4, K4, the other apps and professional audio (AU7–AU10) follow in 19.6.x; the web browser is not required.

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

M0, M1 and M2 are met (M1 on 2026-11-01, with PC 2), and Phase 2 is closed (2026-10-31). Active work is in Phases 3 and 4, which must close before Atlas. The GPU track starts in parallel and feeds Minkar 21.0's performance requirement: equal to or better than the reference system on the same machine. Phase 7 comes after Minkar 19.6.0.

- [x] M0 — Build, toolchain, CI
- [x] M1 — Darwin boot
- [x] M2 — Base userland
- [x] M3 — Frameworks and Swift (met 5 Nov 2026: gate M3)
- [ ] M4 — Graphics and WindowServer (WS0 to WS3 done)
- [ ] M5 — Atlas desktop
- [ ] M6 — Apps and distribution (Minkar 19.6.0)
- [ ] Minkar 21.0 — API 21 complete and performance
- [ ] M7 — Binary compatibility, arm64

| Gate | Phase | State on 2026-10-31 |
| --- | --- | --- |
| M0 | Build, toolchain, CI | Met (CI green since `05fa75cb65`) |
| M1 | Darwin boot | Met in QEMU, on PC 1 (SATA, NVMe, SMP) and on PC 2 (NVMe, 24 threads; 2026-11-01) |
| M2 | Base userland | Met (`boottest GATE=M2`, 17 markers since 2026-11-01, with the code integrity check); Phase 2 closed on 2026-10-31 |
| M3 | Frameworks and Swift | Met (5 Nov 2026): the Swift toolchain, runtime, overlays, Combine and XCTest; gate M3 passes with all seven tools (swift-argument-parser; Rainbow and Files; Yams; xcbeautify; mogenerator; tag; SwiftFormat) |
| M4 | Graphics and WindowServer | Met in QEMU (`boottest GATE=M4`, 54 markers, 2026-11-02, `9d510ec6c2`): WS0 to WS4b done (the new core is the window server, the menus in each app with MB0, the minimal `loginwindow`; the legacy server removed). WS5 done too (IOSurface and virtio-gpu, 2026-11-02, `9a472063c1`; M4 passes on virtio-vga and on the standard VGA). Phase 4 goes on with the GPU (WS6) and on physical hardware |
| M5 | Atlas desktop | Not started |
| M6 (Minkar 19.6.0) | Apps and distribution, the desktop complete for daily use (MB2–MB3, D2–D3, K2–K3, apps A0–A10), WebKit (W0–W5), and audio and media (AU1–AU6) | Not started; `hdiutil attach` already mounts |
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
- [ ] **PC 1 (Phase 1):** boot without `pci_keep_console`, IPv6 on the network (RDNSS, DNS over IPv6) and ACPI S3 sleep; the machine with the RX 6600 as a further test machine. (The second PC of gate M1 is PC 2, 2026-11-01.)
- [x] **Userland (Phase 2):** closed on 2026-10-31 (exFAT reading and writing, `launchctl kickstart`, dyld paths, notifyd and IPv6 tested; the rest went to Phases 3, 5 and 6).
- [x] **Phase 4:** IOHIDSystem event delivery in the kernel (`e2df542919`), the `ApplePS2Mouse` driver (`2aa9a1da6e`) and WindowServer's input reader (`ab71ce75a4`), 2026-10-31; tests `hid-events` 9/0, `ps2-mouse` 12/0, `windowserver` 6/0, the console tests and M2 pass (full suite 2991 passed after (a)).
- [x] **Phase 4:** install WindowServer's job and get a first boot that draws on `IOGOPFramebuffer` (WS0, 2026-10-31): the job is installed disabled (`f56c70bb88`); enabled, WindowServer draws a demo AppKit window that gets a click and a key; gate `M4-draw` (`cce7da92bc`).
- [ ] **Phase 3:** the Foundation classes the symbol report misses most (`NSURLSession` done 2026-10-31, `00617d5f6c`, with cookies; its cache and metrics left; `NSXPCConnection`, `NSXPCListener` and `NSXPCInterface` done 2026-10-31, `82c5f3e8f8`, test `foundation-xpc-connection` 20/0, full suite 2977/0, M2 PASS; XPC services inside app bundles: the app starts them on demand as its children, without launchd, test `foundation-xpc-service` 9/0), and what came from Phase 2 (libSystem, noisy stubs, `logd`).
- [ ] **Phase 3:** the Swift toolchain prototypes.

What this plan adds, and its state:

- [x] **Versions (2026-10-31):** `PROD_VERSION = 19.6.0` for `ProductUserVisibleVersion`, `PROD_COMPAT_VERSION = 10.15` for `ProductVersion`, and `sw_vers` showing "Minkar 19.6.0 (kernel Darwin 19.6.0)", with its manual page (`d2b559759f`). Tested in QEMU: `@available(10.15)` true and `(11.0)` false, `operatingSystemVersion` 10.15.0, `uname -r` 19.6.0. `libsystem_info` and the pthread kext keep their own versions. In the same work: `launch_activate_socket` (`5051c781b4`), `os_log` and `os_activity` (`0709544df7`), `_availability_version_check` (`c0923c1bc7`) and `NSProcessInfo` (`fb6e25b877`).
- [x] **This roadmap up to date (2026-10-31):** rewritten from the plan, with Atlas and Files, the API 19/20/21 → Minkar 19.6.0/20.0/21.0 scheme, the GPU track and the performance requirement.
- [ ] **Version compatibility:** a `SystemVersionCompat.plist`-style mechanism and `SYSTEM_VERSION_COMPAT=1` for binaries linked against an older SDK.
- [x] **Decide the first GPU driver's family** and write the ADR: GCN 1.0, then RDNA2, then Gen9 ([ADR 0002](ADR/0002-GPU-First-Family.md), `69fed7cb81`). A display driver is also what turns the display off, which the GOP framebuffer cannot do: done for GCN 1.0 by G1; PC 1's RX 6600 is next.
- [ ] **Measurement:** measure the reference system on original hardware (PC 2) and keep the values out of the public repository, for Minkar 21.0's performance gates.
- [ ] **NVMe performance in QEMU:** reads through the block device run at 1 MB/s.

## Sources

- [The minkaros/minkar repository](https://github.com/minkaros/minkar), branch `devel`, verified at `c7a066b196` (2026-11-01).
- [Hardware.md](Hardware.md): reference machines.
- [Commands.md](Commands.md): Minkar's commands against the reference list.
- [Userland.md](Userland.md): the history of ported commands (exFAT, `notifyutil`, `launchctl kickstart`).

The state comes from the repository; the architecture decisions, performance requirements and graphics stack sequence are this plan's proposals, and the ones that are confirmed are recorded here.
