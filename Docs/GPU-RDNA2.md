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
- [x] **R1 — The display taken over, in the firmware's mode.** A framebuffer
  of the driver's own in VRAM, inside the CPU's window (BAR 0, 256 MB on
  PC 1), and the plane's address moved to it at a vertical blank; the
  window server draws there. `IOGOPFramebuffer` stays the fallback, as
  `MinkarAMDFramebuffer` does on GCN 1.0. No firmware is needed: the pipe the
  GOP lit keeps running. Gate: PC 1's desktop drawn from the driver's
  buffer, the pitch confirmed by a test pattern, page flips at the vertical
  blank. *Done (2026-11-15, `603ab40577`):* on PC 1 `MinkarAMDFramebuffer`
  takes pipe 0 and scans out its own framebuffer at VRAM 0x2000000, with
  the native mode at 49.999 Hz; the window server, the Dock and the login
  window draw there, which the maintainer saw without flicker. At start the
  driver flips 30 times between the firmware's buffer and its own: all 30
  taken in the vertical blank, a frame (20.0 ms) apart (`MinkarFlipTest`).
  The pitch: a test pattern at 3520 pixels a row, other than the
  firmware's 3456 (`minkaramd_pattern=1`), stood straight, measured with a
  ruler, both with the pitch field at the pitch and at the pitch less one,
  as amdgpu's `dc` writes it: the engine rounds a linear plane's pitch up
  to 64 pixels, so the two conventions give the same picture. The driver
  keeps the firmware's.
- [x] **R2 — Modes.** The display's EDID over the engine's AUX and DDC; the
  PSP brought up and the DMCUB's firmware loaded; modes set by programming
  the OTG, the plane, the clocks and the output (HDMI and DisplayPort, with
  link training); the display turned off and back on; hot plug. Then the
  audio over HDMI and DisplayPort (moved here from AU1 on 14 Nov 2026):
  the digital pins MinkarHDA already finds, enabled with the display's ELD.
  Gate: `wsctl mode` changes PC 1's mode, the display sleeps and wakes,
  and a sine is heard through the display.
  *Done (2026-11-15), its gate met on PC 1; R2's HDMI parts first, without
  the PSP (decided that day with the maintainer):* the VBIOS of DCN 3.0.2 keeps none
  of the display's command tables but the power gating one; the display
  microcontroller (DMCUB) runs them, and the firmware's GOP leaves it
  running with its mailbox ready (in VRAM past the CPU's window, written
  through MM_INDEX/MM_DATA). Done on PC 1: the EDID over DDC with the
  engine's I2C (`a19fcb683a`); the display sleeps and wakes with the
  display wrangler, its HDMI transmitter turned off and on through the
  microcontroller (`4e462ab890`); after a KVM switch or a cable comes
  back, the hot-plug line is seen and the transmitter turned on again
  (`7541ea2c93`); the firmware lights the display as DVI, and the driver
  turns the link to HDMI (with an AVI InfoFrame) and sets the audio up:
  ELD to the codec's endpoint, DTO, N/CTS and audio packets, and
  MinkarHDA's digital outputs, a device of its own in Sound named as the
  display; a sine was heard through the display (`1d2ea76c1f`,
  `688e101c2a`). Then the modes: a refresh rate is set through the
  microcontroller (SetPixelClock between the transmitter's off and on,
  `a00c2b2ae8`), but 60 Hz needs DISPCLK raised past the 304 MHz the
  firmware leaves, which the SMU gives once it has its firmware. So the
  PSP is started (`7fb2d83841`): its bootloader takes the key database,
  the SPL table, the system driver and the secure OS from AMD's sos file;
  its ring takes LOAD_TOC, SETUP_TMR (10 MB) and the SMU's firmware. The
  SMU is then set up (`1120783e0c`): its driver table, the power play
  table from the VBIOS, the SOC's and the display's clock DPM; the
  graphics block's features and its calibration wait for R4 (they never
  answer while that block has no firmware). It all runs at start, once
  the root file system is up; the display then goes to 60.000 Hz, and
  `wsctl mode` and Displays switch between 50 and 60 Hz. Left for R2b.
- [ ] **R2b — The rest of the modes.** Lighting a display the firmware
  did not (PC 1 started with its KVM on another computer): the timing
  generator, the plane, the clocks and the output set from nothing; the
  modes with other totals; the smaller sizes through the plane's scaler,
  as G1's on GCN 1.0. HDMI 2.0 rates (above 340 MHz, scrambled) stay out,
  as on GCN 1.0; DisplayPort waits for a DisplayPort display. Gate: PC 1
  started with its KVM elsewhere shows its desktop when the KVM comes
  back, and `wsctl mode` sets a smaller size.
- [ ] **R3 — Memory and DMA.** The memory hubs' address translation (the
  GART and per-process GPU address spaces), SDMA 5.2 with its firmware,
  interrupts (IH), and the user client; the window server presents its
  frames by DMA, as G2 did on GCN 1.0. Gate: G2's checks on PC 1.
- [ ] **R4 — The graphics block and Mesa.** GFX 10.3 with its microcode,
  Minkar's libdrm and Mesa's RADV on it; Vulkan apps in windows; the
  compositor's Vulkan backend. Gate: G3's and G4's checks on PC 1.
- [ ] **R5 — Clocks and power.** The SMU, through the PSP: the GPU at its
  full clocks, and idle states. Then the measurements of G4.6 again.

R2b is the next step. The PSP starts (R2): R3 and R4 load SDMA's and the
graphics block's firmware through it.
