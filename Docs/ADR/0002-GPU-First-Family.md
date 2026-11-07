# ADR 0002: The GPU track's families and its first driver

Status: accepted, 2026-11-02. The order of families was decided by the
maintainer on 2026-11-01; this ADR records it and the plan built on it.
Closes the "open decision" in `Docs/ROADMAP.md` (Performance
requirements). It is the first step of the GPU track (G1) and of WS6 in
`Docs/WindowServer-Rewrite.md`.

## Context

- Minkar 21.0 requires performance equal to or better than the reference
  system on the same physical machine, 3D and compositing included
  (`Docs/ROADMAP.md`, Performance requirements). The reference system may
  only be measured on its vendor's own hardware; the machine we have for
  that is the Mac Pro 6,1 (PC 2).
- The Mac Pro 6,1 model comes with FirePro D300 (Pitcairn) or D500/D700
  (Tahiti), two of them: different chips, each with its own firmware. PC 2
  has two FirePro D500 (Tahiti, PCI 1002:679e), the GPU tested and
  measured; Pitcairn has never been tested. Both are GCN 1.0, "Southern Islands", graphics IP GFX6, display engine
  DCE 6. It is the oldest of the three families we have, and the one with
  the least mature open drivers.
- The Mac Pro will be available for a short time only.
- The other machines: an AMD Radeon RX 6600 (RDNA2, Navi 23, GFX10.3,
  display engine DCN 3.0.x), usable to test Minkar but not to publish
  reference measurements; and PC 1's Intel HD 630 (Gen9).
- Today Minkar draws through IOFramebuffer drivers without acceleration:
  `IOGOPFramebuffer` (the firmware's framebuffer) and `MinkarVirtioGPU`
  (virtio-gpu 2D in VMs, WS5). The window server composites with pixman
  behind a `WSRenderBackend`-shaped compositor; WS6 adds a Vulkan backend.
- The open drivers for these GPUs are Linux's `amdgpu` and `radeon` (and
  `i915`), carried to FreeBSD by drm-kmod over its LinuxKPI layer, and
  Mesa's user-space drivers (RADV for Vulkan, radeonsi for OpenGL, ANV and
  iris for Intel). Most of the AMD kernel code and of Mesa is under the MIT
  license; drm-kmod's LinuxKPI is BSD-2; some files of the Linux DRM core
  are GPL-2.0 only.

## Decision

### 1. Order of families

1. **AMD GCN 1.0 (GFX6, DCE 6)**, the Mac Pro 6,1's FirePro: first,
   because the reference is measured there and the machine will not be
   available for long.
2. **AMD RDNA2 (GFX10.3, DCN 3.0.x)**, the RX 6600: second. It shares the
   kernel driver (`amdgpu`) and Mesa's RADV with the first, and becomes the
   AMD development machine when the Mac Pro is gone.
3. **Intel Gen9**, PC 1's HD 630: third, with `i915` and ANV/iris.

### 2. How the drivers come in

- **Kernel:** `amdgpu` (which covers GCN 1.0 through its SI support, and
  RDNA2) is ported from drm-kmod onto XNU through a LinuxKPI-like layer of
  our own over IOKit and Mach (`Kernel/Extensions/MinkarDRM`, BSD-2): PCI,
  memory, interrupts, work queues, wait queues, firmware loading. The
  driver is wrapped as an `IOFramebuffer` (display) plus a user client
  (memory and command submission) — no DRM device nodes.
- **Licenses:** only files under MIT, BSD or MIT/GPL dual licenses are
  imported; a script checks every imported file's license before it is
  committed, and GPL-only files are rewritten or left out. Clean room as
  always: no code from anyone who has seen closed reference-platform
  source.
- **Firmware:** GCN 1.0 and RDNA2 need AMD's microcode files (from
  `linux-firmware`, binary, redistributable under AMD's license). They are
  loaded from `/usr/share/firmware/amdgpu` and ship in the image (decided
  by the maintainer, 2026-11-02), unchanged, with their license next to
  them and listed among the third-party notices. They are not source code
  and are not ours: nothing is derived from them.
- **User space:** Mesa's RADV (Vulkan) first, then OpenGL (radeonsi, or
  Zink on RADV); a winsys over our user client and a WSI that presents into
  IOSurfaces. In VMs, lavapipe and llvmpipe keep the same paths working.

### 3. The track's milestones

| Milestone | What | Gate |
| --- | --- | --- |
| G1 | This ADR; the LinuxKPI layer's first part; a KMS prototype: `MinkarAMDGPU` sets a mode through DCE 6 and scans out a buffer in VRAM as an `IOFramebuffer` | The Mac Pro boots to the login window on it at the display's native mode; `wsctl mode` changes the mode |
| G2 | Memory manager (VRAM and GTT, GPU virtual memory), rings (GFX, SDMA), fences, command submission through the user client; IOSurfaces in GPU memory | A command buffer runs and its fence signals; a copy between surfaces by SDMA |
| G3 | Mesa RADV on G2; Vulkan WSI into IOSurfaces; OpenGL after | Vulkan conformance subset; a triangle presented in a window |
| G4 | `WSVulkanBackend` in the window server (WS6), flips paced by the display | The compositing performance gate of Minkar 21.0 on the Mac Pro |
| G5 | Metal on Vulkan (darling-metal and Indium, MPL-2.0) | A Metal sample app renders |

The RX 6600 follows G1–G4 with DCN 3.0.x and GFX10.3; Intel Gen9 after,
with its own kernel port (`i915`) on the same LinuxKPI layer.

### 4. While the Mac Pro is here

The machine's time goes first to what cannot be done later: the reference
measurements (kept out of the repository), and dumps for development
without it (VBIOS, PCI configuration, EDID, display and clock register
state from the reference system and from Minkar), taken with the
measurement and capture kits. G1's prototype is tested on it as soon as
it exists.

## Consequences

- GCN 1.0 support in `amdgpu` is the least used path of that driver
  (Linux still defaults to `radeon` there); what works on it is to verify
  in G1 and G2 (display through DC on DCE 6, power management, the
  Vulkan level RADV reaches on GFX6). If `amdgpu` cannot drive it, the
  fallback is `radeon` for the display (G1) with the same LinuxKPI layer.
- When the Mac Pro is gone, GCN 1.0 work goes on only with the dumps; the
  RX 6600 carries the AMD track, and the performance gate on GCN 1.0 waits
  until a machine with that GPU is available again.
- The LinuxKPI layer is shared by every family, so its first part is
  written for G1 and reused.
