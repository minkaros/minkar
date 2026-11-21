# Minkar's documentation

Every document in `Docs/`, one line each, grouped by what the reader wants. Last
reviewed: 2026-11-21.

## Start here

- [README](../README.md) — what Minkar is, what works, where to start.
- [BUILDING.md](BUILDING.md) — build the tree, make an image and boot it in QEMU.
- [CONTRIBUTING.md](CONTRIBUTING.md) — how to contribute, the first tasks, the rules for code and documentation.

## Plan and status

- [ROADMAP.md](ROADMAP.md) — the goal, the releases, and every open phase and lane.
- [Progress.md](Progress.md) — the state at a glance, generated from `progress/status.toml`.
- [Compatibility.md](Compatibility.md) — how far each designated app gets, and the frameworks they ask for.

## Design and reference

- [Atlas.md](Atlas.md) — the desktop: its rules, how each part is built, the open milestones.
- [WindowServer.md](WindowServer.md) — the window server: process, protocol, compositing, input.
- [SwiftUI.md](SwiftUI.md) — SwiftUI's design: Minkar's own layer on a SwiftCrossUI-based engine.
- [GPU.md](GPU.md) — the GPU families, their steps, the performance requirement and the measurements.
- [Structure.md](Structure.md) — the repository's structure and the steps that move parts out.
- [SourceMap.md](SourceMap.md) — where things are in the tree, and in what order they build.
- [Userland.md](Userland.md) — the commands: policy, origins and coverage.
- [Hardware.md](Hardware.md) — the reference machines and what works on each.
- [BootArgs.md](BootArgs.md) — the loader's and the kernel's boot arguments.

## Decisions

- [Decisions.md](Decisions.md) — every decision, one line each, newest first.
- [ADR/](ADR/) — architecture decision records: [0001](ADR/0001-Kernel-Entitlements.md) kernel entitlements, [0002](ADR/0002-GPU-First-Family.md) the GPU families, [0003](ADR/0003-Kernel-Upgrade.md) the move to Darwin 25, [0004](ADR/0004-App-Sandbox.md) the app sandbox, [0005](ADR/0005-Audio-And-Media.md) audio and media, [0006](ADR/0006-WebKit.md) WebKit, [0007](ADR/0007-Product-Version.md) the release and the reported version, [0008](ADR/0008-CF-Bridge-Symbols.md) the bridged classes, [0009](ADR/0009-Security-Framework.md) Security.
