# Minkar Roadmap

Last verified against the tree: 2026-10-26 (`devel`, gate M2 passing).

## Summary

The goal of Minkar 1.0 is a system on Darwin 19.6.0 that builds and runs
Cocoa apps: first against the macOS 10.15 API, then in stages up to
macOS 12, with the Magma desktop in a Monterey-like style. The critical path
goes strictly bottom-up:

    Darwin boot → launchd → userland → frameworks → WindowServer → Magma → apps

Full Objective-C and Swift, DMG support, binary compatibility and arm64 run in
parallel or are deferred so they do not block that path.

The plan has 8 phases (0–7). Each has deliverables and an exit gate that can
be checked automatically. No phase is closed while the one below it fails its
gate.

### Principles

1. **Verifiable gates, bottom-up.** Each phase ends with an automated test
   that boots the image in QEMU. No green test, no closed phase.
2. **The contract is the public macOS API, in stages.** 10.15 first, then 11
   and 12. Every symbol is classified as implemented, stub or missing, and
   measured.
3. **Noisy stubs.** Every stub logs its first call to the system log, so real
   apps tell us what is missing, in the order they use it.
4. **Reuse before writing.** Apple open source code first (xnu, dyld,
   launchd, hfs), then swift-corelibs, then FreeBSD userland. New code only
   fills the gaps.
5. **Clean licenses.** New code is MIT or BSD-2-Clause, nothing GPL in base
   components (DMG, for example), and a clean-room rule: nobody who has seen
   Apple proprietary code contributes code.
6. **Reference apps as acceptance tests.** A fixed set of open source macOS
   apps is built at every milestone; the symbols they import set the priority
   of framework work.

### How to read this document

- `[x]` means done **with evidence that it works**: a `boottest` marker,
  command output, or a commit that implements and tests it.
- Code that is present but untested stays `[ ]`, marked *Partial*.
- Status lines say where the evidence is.

## Version policy

Minkar separates three layers that ship together in macOS. The kernel stays at
Darwin 19.6.0; the API level and the look move on their own.

| Layer | Decision | Note |
| --- | --- | --- |
| Kernel and BSD base | Darwin 19.6.0 (Catalina 10.15.6) | Changed only when a newer API needs a kernel interface |
| API level | 10.15 first, then 11 and 12 in stages | Each stage driven by the imports of the reference apps |
| Look | Monterey style from Phase 5 | Our own design; no Apple icons or artwork |

- **Kernel-dependent APIs.** Some newer APIs expect interfaces XNU 19.x lacks
  (modern libpthread and libdispatch use newer workqueue and kevent
  interfaces). Each case gets a narrow kernel patch or a user-space
  implementation, and the decision is documented.
- **Reported version (decided 2026-10-25).** `ProductVersion` in
  `SystemVersion.plist` reports the last closed API stage; `uname -r` stays at
  19.6. `@available`, `sw_vers`, `NSProcessInfo` and LaunchServices read
  `ProductVersion`, and libSystem (`_availability_version_check`) takes it
  from there.
  - It starts at 10.15 and moves to 11.0 and 12.0 only when each stage closes.
  - Stage gate: a script checks that no API on the stage's list is a stub,
    using the Phase 0 symbol inventory and the noisy-stub log.
  - Compatibility: a `SystemVersionCompat.plist` / `SYSTEM_VERSION_COMPAT=1`
    mechanism, as Apple did in Big Sur, so binaries linked against an older
    SDK see the old version.
  - Rejected: raising `kern.osrelease` to 21.x (it would lie about the kernel
    ABI), and reporting 10.15 forever (it blocks apps with
    `LSMinimumSystemVersion` 11 or 12).
- **Software that derives the macOS version from Darwin** (19 → 10.15) is
  tracked in the Phase 7 compatibility matrix. Example: a native clang with
  the `darwin19.6.0` triple assumes a 10.15 deployment target; set
  `MACOSX_DEPLOYMENT_TARGET`.

## Baseline

Minkar runs on Darwin with its own EFI loader, and the whole system builds as
Mach-O. The biggest gap is between the kernel and the desktop.

