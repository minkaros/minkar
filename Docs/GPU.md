# GPU track

The GPU track: which GPU families Minkar drives, in what order, what is left for each,
and Minkar's own measurements. For contributors to the drivers, Mesa and the window
server's compositor. Last reviewed: 2026-11-21.

The track runs in parallel with every release and is not cut back. Minkar 19.6.0 closes
the functional gates; the performance requirement (equal to or better than the reference
system on the same physical machine) is the gate of a release after 25.x, measured on the
new kernel, and depends on this track.

## Families and order

Decided in [ADR 0002](ADR/0002-GPU-First-Family.md):

1. **GCN 1.0**, PC 2's two AMD FirePro D500 (Tahiti, PCI 1002:679e): steps G1–G5.
2. **RDNA2**, PC 1's AMD Radeon RX 6600 (Navi 23): steps R0–R5.
3. **Intel Gen9**, the HD 630 in PC 1's i7-7700, with `i915` and ANV/iris: not started.

Licenses: only drm-kmod's MIT/BSD parts, under a LinuxKPI-style layer for XNU. AMD's
firmware is fetched at a pinned version with its SHA-256 and kept under its own license;
it is never committed. PC 2's other configuration, the FirePro D300 (Pitcairn), is
untested: the driver knows its tables, but no D300 is at hand.

## GCN 1.0 (PC 2)

| Step | What | State |
| --- | --- | --- |
| G1 | KMS: `MinkarAMDFramebuffer` takes the display from the firmware, modes through the VBIOS's tables | Done (`a42a1800b1`, `050acdb1ee`) |
| G2 | VRAM and GART, SDMA rings, fences, interrupts, per-process GPU address spaces, the user client | Gate met (`6be0dcb84c`); left: the GFX ring, the GPU's clocks (deferred), DisplayPort link training (no DisplayPort display) |
| G3 | Mesa RADV on Minkar's libdrm; `VK_EXT_metal_surface` over `CAMetalLayer` | Done: a triangle in a window, CTS subset 1,887 passed, 0 failed (`18629db65a`) |
| G4 | `WSVulkanBackend` in the window server, page flips at the vertical blank | Done (`bfb9e26e4d`, `e487b84770`); the copy from swapchain image to drawable is left |
| G5 | Metal on Vulkan (darling-metal and Indium, MPL-2.0) | Not started. Gate: a Metal sample app renders |

**Next:** the GPU's clocks (si_dpm and the SMC), then the measurements below again, then G5.

## RDNA2 (PC 1)

The RX 6600 is Navi 23 (PCI 1002:73ff): GFX 10.3, display engine DCN 3.0.2 (amdgpu's
`dcn302`), 8 GB of VRAM. What differs from GCN 1.0:

- **Display engine:** DCN, not DCE. Each pipe is a plane reader (HUBP) feeding a timing
  generator (OTG), with clocks (DCCG) and outputs (DIO) of its own. The VBIOS keeps no
  display command tables; the display microcontroller (DMCUB), left running by the GOP,
  runs them.
- **Firmware:** loaded by the PSP, the security processor (SDMA 5.2, the graphics block,
  the SMU). The PSP and the SMU are brought up at start, once the root file system is up.
- **Registers:** described by IP blocks and segments; `tools/drm-import/dcn302-regs.py`
  takes the ones MinkarDRM uses from drm-kmod's MIT headers.
- **Firmware files:** `dimgrey_cavefish_*.bin` from linux-firmware, pinned by SHA-256.

| Step | What | State |
| --- | --- | --- |
| R0 | Display engine read: `MinkarDisplayPipes` publishes what each pipe shows | Done |
| R1 | Display taken over in the firmware's mode, own framebuffer, flips at the vertical blank | Done (`603ab40577`) |
| R2 | EDID, sleep and wake, hot plug, HDMI audio, PSP and SMU, 50 and 60 Hz | Done (`1d2ea76c1f`, `7fb2d83841`, `1120783e0c`) |
| R2b | A display the firmware did not light; smaller sizes through the scaler; other totals | Partial: a display lit after start works; the scaler and other totals are left |
| R3 | Memory hubs (GART, per-process address spaces), SDMA 5.2, interrupts, user client | Not started. Gate: G2's checks on PC 1 |
| R4 | GFX 10.3 with its microcode, libdrm and RADV, the Vulkan backend | Not started. Gate: G3's and G4's checks on PC 1 |
| R5 | SMU clocks and power: full clocks and idle states, then the measurements again | Not started |

