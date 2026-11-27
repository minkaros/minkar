# Minkar

Minkar is an open source operating system for x86-64 PCs that runs the published binaries
of apps made for the reference platform, unmodified and without recompiling. It is built
on Darwin's kernel and runtime (XNU, objc4, CoreFoundation), its own Foundation and
Cocotron-based AppKit, and FreeBSD's userland, with new code for the gaps. Its desktop is
Atlas, with its own visual language.

Last reviewed: 2026-11-27. Spanish: [README.es.md](README.es.md).

## Status

<!-- progress:readme:begin -->
**Works today**

- Boots on generic x86-64 PCs (PC 1, PC 2) and in QEMU, from SATA, NVMe or virtio, on every CPU
- A Unix base: launchd, networking, SSH, and 354 of the reference system's 367 commands
- Objective-C, Swift 6.4, Foundation and Combine; seven open source tools pass their tests
- A GPU-composited window server and the Atlas desktop: login window, menu bar, Dock, Files, System Preferences
- Terminal, Text Editor, System Monitor and Logs; disk images, printing, a system installer
- WebKit on WPE, audio, MIDI and video (CoreAudio, CoreMIDI, AVFoundation, WebKit's media)
- Firefox, Thunderbird, Hex Fiend, Skim, TextMate, Platypus and Sloth from their published binaries

**In progress**

- Minkar 19.6.0 (gate M6): printing over USB, the final image tried on PC 1, the release
- Firefox in daily use: every symbol it imports exists; its unchecked messages done
- GPU drivers: the display on PC 1's RDNA2 (R2b), then its rings and Vulkan (R3–R4)

**Next**

- 19.6.x: the apps A5–A10, MB4, D4, K4, USB audio, the loader cases (BC2), the tests on PC 2
- After 19.6.x: Darwin 25, arm64 in QEMU, SwiftUI

_Updated 2026-11-27, devel `6e304752aa`_
<!-- progress:readme:end -->

Every milestone: [Docs/Progress.md](Docs/Progress.md). Which apps run and how far:
[Docs/Compatibility.md](Docs/Compatibility.md).

## Features

- **Binary compatibility**, the main goal: the published binaries of a set of designated
  apps start and work, one API level behind the reference platform's current one.
- **Source compatibility:** a Cocoa app can be compiled on Minkar and run.
- **The Atlas desktop:** a menu bar that reflects the active app, a Dock, Files, System
  Preferences and everyday apps, built on the Cocoa APIs apps expect.
- **The reference platform's layout:** `/Library`, `/System`, `/Users`, `/Volumes`, apps
  in bundles, installed from disk images.
- **File systems:** HFS+ (the root), FAT and exFAT (read and write), NFS; APFS comes after
  Minkar 19.6.0.
- **GPU acceleration:** a compositor designed for the GPU, AMD drivers and Mesa's Vulkan,
  with performance equal to or better than the reference system on the same machine as a
  release gate.
- **Security:** kernel-checked code signatures and entitlements, and an app sandbox.
- **A current Unix environment:** today's shells and tools, from FreeBSD and their own
  projects.

## Try it

There is no downloadable image yet. Build one and boot it in QEMU with the
[quick start](Docs/BUILDING.md#quick-start).

## Where to start

- [Docs/README.md](Docs/README.md): the documentation, by reader.
- [Docs/CONTRIBUTING.md](Docs/CONTRIBUTING.md): how to contribute, and the first tasks.
- [Docs/Compatibility.md](Docs/Compatibility.md): which apps run and what they need.

## Credits

Minkar is a fork of [ravynOS](https://github.com/ravynsoft/ravynos).
Its contributors up to the fork are listed in
[CREDITS-ravynOS.txt](CREDITS-ravynOS.txt), and ravynOS's own license is
in [LICENSE-ravynOS](LICENSE-ravynOS). Minkar's contributors are listed in
[AUTHORS](AUTHORS).

## Trademarks

See [TRADEMARKS.md](TRADEMARKS.md).

## License

Code written for Minkar is under the BSD 2-Clause license
([LICENSE-minkar](LICENSE-minkar)). Each other file keeps the license it
came with; see [NOTICE](NOTICE) for the third-party components and their
licenses.