| Component | State | What is missing | Phase |
| --- | --- | --- | --- |
| EFI loader + Darwin kernel | Boots in QEMU (gates M0, M1) | Reference PC, virtio drivers | 1 |
| System build (bmake, `/usr/share/mk`, frameworks) | Full `world` builds; CI builds and boots it | Per-layer targets, toolchain manifest | 0 |
| libSystem | Partial | Many functions missing or link-only stubs | 2 |
| launchd / launchctl | launchd-842 as PID 1 (gate M2) | `launchctl kickstart` | 2 |
| BSD userland + `open`, `plutil`, `defaults` | Mostly built | `zsh`, `make`, `git`, `open` | 2 |
| CoreFoundation | Partial | CF-Lite up to the macOS 12 APIs | 3 |
| Foundation, CoreText, CoreData | Partial | Audit against 10.15, then 11 and 12 | 3 |
| Swift 6 | None in the tree | Compiler, stdlib, overlays | 3 |
| WindowServer + Quartz (CGDirectDisplay) | Builds on XNU with an IOFramebuffer backend; never run | Run it, input from IOHIDSystem | 4 |
| SystemUIServer / Menu Extras | Partial, not built | Menu Extras and status items | 5 |
| Dock | Rudimentary, not built | Almost everything | 5 |
| Filer | Does not exist | New pure Cocoa app | 5 |
| System Preferences | Does not exist | App + preference pane loader | 5 |
| AppKit | Partial | Missing pieces and Monterey look | 5 |
| DMG | Does not exist | Non-GPL read/write, mount in `/Volumes` | 6 |
| Terminal, TextEdit, IDE, browser | Do not exist | Find or write | 6 |
| Binary compatibility, arm64 | Future | Everything | 7 |

## Phase 0 — Build, toolchain and CI

Goal: any contributor gets a bootable image from scratch with one command,
and every PR is tested by booting it in QEMU.

- [x] Pin the toolchain (`Default.xctoolchain`): clang, Mach-O linker and
  cctools versions in a manifest; reproducible toolchain build before the
  sysroot. Done 2026-10-27: `tools/toolchain/MANIFEST` pins each
  component's upstream version (clang 17.0.6 from `swift-6.0-RELEASE`,
  ld64-609, GNU make 4.4.1...), the git tree of its sources and the
  version line its tool prints; `tools/toolchain/manifest.sh check`
  verifies them and that one input compiles, links and archives to the
  same bytes twice, in CI after every build. The build exports
  `SOURCE_DATE_EPOCH`: ld64 carried the time it was built.
- [x] Per-layer bmake targets: `toolchain`, `kernel`, `libsystem`,
  `userland`, `frameworks`, `sysapps`, each incremental with its own
  check. Done 2026-10-27: `bmake <layer>` builds the layer's stages,
  `bmake <layer>-check` checks what it installed without booting
  (`tools/check/sysroot-check.py`: every library a binary loads is in
  the sysroot, the property lists parse, kexts and the kernelcache are
  well formed), `layers-check` runs them all and `image-check` boots the
  image (a disk image, `image`, instead of an ISO) to gate M2. The first
  run found every framework's `Info.plist` empty but four (fixed).
  Third-party frameworks (CFNetwork, Security, OpenDirectory) moved to
  Phase 3.
- [x] CI per PR: full build + boot in QEMU (q35 + OVMF) on the serial
  console, waiting for boot markers.
  `.github/workflows/build-boottest.yml` runs on every push and pull
  request to `main` and `devel`: toolchain (cached), system build
  (`tools/ci/build-stages.sh`), image and `boottest GATE=M2`. First green
  run on `devel`: `05fa75cb65` (2026-10-26).
- [x] Symbol inventory: a script that lists each dylib's exports
  (`nm -gU`), matches them against the reference apps' imports and publishes
  a per-framework coverage report. *Partial:* `tools/symbols/` (tested):
  the inventory follows re-exports and flags install names dyld would not
  find; the reference apps are four Objective-C apps for macOS 10.12-10.13
  (`reference-apps.tsv`), 67.3% of whose system imports Minkar has. The CI
  publishes the report (run summary and `symbol-coverage` artifact).
- [x] Build documentation (supported host, dependencies, expected times,
  common errors). [BUILDING.md](BUILDING.md) has the host and every package
  (Open CF-lite included), measured stage times and 17 common errors; the
  CI's green runs on a clean Ubuntu prove the package list complete.

- [x] **Exit gate (M0):** clean build from scratch in CI, and the image
  reaches the kernel's first message in QEMU. Met in CI (`05fa75cb65`: the
  system builds on a clean runner and the image passes gate M2, which
  includes M0's marker).

## Phase 1 — Darwin boot

Goal: XNU boots on a generic x86-64 PC up to PID 1 with the root disk
mounted.

- [x] EFI loader: loads the prelinked kernel, passes the GOP framebuffer, the
  memory map and the ACPI tables (`Image/patches/`, boot log: `ACPI RSDP`,
  `Framebuffer: 1024x768x32`).