HDMI 2.0 rates (above 340 MHz, scrambled) are out, as on GCN 1.0. DisplayPort waits
for a DisplayPort display. **Next:** R2b.

## Measurements

Minkar's own figures, taken on the physical machines, so each step of the graphics stack
can be compared with the ones before it. Figures of the reference system are never
committed: they are taken on its vendor's hardware and kept outside the repository.

### How they are taken

- `wsctl stats` (interface version 17): frames, compositing time, frame time (drawn and
  presented, including the wait for the vertical blank with page flips), damage to
  present, late frames (over two refreshes), and input-to-present latency (from
  IOHIDSystem's time of a pointer move to the present of the next frame). Percentiles
  over the last 1,024 frames; `wsctl stats reset` starts a measurement.
- `minkar-vkcopy`: the cost of presenting a Vulkan frame, the GPU's copies timed by GPU
  timestamps and the CPU's by its clock (medians of 100 runs).
- A script restarts the window server with each compositor backend
  (`WSCompositor=pixman|vulkan`) and runs each scene for 15 to 30 seconds: the idle
  desktop with two windows; a 600x900 window sliding at 1,200 points a second with the
  pointer circling; a Vulkan app presenting through a `CAMetalLayer`; the real mouse.

### G4.6 — the window server on Vulkan (2026-11-03)

PC 2, FirePro D500 driving one display at 3440x1440 and 60 Hz (the mouse scene at
1152x864 and 75 Hz), mouse and keyboard through a KVM switch at about 690 pointer events
a second. **The GPU runs at the clocks the firmware leaves** (memory 150 MHz, engine
450 MHz, of 1,270 and 725 MHz), so every figure that touches VRAM is several times
slower than the hardware allows. They are a baseline.

| Scene | | pixman | Vulkan |
| --- | --- | --- | --- |
| Idle desktop | frames, window server CPU | 0, 0 | 0, 0 |
| Sliding window | compositing p50 / p95 | 0.5 / 0.9 ms | 1.8 / 2.0 ms |
| | frame, presented | 3.0 ms (copied, no wait) | 16.6 ms (waits for the vertical blank) |
| | damage to present p50 / p95 | 12.9 / 19.9 ms | 25.5 / 32.9 ms |
| | late frames | 0 of 1,001 | 12 of 975 |
| | window server CPU | 1.2 s in 15 s | 2.4 s in 15 s |
| Vulkan app | compositing p50 | 14.4 ms | 1.0 ms |
| | window server CPU | 13.4 s in 15 s (89 %) | 1.4 s in 15 s (9 %) |
| | the app's frame rate | 49 fps | 59.7 fps (the display's) |
| Real mouse | input to present p50 / p95 | 16.0 / 19.0 ms | 28.0 / 29.9 ms |
| | window server CPU | 18 % | 17 % |

Presenting a 3440x1440 Vulkan frame (`minkar-vkcopy`):

| | Time | |
| --- | --- | --- |
| Since G4.5: the GPU copies the image into the drawable in VRAM | 2.65 ms | 7.5 GB/s |
| Before G4.5: the GPU copies it into system memory | 7.0 ms | 2.8 GB/s |
| then the CPU into VRAM | 6.7 ms | 2.9 GB/s |

What they show:

- Vulkan apps are where the Vulkan backend counts: compositing 14 times cheaper, a tenth
  of the window server's CPU, the app at the display's rate.
- In simple scenes pixman is still faster, because the GPU's memory runs at 150 MHz.
- Page flips cost one refresh of latency (28 ms against pixman's 16 ms, which may tear).
- The copy left in G4.5 takes 2.65 ms; about 0.3 ms is expected at full memory clock.

### Work these figures point to

For the performance gate of a release after 25.x:

- The GPU's clocks (si_dpm and the SMC), then these measurements again.
- Compositing just before the vertical blank, not at the first damage, to bring
  input-to-present near one refresh.
- Coalescing pointer moves within a frame: about 690 events a second cost the window
  server 17 to 18 % of a core.
- Rendering into the drawable without the copy left in G4.5, if it still matters once
  the clocks are set.
