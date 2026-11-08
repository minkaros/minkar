# GPU track: RDNA2 (PC 1's RX 6600)

The second GPU family of [ADR 0002](ADR/0002-GPU-First-Family.md), after GCN 1.0
(G1–G4 on PC 2's FirePro D500). PC 1's card is an AMD Radeon RX 6600: Navi 23
(PCI 1002:73ff), graphics IP GFX 10.3, display engine DCN 3.0.2 (amdgpu's
`dcn302`), 8 GB of VRAM, VBIOS 115-D534P00-100. Today PC 1 draws through
`IOGOPFramebuffer`, on the framebuffer the firmware's GOP leaves; nothing is
accelerated, and the display cannot change mode nor be turned off.

Decided 2026-11-07 (maintainer): the RX 6600 comes after the batch of that
day, and its first goal is the native display, as G1 was on the D500.

## What is different from GCN 1.0

- **The display engine** is DCN, not DCE: each pipe is a plane reader
  (HUBP) feeding a timing generator (OTG), with clocks (DCCG) and outputs
  (DIO) of their own. A mode is set by programming those blocks, not by the
  VBIOS's command tables alone; amdgpu does it in its `dc` layer, with a
  display microcontroller (DMCUB) that runs AMD's firmware.
- **Firmware is loaded by the PSP**, the security processor: the DMCUB's,
  the DMA engines' (SDMA 5.2) and the graphics block's microcode go through
  it, signed. Bringing the PSP up (its own firmware, `sos` and `ta`) comes
  before any of them.
- **Power and clocks** are the SMU's (SMU 11.0.7), also through the PSP.
- The register space is described by IP blocks and segments
  (`dimgrey_cavefish_ip_offset.h`); MinkarDRM takes the registers it uses
  from drm-kmod's MIT headers with a generator (`tools/drm-import/dcn302-regs.py`).

The firmware files are AMD's (`dimgrey_cavefish_*.bin` in linux-firmware),
redistributable under their own license: fetched at a pinned version with
their SHA-256, as Tahiti's are (ADR 0002).

## Steps

- [x] **R0 — The display engine read.** MinkarAMDGPU names the family and,
  read only, publishes what each pipe shows (`MinkarDisplayPipes`: on or not,
  active and total size, the plane's address, pitch and pixel format, and
  the VRAM's place in the engine's address space). *Done (2026-11-08):* on
  PC 1, pipe 0 shows 3440x1440 (3600x1481 with the blanks), ARGB8888, from
  the start of VRAM (0x8000000000 in the engine's space; 8 GB). The pitch
  field holds 3456, as the GOP reports, while amdgpu's `dc` writes the pitch
  less one: R1 settles it.
- [ ] **R1 — The display taken over, in the firmware's mode.** A framebuffer
  of the driver's own in VRAM, inside the CPU's window (BAR 0, 256 MB on
  PC 1), and the plane's address moved to it at a vertical blank; the
  window server draws there. `IOGOPFramebuffer` stays the fallback, as
  `MinkarAMDFramebuffer` does on GCN 1.0. No firmware is needed: the pipe the
  GOP lit keeps running. Gate: PC 1's desktop drawn from the driver's
  buffer, the pitch confirmed by a test pattern, page flips at the vertical
  blank.
- [ ] **R2 — Modes.** The display's EDID over the engine's AUX and DDC; the
  PSP brought up and the DMCUB's firmware loaded; modes set by programming
  the OTG, the plane, the clocks and the output (HDMI and DisplayPort, with
  link training); the display turned off and back on; hot plug. Gate:
  `wsctl mode` changes PC 1's mode, and the display sleeps and wakes.
- [ ] **R3 — Memory and DMA.** The memory hubs' address translation (the
  GART and per-process GPU address spaces), SDMA 5.2 with its firmware,
  interrupts (IH), and the user client; the window server presents its
  frames by DMA, as G2 did on GCN 1.0. Gate: G2's checks on PC 1.
- [ ] **R4 — The graphics block and Mesa.** GFX 10.3 with its microcode,
  Minkar's libdrm and Mesa's RADV on it; Vulkan apps in windows; the
  compositor's Vulkan backend. Gate: G3's and G4's checks on PC 1.
- [ ] **R5 — Clocks and power.** The SMU, through the PSP: the GPU at its
  full clocks, and idle states. Then the measurements of G4.6 again.

R1 is the next step. R2 and later wait for the PSP, the largest piece of
this family: its start is studied first, in amdgpu's `psp_v11_0`, before R2
is begun.
