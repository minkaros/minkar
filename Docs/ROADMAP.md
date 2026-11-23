# Roadmap

What Minkar aims at, how it is measured, what is done and what is open. Last reviewed:
2026-11-21. Spanish copy: [ROADMAP.es.md](ROADMAP.es.md).

The state at a glance is [Progress.md](Progress.md), generated from
`progress/status.toml`; how far each designated app gets is
[Compatibility.md](Compatibility.md); why things are the way they are is
[Decisions.md](Decisions.md).

## Goal

Minkar runs the published binaries of apps made for the reference platform, unmodified
and without recompiling, one API level behind the platform's current one: today the
current level is API 26 (arm64 only), so the target is API 25. Progress is measured by
the designated apps, not by "API N complete": a release closes when its set of designated
apps starts and works from the published binaries (gate BC).

"API N" names the public Cocoa API of Darwin N; it is not the kernel. Code, plists and
availability macros use the version number.

| API | Version in code | Year |
| --- | --- | --- |
| API 19 | 10.15 | 2019 |
| API 20 | 11.0 | 2020 |
| API 21 | 12.0 | 2021 |
| API 22–24 | 13.0 to 15.0 | 2022–2024 |
| API 25 | 26.0 | 2025, the target |
| API 26 | 27.0 | 2026, the reference platform's current level (arm64 only) |

## Versions

The Minkar version follows the Darwin number of its kernel. `ProductVersion` is separate:
it reports the compatibility level (10.15 today), never the Minkar version, and rises when
the designated apps that need a higher one pass gate BC.

| Minkar | Kernel | What closes it |
| --- | --- | --- |
| 19.6.0 | Darwin 19.6.0 | Gates M5 and M6 (with audio AU1–AU10) and app set 19 green (BC4) |
| 19.6.x | Darwin 19.6.0 | Fixes, MB4, D4, K4, apps A5–A10; `ProductVersion` may rise by app sets |
| 25.x | Darwin 25 | KX3: every gate passes on the new kernel |
| Later | Its Darwin | Designated apps, arm64, SwiftUI, performance |

- `ProductUserVisibleVersion` shows the release (19.6.0); `sw_vers` prints
  `Minkar 19.6.0 (kernel Darwin 19.6.0)`; `uname -r` stays 19.6.0 until KX3.
- `@available`, `NSProcessInfo` and `LSMinimumSystemVersion` read `ProductVersion`; every
  missing symbol is a noisy stub, so `@available` hides no silent gap.
  `SYSTEM_VERSION_COMPAT` for binaries linked against an older SDK is still to do.
- A gate decides a release, not a date. The performance requirement (equal to or better
  than the reference system on the same physical machine) is the gate of a release after
  25.x, measured on the new kernel.

## Principles

1. **Verifiable gates, bottom-up.** No green boot test, no closed phase.
2. **Published binaries run.** [Compatibility.md](Compatibility.md) and the stub log order the framework work, whatever API level a symbol belongs to.
3. **Noisy stubs.** Every stub logs its first call.
4. **Reuse before writing.** APSL code first, then swift-corelibs, then FreeBSD.
5. **Clean licenses and clean room.** Minkar's own code is BSD-2-Clause or MIT; nobody who has seen the reference platform's closed source contributes code.
6. **Scope frozen per release.** A framework or app enters only if a designated binary or the stub log asks for it; audio and the GPU are exempt.
7. **Limited work in progress.** At most two active lanes, plus audio and the GPU.

## Architecture

- Darwin 19.6.0 until KX3, then Darwin 25; objc4, CF-Lite, Foundation with the `libswiftFoundation` overlay, Swift 6.4.2, Combine from OpenCombine.
- A Cocotron-based AppKit, Minkar's own [window server](WindowServer.md) and desktop, [Atlas](Atlas.md); WPE WebKit under Minkar's own `WebKit.framework`.
- A GPU-first compositor (Vulkan), Mesa, Metal on darling-metal and Indium ([GPU.md](GPU.md)); [SwiftUI](SwiftUI.md) on an engine adapted from SwiftCrossUI.

## Closed phases

| Phase | Gate | Closed by |
| --- | --- | --- |
| 0 — Build, toolchain and CI | M0: CI builds the system and boots it | `05fa75cb65` |
| 1 — Darwin boot | M1: QEMU, PC 1 (SATA, NVMe, SMP) and PC 2 (NVMe, 24 threads) | `9c3dbe9873` |
| 2 — Base userland | M2: launchd as PID 1, DHCP, console and SSH login, name resolution | `71d99e9cdb` |
| 3 — Base frameworks and Swift | M3: seven open source command-line tools pass their tests | `22a0e67f0d` |
| 4 — Graphics and WindowServer | M4: AppKit windows drawn, dragged and typed into, in QEMU and on PC 2 | `9d510ec6c2` |

Left from these phases:

