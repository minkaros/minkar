# Roadmap

What Minkar aims at, how it is measured, what is done and what is open. Last reviewed:
2026-11-27. Spanish copy: [ROADMAP.es.md](ROADMAP.es.md).

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
| 19.6.0 | Darwin 19.6.0 | Gates M5 and M6 (with audio AU1–AU6 and AU8–AU10) and app set 19 green (BC4) |
| 19.6.x | Darwin 19.6.0 | Fixes, MB4, D4, K4, apps A5–A10, audio AU7, the tests on PC 2; `ProductVersion` may rise by app sets |
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
[Atlas.md](Atlas.md). Passed on PC 1 on 26 Nov 2026 (the tests' `tools/m5-gate.sh`), and
again on 28 Nov 2026 by the maintainer, with a session, on a system installed from the
release medium (`Minkar-19.6.0.img`, `b57f1c58dc`).

- [x] LoginWindow: Directory Services, the user's Aqua session and its `LaunchAgents`, Lock Screen. `637ca4f310`
- [x] Menus: MB0–MB3 (MB4 in 19.6.x).
- [x] Dock: D0–D3 (D4 in 19.6.x).
- [x] Files: K0–K3 (K4 in 19.6.x).
- [x] System Preferences: the app and its panes, reviewed against the reference (tests `general-pane`, `users-pane`, `network-pane`).
- [x] The Atlas visual language: light and dark appearances, accent and highlight colors; approved 25 Nov 2026. `97657a1692`
- [x] Commands: `lsappinfo`, `mdls`, `mdfind`, `mdutil` (no index), `say` (tests `lsappinfo`, `md-commands`, `say`).

### Phase 6 — Applications and distribution

Gate M6, Minkar 19.6.0: installed on a physical machine from its own USB medium; a DMG
downloaded, mounted and its app run; Atlas complete for daily use (MB2–MB3, D2–D3, K2–K3,
A0–A4, N0–N1, W0–W3); WebKit through W5; audio AU1–AU6 and AU8–AU10; app set 19 green (BC4).

- [x] DMG: UDIF with ADC, zlib, bzip2, LZFSE and LZMA chunks; `hdiutil convert` and `create`.
- [x] HFS+ compression: read (zlib, LZVN, LZFSE) and written by `ditto --hfsCompression` (test `hfs-compression`).
- [x] hfs's adaptation finished: the sysctl list built once, assertions as the kernel's configuration says. `e0ef2f1168`
- [x] System installer: USB medium, Installer and Setup; installs on PC 1 (24 Nov 2026; test `install-gate.sh`). `4bee1bce14`
- [x] Apps: A0–A4 ([Atlas.md](Atlas.md#5-applications)); A5–A10 in 19.6.x.
- [x] Commands: gitk and git gui on Tcl/Tk Aqua, `osascript`, `opendiff` (tests `tcltk-git`, `osascript`, `opendiff`); `tmutil` in 19.6.x.
- [x] Security's authorization: authd, the Authorization Agent, the panes' lock (tests `authorization`, `pane-lock`). `725fc8177d`
- [x] Printing over the network: cupsd, its commands, the Printers pane and the print panel; a network printer prints from PC 1.
- [x] Printing over USB: `IOUSBInterfaceUserClient`, IOUSBLib's plug-in, CUPS's USB backend, a ring per bulk endpoint in the xHCI driver, and the Printers pane's generic PDF models; a laser printer added from the pane prints on PC 1 (28 Nov 2026; tests `usb-apps`, `printers-pane`). `e75ceafe13` `ed2cb97416`
- [x] Graphical boot, installed system and installer: the boot picker with a countdown, a logo and a bar that never stands still, no text over them; verbose with Command-V or from the picker; seen on PC 1 (28 Nov 2026; tests `graphical-boot`, `loader-log`). `7e29e17901` `ee3d0b8e0b` `5e7c18a7b2`

#### WebKit

- [x] W0 — Its 23 dependencies in `/usr/lib` pass their tests on PC 1 (`bmake w0-check`). `8db248db40`
- [x] W1 — JavaScriptCore with its JIT: the twelve JSTests suites pass on PC 1 (`bmake w1-check`; skips in `Tests/W1/skip.txt`). `27a2277e77`
- [x] W2 — WebCore and the processes on a Minkar WPE platform. `5abd93f984`
- [x] W3 — `WKWebView`. `a5d929ebdd`
- [x] W4 — The older `WebView`. `86ed0b5d33`
- [x] W5 — The designated apps' web views, from their published binaries (tests `foundation-w5-apps`, `appkit-w5-apps`). `ac148eb478`

#### Audio and media

Minkar's own frameworks on FFmpeg (LGPL) and dav1d; no GTK, Qt or GStreamer.

- [x] AU1–AU6 — IOAudioFamily and HDA, the audio service and CoreAudio, AudioToolbox, FFmpeg and dav1d, CoreMedia and AVFoundation, WebKit's media. `f39b73ee62`
- [ ] AU7 — USB Audio Class 2.0; in 19.6.x.
- [x] AU8 — CoreMIDI: midiserver, virtual endpoints between processes, the IAC Driver (test `coremidi`). `22ade151db`
- [x] AU9 — Hosting Audio Units; Accelerate's vDSP, vForce and vImage conversions (test `au-hosting`). `2f42b66c97`
- [x] AU10 — Server plug-ins, aggregate devices, process taps; latency and drift measured on PC 1. `33ad929fb2`

### Lane BC — Binary compatibility

The designated apps' published x86-64 binaries start and work without recompiling; the
apps and how far each gets are in [Compatibility.md](Compatibility.md). Install names,
public ivar layouts and struct sizes stay as on the reference platform.

- [x] BC0 — Gate BC (`bmake bc-check`) in the CI with every designated app. `17505a91f2`
- [x] BC1 — The generated matrix, [Compatibility.md](Compatibility.md).
- [ ] BC2 — Loader cases: chained fixups, `LC_BUILD_VERSION`, universal binaries, embedded frameworks, signatures and hardened runtime, `SYSTEM_VERSION_COMPAT`; partial.
- [ ] BC3 — ABI audit from the type encodings in the apps' binaries, without the vendor's headers.
- [x] BC4 — App set 19 green: TextMate, Platypus, Sloth, Firefox, Thunderbird, Hex Fiend (KeepingYouAwake in 19.6.x).
- [ ] BC5… — One group of designated apps per `ProductVersion` rise.

### Lane ARM — arm64

On the new kernel, in a virtual machine only for now.

- [x] ARM0 — The userland and frameworks compile for arm64 in the CI (about 96 % of the objects); nothing is linked or booted.
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
done, G5 (Metal) left; its clocks and the tests on PC 2 in 19.6.x. RDNA2 (PC 1): R0–R2
done, R2b next; R3–R4 give WebRender and the compositor the GPU there. Then Intel Gen9.
Detail in [GPU.md](GPU.md).

## Next steps

Lane A, in order, to close 19.6.0:

1. Why PC 1 takes about a minute to restart.
2. Gates M6 and BC with the final image; the full suite; the release image; `devel` to
   `main` and the 19.6.0 tag, with the maintainer's confirmation; 19.6.0 ships as Pre Alpha.

Lane BC alongside: BC2. The GPU (R2b on PC 1) in parallel; what needs PC 2 waits for 19.6.x.

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