- [x] XNU platform for non-Apple hardware: generic ACPI platform expert,
  APIC, TSC/HPET timers, no SMC (boot log: `[ACPI PE]`,
  `AppleAPIC::start()`, ACPICA).
- [x] Minimal kexts, starting with what QEMU exposes:
  - [x] Storage: AHCI with MSI (the AHCI driver logs that it uses an MSI
    interrupt; `556c20aad1`).
  - [x] Storage: virtio-blk (`MinkarVirtio`, `43c4efe09d`: every sector
    read and writes reaching the disk, tested in QEMU). NVMe later.
  - [x] Input: USB xHCI + HID (`MinkarXHCIPort`, `IOUSBFamily`, `MinkarUSB`,
    `863a628989`): a USB keyboard's keys reach the console login and a
    tablet's positions and buttons are exact, tested in QEMU. Delivering
    events to WindowServer is Phase 4's IOHIDSystem work.
  - [x] Network: e1000 and e1000e (`MinkarE1000`), MSI on the e1000e
    (`MinkarE1000: Intel 10d3, MSI`).
  - [x] Network: virtio-net (`MinkarVirtio`: ping, ssh and 1 MB round
    trips through it in QEMU).
  - [x] Display: GOP framebuffer as console device
    (`IOGOPFramebuffer: Kernel graphics console initialized`; checked in the
    log only, not visually).
- [x] Root file system: HFS+ (`hfs: mounted Minkar`). APFS and ZFS later.
- [x] Serial console for the `boottest` gates.
- [x] Readable panic logs for debugging in CI. The kernel in the image
  has no symbol table, so even with `keepsyms=1` a panic backtrace is only
  addresses; `tools/panic-report.py` names every frame from the build's
  unstripped kernel (and kexts), checking the kernel UUID and the slide.
  `boottest` reads the console on to the end of the panic report and
  prints it with function names, so a panic in CI shows where it happened.
  Tested with a real panic (`reboot_np(RB_PANIC)` as root) in QEMU.
- [x] Boots under QEMU with KVM as well as TCG (2026-10-27): the kernel
  read two MSRs KVM does not have (core/thread count, flex ratio) before
  it could handle a fault; under a hypervisor it now uses CPUID and the
  PIT. The test suite's three VMs run under KVM here (the xHCI driver
  now sleeps until its interrupt instead of polling every millisecond):
  the full suite takes about 9 minutes instead of 32. *Partial:* on the
  CI runners the kernel does not boot under KVM yet, so `boottest` uses
  TCG unless KVM is asked for.
- [ ] Reference hardware list: one QEMU VM and two documented physical PCs,
  each with what works, what does not yet and how it was fixed, in
  [Hardware.md](Hardware.md). *In progress:* PC 1 (Gigabyte H270N-WIFI,
  i7-7700) boots to login from SATA with its native display, USB keyboard
  through a KVM switch, the I219-V network and a usable console terminal;
  still open there: the I211, NVMe. PC 2 is not chosen yet.

- [ ] **Exit gate (M1):** in QEMU and on a reference PC, the kernel mounts
  the HFS+ root and runs `/sbin/launchd` (or a temporary init) with a shell
  on the console. *Met in QEMU:* `boottest GATE=M1` with the temporary init
  `BSD/sbin/minkarinit` (BSD-2-Clause). *Met on PC 1* (2026-10-27): it
  boots from SATA with launchd to a console login. A second PC remains.

## Phase 2 — Base userland

Goal: a complete multi-user Unix system on Darwin, managed by launchd, with
the services Magma and the apps will need.

**libSystem and dyld**

- [ ] Complete libc, libm, pthread, libdispatch, libxpc, notify and os_log,
  guided by the Phase 0 coverage report. *Partial:* enough for M2; the
  coverage report exists (`tools/symbols`): the reference apps find 98.1%
  of their libSystem imports. The SDK's `math.h` was a stand-in (isnan
  and the other classification macros as double-only functions, its own
  `bool`, half the C99 functions undeclared); since 2026-10-26 it is
  macOS-compatible and declares everything `libsystem_m` exports.
  `vis(3)` in libc predates `VIS_SHELL`, `VIS_META` and the other newer
  flags.
- [ ] Turn silent stubs into noisy stubs (log on first call).
- [ ] Real dyld: optional shared cache at first; `@rpath`, `@loader_path`
  and re-exports right from day one. *Partial:* `Libraries/dyld` is Apple's
  dyld; `@rpath` and `@loader_path` are untested.

**launchd**