- AMD processors, x2APIC, boot without `pci_keep_console=1`; on PC 1, IPv6 DNS, ACPI S3 and Wi-Fi.
- Noisy stubs everywhere, dyld's shared cache, an optimized libSystem; Rainbow's speed test in gate M3.
- Provisioning profiles checked; the sandbox's phase 3 (IOKit, mappings, process information, preferences).
- NVMe reads at 1 MB/s in QEMU.

## Open work

### Phase 5 — Atlas desktop

Gate M5: from boot, a graphical session starts, an app opens from the Dock, Files browses
and the resolution changes from System Preferences, in the Atlas visual language. Detail:
[Atlas.md](Atlas.md).

- [ ] LoginWindow: authentication through Directory Services, the user's `LaunchAgents`, Lock Screen.
  Directory Services done: OpenDirectory (APSL) and its service in `directoryd` (users,
  groups, passwords; test `opendirectory`). The login window and the user's session done
  (autoLoginUser, the users' list, the session's apps as the user, Log Out back to the window;
  test `login-window`); the user's `LaunchAgents` in its own launchd (the Background session;
  Aqua-only agents later); Lock Screen (test `lock-screen`).
- [x] Menus: MB0–MB3 done (MB4 in 19.6.x).
- [x] Dock: D0–D3 done (D4 in 19.6.x).
- [x] Files: K0–K3 done (K4 in 19.6.x).
- [x] System Preferences: the app and its panes, with Notifications and Users & Groups (users
  created, deleted, made administrators, passwords, login options; test `users-pane`). Reviewed
  against the reference: General (appearance, accent and highlight colors, scroll bars, the
  default browser; test `general-pane`), Notifications, Network with its Advanced sheet (test
  `network-pane`), Displays, Date & Time (test `timezone-change`), Mouse, Sound, Keyboard (Text,
  Input Sources, Shortcuts: the system-wide shortcuts with the reference's numbers in
  com.apple.symbolichotkeys; tests `hotkeys`, `symbolic-hotkeys`), Dock (test `dock-pane`).
- [ ] AppKit and the Atlas visual language across the system. Done: the light and dark
  appearances (Auto by the hour) and the accent color, followed by every app at once, the menu
  bar, the Dock and the Menu Extras (highlighted in the accent, template icons tinted; Lucide's
  icons); test `general-pane`. Left: the remaining fixed greys in AppKit's controls and the apps.
- [ ] Commands: `mdfind`, `mdls`, `mdutil`, `say`. Done: `lsappinfo` (test `lsappinfo`).

### Phase 6 — Applications and distribution

Gate M6, Minkar 19.6.0: installed on a physical machine from its own USB medium; a DMG
downloaded, mounted and its app run; Atlas complete for daily use (MB2–MB3, D2–D3, K2–K3,
A0–A4, N0–N1, W0–W3); WebKit through W5; audio AU1–AU10; app set 19 green (BC4).