- [x] Port launchd to Darwin: launchd-842 runs as PID 1 (`91b426f401`, boot
  log: `launchd[1] has started up`).
- [ ] `launchctl` with `load`, `bootstrap`, `list` and `kickstart`.
  *Partial:* `load`, `list` and the old-style `bootstrap` exist
  (`BSD/bin/launchctl/launchctl.c`); `kickstart` does not.
- [x] XML property lists, so `LaunchDaemons` and `LaunchAgents` work as they
  come from macOS: every job in `/System/Library/LaunchDaemons` is XML, and
  gate M2 logs in through `sshd` started from `com.openssh.sshd.plist`.

**BSD userland, in batches**

The generic Unix commands come from FreeBSD `main`, with our own macOS
layer where macOS behaves differently; Darwin's own components stay. Each
command, where it comes from and what was fixed while porting it, is in
[Userland.md](Userland.md); [Commands.md](Commands.md) counts them against
macOS's (285 of 354 on 2026-10-29).

- [x] Boot: `sh`, `ls`, `cp`, `mv`, `rm`, `mount`, `ifconfig`, `ps`, `kill`.
- [x] Replace the commands from Apple's command sets with FreeBSD main's
  and the macOS layer: none is left (2026-10-27). Missing in `grep`:
  macOS's `-Z`/`-J`.
- [ ] Everyday tools missing from the image. *Partial:* ported in batches
  since 2026-10-26, with locales, time zones, manual pages and libiconv.
  `passwd` since 2026-10-28. Missing: `nslookup`/`dig` and the rest of
  Commands.md's BSD tools.
- [ ] Power user: `zsh`, `tar`, `ssh`, `curl`, `make`, `git`. *Partial:* all
  six in the image (`git` 2.56.0 since 2026-10-28), without git's Perl,
  Python and Tcl commands.
- [ ] Apple tools: `open`, `plutil`, `defaults`, `sw_vers`, `ditto`
  (`hdiutil` comes with Phase 6). *Partial:* all but `open`, which needs
  LaunchServices; `chmod` has macOS's ACL syntax, and the kernel applies
  allow entries as well as deny ones since 2026-10-28.

**Support daemons** (needed by Phases 5 and 6)

- [ ] `notifyd`. *Partial:* installed with a launchd job; no gate checks it.
- [ ] `logd` (missing; `syslogd` and `aslmanager` have launchd jobs
  instead).
- [ ] Directory Services–style user management (missing).
- [ ] DiskArbitration, mounting volumes in `/Volumes` (missing).

- [x] **Exit gate (M2):** launchd as PID 1, network by DHCP, console login
  and SSH access. `boottest GATE=M2` sees its 13 markers (about 75 s under
  QEMU without KVM):
  - [x] launchd-842 starts as PID 1.
  - [x] root logs in on the console with a password, through PAM
    (`Welcome to Minkar.`).
  - [x] `dhcpclient` gets 10.0.2.15 for `en0` (`ipconfig getifaddr en0`).
  - [x] root logs in over SSH from the host with a key.
  - [x] configd has the DNS configuration (`scutil --dns`).
  - [x] The host's name resolves through mDNSResponder and `getaddrinfo`
    (`dscacheutil -q host`).

Components behind M2:

- [x] Our own IONetworkingFamily (BSD-2-Clause) with the macOS API.
- [x] `MinkarE1000` driver for Intel e1000 and e1000e NICs.
- [x] Our own `ifconfig`, `dhcpclient` and `ipconfig`.
- [x] OpenSSH 10.6p1 without OpenSSL, with PAM (`371b4db57a`).
- [x] MSI for disk and network; fixed the AHCI race with MSI that made
  launchd fail (`556c20aad1`: gate M2 passed three runs out of three).
- [x] `getaddrinfo`, `id` and `dscacheutil`.
- [ ] IPv6: link-local, SLAAC and RDNSS. *Partial:* tested by hand in QEMU
  (`ce72ffdae3`, `6c014daca8`); no gate checks it.
- [ ] DNS over IPv4 and IPv6 with `libresolv` and mDNSResponder. *Partial:*
  the gate checks IPv4 only; IPv6 was tested by hand.

## Phase 3 — Base frameworks and Swift

Goal: a command-line app in Objective-C or Swift written for macOS 10.15
builds and runs unchanged; then the API moves up in stages to macOS 12. This
phase can start in parallel with the end of Phase 2.

**Architecture decisions**

- [x] **Objective-C runtime: objc4.** Already in the tree:
  `Libraries/Makefile` builds `objc4` before `Libsystem`, and `Minkar.sdk`
  takes its headers. It is Apple's ABI throughout the system, which Phase 7
  needs. Still to confirm: `__objc_classlist` and `__objc_imageinfo` in a
  built binary.