- [x] DMG: UDIF with ADC, zlib, bzip2, LZFSE and LZMA chunks attached by the kernel's `MinkarDiskImage`; `hdiutil convert` and `create`.
- [ ] HFS+ compression (decmpfs) through `MinkarFSCompression`; partial.
- [ ] The rest of hfs's adaptation (`Kernel/_PROVENANCE`).
- [ ] System installer: a bootable USB image, the installer app, the first start (language, keyboard, time zone, the first administrator); no test accounts in the released image.
- [x] Apps: A0–A4 done ([Atlas.md](Atlas.md#5-applications)); A5–A10 in 19.6.x.
- [ ] Commands: `osascript`, `opendiff`, `tmutil`; git's Tcl commands.

#### WebKit

- [ ] W0 — its dependencies in `/usr/lib`: partial (21 of 23 pass their own tests).
- [ ] W1 — JavaScriptCore with its JIT: the ES6 suite passes; the other suites left.
- [x] W2 — WebCore and the processes on a Minkar WPE platform. `5abd93f984`
- [x] W3 — `WKWebView`. `a5d929ebdd`
- [x] W4 — The older `WebView`. `86ed0b5d33`
- [ ] W5 — The designated apps' web views: what their code uses works (`ac148eb478`); the apps themselves left.

#### Audio and media

Minkar's own frameworks on FFmpeg (LGPL) and dav1d; no GTK, Qt or GStreamer.

- [x] AU1–AU6 — IOAudioFamily and HDA, the audio service and CoreAudio, AudioToolbox, FFmpeg and dav1d, CoreMedia and AVFoundation, WebKit's media. `f39b73ee62`
- [ ] AU7 — USB Audio Class 2.0.
- [ ] AU8 — CoreMIDI, framework and service.
- [ ] AU9 — Hosting Audio Units, and Accelerate.
- [ ] AU10 — Aggregate devices and device plug-ins; latency and drift measured against the reference.

### Lane BC — Binary compatibility

The designated apps' published x86-64 binaries start and work without recompiling; the
apps and how far each gets are in [Compatibility.md](Compatibility.md). Install names,
public ivar layouts and struct sizes stay as on the reference platform.

- [ ] BC0 — Gate BC (`bmake bc-check`) in the CI with every designated app; waits for its first CI run.
- [x] BC1 — The generated matrix, [Compatibility.md](Compatibility.md).
- [ ] BC2 — Loader cases: chained fixups, `LC_BUILD_VERSION`, universal binaries, embedded frameworks, third-party signatures and hardened runtime, `SYSTEM_VERSION_COMPAT`; partial.
- [ ] BC3 — ABI audit from the type encodings in the apps' binaries, without the vendor's headers.
- [ ] BC4 — App set 19 green; part of M6.
- [ ] BC5… — One group of designated apps per `ProductVersion` rise.

### Lane ARM — arm64

On the new kernel, in a virtual machine only for now.

- [ ] ARM0 — The userland and frameworks compile for arm64 in the CI (about 96 % of the objects); nothing is linked or booted.
- [ ] ARM1 — Darwin 25 boots on arm64 in a VM to gate M2; the boot target is not decided.
- [ ] ARM2 — The desktop on arm64 in QEMU.
- [ ] ARM3 — The designated apps' arm64 binaries start.

### Lane KX — New kernel: Darwin 25

`xnu-12377.121.6`, x86-64 first; detail in [ADR 0003](ADR/0003-Kernel-Upgrade.md). Until
KX3, `devel` stays on Darwin 19.6.0 and new framework and desktop code builds on both.

- [x] KX0 — ADR 0003 accepted for x86-64, with conditions; a compile-only check of Darwin 25.
- [ ] KX1 — Darwin 25 boots in QEMU to gate M2 on a branch; after 19.6.0 ships and after R1.
- [ ] KX2 — Every gate in QEMU, on PC 1 and on PC 2.
- [ ] KX3 — `devel` moves to Darwin 25; Darwin 19 becomes the 19.6.x maintenance branch.

### Lane SU — SwiftUI

Detail in [SwiftUI.md](SwiftUI.md).

- [ ] SU0 — The symbol inventory reports what the designated apps import from SwiftUI; in the CI.
- [x] SU0b — SwiftCrossUI's clean-room and license review, approved 2026-11-21.
- [ ] SU1 — The design, [SwiftUI.md](SwiftUI.md); proposed.
- [ ] SU2 — SwiftCrossUI imported; Minkar's SwiftUI test apps show their windows. After 19.6.0.
- [ ] SU3 — Every SwiftUI symbol the designated binaries import exists with the same name and layout.
- [ ] SU4 — A published binary that links SwiftUI starts (candidate: iTerm2).

### Lane R — Structure

The core stays in one repository; the rest leaves it in steps. Detail in
[Structure.md](Structure.md).

- [ ] R0 — The inventory and Structure.md; nothing moves.
- [ ] R1 — The toolchain to its own repository, as a pinned artifact, with AArch64; right after 19.6.0, before KX1.
- [ ] R2 — BSD's third-party sources as pinned downloads plus patches; 19.6.x.
- [ ] R3 — The CI publishes each built layer; 19.6.x, after R1.
- [ ] R4 — Atlas's apps to their own repository, built against the published SDK.

### GPU track

In parallel with everything, feeding the performance requirement. GCN 1.0 (PC 2): G1–G4
done, G5 (Metal) left, the GPU's clocks next. RDNA2 (PC 1): R0–R2 done, R2b next. Then
Intel Gen9. Detail in [GPU.md](GPU.md).

## Next steps

The maintainer's order (23 Nov 2026). Lane A, one after another:

1. Printing: done (cupsd and its commands, the Printers pane); to be tried on a real printer on PC 1.
2. W0–W3: window tabs (done), tiling (done), window restoration (done), Emoji & Symbols (done; the emoji font to choose).
3. The commands of Phases 5 and 6: `lsappinfo`, `mdfind`, `mdls`, `mdutil`, `say`, `osascript`,
   `opendiff`, `tmutil`.
4. Security's authorization in full: authd (the APSL sources in the tree), the rights
   database, Atlas's password dialog, the panes' lock (the maintainer's choice, 23 Nov 2026).
5. The system installer and HFS+ compression.
6. WebKit: W0, W1, W5.
7. LoginWindow: the user's Aqua session (agents limited to Aqua); the wait before the login
   window on PC 1.
8. The visual language (the light appearance and the controls), once the maintainer decides.

Lane BC alongside: KeepingYouAwake (the one of app set 19 with no window), BC2, BC0 in the
CI. Audio AU7–AU10 and the GPU (clocks on PC 2, R2b on PC 1) in parallel, as always.

## Risks

| Risk | Mitigation |
| --- | --- |
| A small team with many layers open | Gates in order; two lanes plus audio and the GPU; scope frozen per release |
| Few people experienced with XNU | The boot documented step by step; QEMU first |
| CF-Lite predates the APIs the apps use | swift-corelibs CF as reference; bridging tests on every change |
| GPU drivers, the longest block | A parallel track, one family at a time |
| `ProductVersion` raised with the API incomplete | Noisy stubs; gate BC before each rise |
| Designated apps that are not representative | The list reviewed at each rise |
| Unpublished pieces of the new kernel | Listed in ADR 0003; a prototype on a branch |
| Licenses and clean room | A license review on every import |