- [x] **CoreFoundation: Apple's CF-Lite (decided 2026-10-25).** The current
  `Frameworks/CoreFoundation`, with its own toll-free bridging
  (`CFBridging.c`, `CFRuntime.c`) and CF ↔ XPC bridge. It is not replaced by
  Swift's CF. Public CF-Lite predates macOS 10.12, so the CF APIs from 10.12
  to 12 are added by hand, using swift-corelibs CF (Apache 2.0) as a
  reference. The old Cocotron-based
  `Frameworks/CoreFoundation.old` was removed (2026-10-26).
- [x] **Foundation for Swift: Objective-C Foundation plus the
  `libswiftFoundation` overlay (decided 2026-10-25).** The macOS 12 model.
  Base: the last open Darwin overlay (Apache 2.0) in `swiftlang/swift`'s
  history, updated to Swift 6.0. swift-foundation is a reference for new
  APIs only.
- [x] **Swift runtime: Swift 6.0 in `/usr/lib/swift` (decided
  2026-10-25).** Same version as the LLVM in the tree (Apple's fork,
  `swift-6.0-release`). Starting point: no Swift compiler, stdlib or overlays
  in the tree; only the Dispatch overlay (`libdispatch/src/swift`) and the
  Swift annotations in the CF headers.
- [x] **Combine and SwiftUI (decided 2026-10-25).** Both are macOS 10.15
  APIs. Combine enters the 10.15 stage on OpenCombine (MIT), renamed as the
  `Combine` module and installed as `Combine.framework`; it depends on the
  Swift toolchain gate. The 10.15, 11 and 12 stages explicitly close without
  SwiftUI. OpenSwiftUI (MIT, early development) is being watched; the
  decision is revisited by measuring how many reference apps need it.

Decision order: CoreFoundation constrains Foundation for Swift, which
constrains the Swift runtime.

**Work**

0. Bring the remaining third-party frameworks into the tree and package
   them with `minkar.framework.mk` (moved from Phase 0, 2026-10-27):
   13 frameworks build and install; CFNetwork is commented out, and
   Security and OpenDirectory are not built.
1. Complete CF-Lite up to the macOS 12 APIs, with `CFString` ↔ `NSString`,
   `CFArray` ↔ `NSArray` and `CFDictionary` ↔ `NSDictionary` bridging tests
   on every change.
2. Audit Foundation against 10.15, starting with what the reference apps
   import (`NSFileManager`, `NSRunLoop`, `NSURLSession`,
   `NSJSONSerialization`, KVO, `NSUserDefaults`). The symbol report lists
   what they miss, most used first: `NSURLSession` and `NSUUID` (all
   four apps), `NSRegularExpression` (two), the `NSHashTable` class,
   `NSJSONSerialization` and `NSProgress` (one each). Done:
   `NSException` (2026-10-25): raising no longer crashes a program that
   catches the exception, and uncaught exceptions are reported as on
   macOS. `NSUUID` and the `NSSecureCoding` protocol (2026-10-26).
   Also done 2026-10-26: `NSRegularExpression`, `NSJSONSerialization`
   (with JSON5), `NSCache`, subscripting, the collections' block API and
   lightweight generics, `NSBlock`, `@[]` and `@{}` (they did not link),
   `NSString`'s protocols, its UTF-8 (surrogate pairs, malformed input)
   and UTF-16/UTF-32 conversions, `NSNumber`'s shared doubles (1.0, 0.0,
   the infinities and NaN read back as garbage) and file streams (they
   could not read or write). Found while testing them: `+alloc` did not
   go through `+allocWithZone:`, so `alloc`/`new` gave the abstract class
   of every class cluster (fixed 2026-10-26); `NSKeyedUnarchiver` cannot
   read the XML property list `NSKeyedArchiver` writes; `CFArray` and
   `CFDictionary` are not bridged to `NSArray` and `NSDictionary` yet.
   Second batch (2026-10-26): `NSLog` crashed every program; 17
   unimplemented `NSString` methods and the search options;
   `NSInvocation` could call nothing and forwarding lost arguments and
   return values (now the System V calling convention, in both
   directions); string hashes were 0 and differed between string classes;
   mutating a collection during for-in looped forever; `NSHashTable` is a
   class with weak memory, `NSMapTable`'s weak keys and values are zeroing
   (they dangled), and `NSPointerFunctions` and `NSPointerArray` exist.
   Third batch (2026-10-26): `stringWithFormat:` printed garbage for
   `INT64_MIN` and `%e`/`%g` were wrong (`%a` was missing); `NSNumber`
   described doubles with too few digits; binary property lists can be
   written (byte for byte as on macOS) and the XML ones keep 64-bit
   integers, exact doubles and dates; the property list and keyed
   archiving APIs with `NSError` and secure coding (`NSKeyedArchiver`
   writes binary by default); `NSNull` vanished from archives; the modern
   `NSString` API (`containsString:`, enumeration by lines, words and
   sentences, percent encoding, transforms, the URL character sets);
   `NSUserDefaults` changed the whole process's locale; `NSProgress`
   exists. The image has UTF-8 locale data since 2026-10-27; Foundation's
   locale-dependent behaviour is not tested yet.
   Left from the symbol report: `NSURLSession`.
3. Security (Apple open source) for certificates and a basic keychain,
   needed for TLS in `NSURLSession`.
4. LaunchServices: bundle database and UTI types; `open` and Filer depend on
   it.
5. CoreText and CoreData: minimal implementation driven by real use.
6. Swift 6: full stdlib and runtime (not only Embedded), with concurrency in
   the system as in macOS 12, and module maps for the system frameworks.
7. API stages 10.15 → 11 → 12: a stage closes when the reference apps built
   with that SDK work; then `ProductVersion` goes up. Check at each stage
   which APIs need kernel changes.

**Swift toolchain** (the prototype gates the rest of the Swift work)

- [ ] Prototype 1: build `swiftc` (`swiftlang/swift`, `release/6.0`) on the
  Ubuntu host as a cross compiler, against the tree's `swift-6.0-release`
  LLVM.
- [ ] Prototype 2: build `libswiftCore` for `x86_64-apple-macosx10.15`
  against `Minkar.sdk` and run a (non-Embedded) Swift "hello world" in QEMU.
  If this fails, reconsider building the stdlib on an x86-64 macOS host.
- [ ] `libswift_Concurrency` and the system overlays: `Darwin`,
  `ObjectiveC`, `CoreFoundation` and `Dispatch` (from
  `libdispatch/src/swift`).
- [ ] `libswiftFoundation` overlay from the last open version, updated to
  Swift 6.0.
- [ ] Install everything in `/usr/lib/swift` in the image and add a
  `boottest` gate that runs a Swift binary.

**Exit gate (M3):** a fixed set of open source macOS command-line tools
(Objective-C and Swift) builds and passes its tests on Minkar.

## Phase 4 — Graphics and WindowServer

Goal: WindowServer draws AppKit windows on the GOP framebuffer and receives
keyboard and mouse input. Correct in software first; acceleration later,
behind a stable interface.

**Architecture**

- A display backend interface inside WindowServer: GOP (software) →
  virtio-gpu (VMs) → accelerated. AppKit and Quartz never see the backend.
- Compositing with Onyx2D and QuartzCore, as today; the port changes the
  output, not the drawing model.
- Events: kernel HID → WindowServer → each app's event queue, with the same
  `NSEvent` model.
- App ↔ WindowServer communication over Mach/XPC, not ad hoc sockets.

**Backend decision (2026-10-25)**

- **Output for 1.0:** `IOGOPFramebuffer.kext` (Apple's IOGraphics) with
  `IOFramebufferDisplay.m` in WindowServer and software compositing. Works
  on any UEFI PC without GPU drivers, at the resolution the firmware sets.
- **VMs:** a virtio-gpu 2D driver as an `IOFramebuffer` subclass, to change
  resolution without touching WindowServer.
- **Input:** IOHIDFamily, with `ApplePS2Controller` in QEMU and xHCI
  (`IOUSBFamily` and the xHCI driver) + HID on real hardware. libinput, libevdev
  and mtdev are no longer built (`CoreServices/WindowServer/Makefile`) and
  are to be removed from the tree.
- **Keyboard:** libxkbcommon and xkeyboard-config as a stopgap for the
  layouts (`WSKeymap.c`). Long term, macOS-style layouts with the
  `UCKeyTranslate` API and Text Input Sources (Phase 5).
- **3D acceleration:** deferred past 1.0, with an ADR comparing DRM-KMS +
  Mesa against our own IOAccelerator-style drivers.

**Work**

- [ ] Output backend on IOFramebuffer: `IOGOPFramebuffer.kext` +
  `IOFramebufferDisplay.m`. *Partial:* the kext loads
  (`IOFB: GOP framebuffer 1024x768`) and WindowServer builds with the
  backend (`1ea4bce4eb`), but WindowServer has never run.
- [ ] Check in QEMU that WindowServer starts and draws on
  `IOGOPFramebuffer`. Its launchd job
  (`SystemLibrary/LaunchDaemons/org.minkar.WindowServer.plist`) is in the
  tree but not installed yet.
- [ ] IOHIDSystem input reader in WindowServer. Today WindowServer reads no
  hardware: `WSInput` only translates and dispatches. A client with
  `kIOHIDServerConnectType`, the shared `EvGlobals` queue and a Mach port
  notification, like the display with `kIOFBServerConnectType`. Test: in
  QEMU, a key press and a mouse move reach an app as `NSEvent`s.
  - [ ] PS/2 keyboard. *Partial:* `ApplePS2Keyboard` is an `IOHIKeyboard`
    and its keys reach the console (`IOBSDConsole`: typing at the login
    prompt works in QEMU), but IOHIDSystem delivers no events to user space
    clients (its dispatch loop is under an undefined `NEW_HID`, and the
    event queues are not created).
  - [ ] `ApplePS2Mouse` driver (an `IOHIPointing` subclass) on the
    `ApplePS2MouseDevice` nub; only the nub exists today. `IOHIPointing` is
    built since `90c96e8862`.
  - [x] `IOKit/hidsystem` headers (`IOHIDShared.h`, `IOLLEvent.h`) in the
    SDK, installed by `IOHIDFamily/HID/Makefile`.
  - [x] Then: xHCI + HID for real hardware and a USB tablet in QEMU
    (`MinkarUSB`; see Phase 1). Untested on real hardware.
- [ ] Remove `libinput`, `libevdev` and `mtdev` from the tree.
- [ ] virtio-gpu 2D driver as an `IOFramebuffer` subclass.
- [ ] Multiple displays and resolution changes, only after M4.
- [ ] 3D acceleration ADR (deferred past 1.0).

**Exit gate (M4):** in QEMU and on a reference PC, several AppKit apps open
windows, are dragged, receive keyboard input and redraw without artifacts.

## Phase 5 — Magma desktop

Goal: a complete Monterey-style graphical session with login, global menu
bar, Dock, desktop icons and preferences. The order follows what the user
sees at boot.

1. **LoginWindow:** authentication against the Phase 2 user service, and
   session start (the user's `LaunchAgents`).
2. **SystemUIServer and menu bar:** the active app's menu through
   WindowServer, plus a public Menu Extras and `NSStatusItem` API.
3. **Basic Menu Extras:** volume, displays, network, clock, notifications.
   Each one a loadable bundle, not built into SystemUIServer.
4. **Dock:** launcher, running apps, minimizing and wallpaper, on
   LaunchServices.
5. **Filer:** a new pure Cocoa app. Browsing with LaunchServices, mounting
   through DiskArbitration, desktop icons, Trash and "Open With".
6. **System Preferences:** host app + `.prefPane` bundle loader
   (`NSPreferencePane`). First panes: Displays, Keyboard, Users, Network,
   Date & Time.
7. **AppKit:** complete the missing controls and give the whole system the
   Monterey look (our own design, no Apple artwork), prioritized by what
   Filer and System Preferences use.

All of these are ordinary Cocoa apps or bundles, so they double as a
continuous AppKit test.

**Exit gate (M5):** from boot, a graphical session starts, an app opens from
the Dock, Filer browses, and the resolution changes from System Preferences.

## Phase 6 — Applications and distribution

Goal: install software as on a Mac (drag from a DMG to `/Applications`) and
have the basic everyday apps. This is the first real use of the system and
the best source of bugs.

**DMG**

1. Our own `DiskImages.framework` (MIT/BSD): read UDIF with its usual
   compression formats, then write.
2. Mount through DiskArbitration at `/Volumes/<label>`, using the Phase 1
   HFS+ driver.
3. A minimal `hdiutil` (`attach`, `detach`, `create`) on the framework, and
   double-click in Filer.

**Applications**

| App | Recommended option | Why | Critical dependency |
| --- | --- | --- | --- |
| Terminal | Write in Cocoa | Exercises `NSTextView`, pty and fonts | AppKit text, CoreText |
| TextEdit | Write in Cocoa | Exercises documents, RTF and the Cocoa text system | `NSDocument`, CoreText |
| Browser | Ladybird (BSD-2-Clause) | Already has its own AppKit UI | Foundation networking, AppKit |
| IDE | CodeEdit, long term | Needs SwiftUI, excluded from stages 10.15–12 | SwiftUI |

Many current macOS apps use SwiftUI; this is tracked as an open decision
(see Phase 3).

**Exit gate (M6):** a DMG is downloaded, mounted, the app copied to
`/Applications` and run; Terminal, TextEdit and the browser are used daily
on the development machine itself.

## Phase 7 — Binary compatibility and arm64

Goal after 1.0: run x86-64 binaries built on macOS without recompiling, and
port the system to arm64. It does not block the critical path, but some
decisions must be made now so it stays possible.

**Decisions that protect this phase today**

- Install names of dylibs and frameworks identical to macOS
  (`/System/Library/Frameworks/...`). *Done 2026-10-25* for every
  framework (AppKit in `Versions/C`), `libobjc.A.dylib` and
  `libbz2.1.0.dylib`, with macOS's compatibility versions; checked in the
  image with dyld. libicucore exports ICU's plain names (`u_strlen`), as
  macOS programs import them.
- Each dylib's compatibility and current versions follow the closed API
  stage (10.15 → 11 → 12).
- No changes to the layout of public ivars or the size of structs exposed in
  headers.
- `ProductVersion` and `uname -r` are documented together; the compatibility
  matrix records which apps fail because they derive the macOS version from
  Darwin.

**Work (after M6)**

1. Load macOS binaries in dyld: code signatures ignored or checked without
   Apple's chain, hardened runtime tolerated.
2. Compatibility matrix with real x86-64 apps: starts, works, fails and why.
3. arm64: boot loader, XNU arm64 platform and cross toolchain; start on
   QEMU `virt`.

**Exit gate (M7):** a set of downloaded third-party x86-64 apps runs without
recompiling; the arm64 image boots to M2 in QEMU.

## Milestones and critical path

| Gate | Phase | State |
| --- | --- | --- |
| M0 | Build, toolchain, CI | Met locally; CI written, first green run pending |
| M1 | Darwin boot | Met in QEMU and on PC 1; a second PC missing |
| M2 | Base userland | Met (`boottest GATE=M2`) |
| M3 | Base frameworks and Swift | Not started |
| M4 | Graphics and WindowServer | In progress |
| M5 | Magma desktop | Not started |
| M6 | Applications and distribution (1.0) | Not started |
| M7 | Binary compatibility, arm64 | After 1.0 |

Phases 3 and 4 run in parallel after M2, and both must close before Magma.
M6 is 1.0; Phase 7 comes later and today only requires not breaking the ABI.
Dates stay open until it is known how many people take each phase.

## Risks and mitigations

The dominant risk is people, not technology: a small team with many layers
open at once. The general mitigation is closing gates in order and limiting
work in progress.

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Few developers with XNU and kext experience | Phase 1 drags and blocks everything | Document the boot step by step; only QEMU hardware until M2 |
| Public CF-Lite predates macOS 10.12 | CF APIs missing for stages 11 and 12 | Add them by hand with swift-corelibs CF as reference; bridging tests on every change |
| Heterogeneous real hardware | Bugs nobody can reproduce | Short reference hardware list; everything else best effort |
| No clear path to graphics acceleration | Slow desktop on real hardware | Abstract backend in WindowServer; virtio-gpu first; DRM vs. own drivers decision documented |
| SwiftUI outside the roadmap | Many modern apps do not build | Decided: stages close without SwiftUI; OpenSwiftUI watched; measure how many reference apps use it |
| Licenses and clean room | Code that has to be removed | License review on every third-party import; clean-room statement on every PR |
| Work spread over many layers | Nothing becomes usable | Public board per phase; at most two active phases at a time |
| Stale documentation | Contributors work from old information | This roadmap lives in `Docs/` and is updated at every milestone |

## Next steps

The highest-impact open items, in critical-path order:

1. Reference PC 1 (Phase 1): its console is usable at the native
   resolution and both network cards are on the LAN; next, powering off
   (ACPI S5), the other CPUs (SMP, also missing in QEMU), NVMe and the
   boot without `pci_keep_console`; then a second, different PC (gate M1;
   a Mac Pro 6,1 is the candidate). See [Hardware.md](Hardware.md).
2. BSD userland batches (Phase 2): `nslookup`/`dig`,
   then the rest of `Docs/Commands.md`'s missing BSD tools.
3. IOHIDSystem event delivery in the kernel, the `ApplePS2Mouse` driver and
   the WindowServer input reader (Phase 4).
4. Install WindowServer's launchd job and get a first boot that draws on
   `IOGOPFramebuffer` (Phase 4).
5. The Foundation classes the reference apps miss most, from the symbol
   report (Phase 3).
6. The Swift toolchain prototypes (Phase 3).
