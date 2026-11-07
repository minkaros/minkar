# Minkar — measurements

Minkar's own performance figures, taken on the physical machines, so each
step of the graphics stack can be compared with the ones before it and,
from Minkar 21.0's performance gate on, with the reference system. Figures
of the reference system are never committed (they are taken on its
vendor's hardware, kept outside the repository). Spanish:
[Measurements.es.md](Measurements.es.md).

## How they are taken

- `wsctl stats` (interface version 17): frames, compositing time (the
  frame drawn), frame time (drawn and presented; with page flips this
  includes the wait for the vertical blank), damage-to-present time (from
  the first change after a frame to the frame that shows it on the
  screen), late frames (over two refreshes), input-to-present latency (from
  IOHIDSystem's time of a pointer move to the present of the frame after
  it). Percentiles over the last 1,024 frames; `wsctl stats reset` starts
  a measurement.
- `minkar-vkcopy`: what presenting a Vulkan frame costs, the GPU's copies
  timed by GPU timestamps and the CPU's by its clock (medians of 100 runs).
- The scenes are run by a script that restarts the window server with each
  compositor backend (`WSCompositor=pixman|vulkan`), 15 to 30 seconds a
  scene: the desktop idle with two windows; a 600x900 window sliding at
  1,200 points a second with the pointer circling; a Vulkan app presenting
  through a CAMetalLayer; someone moving the real mouse.

## G4.6 — the window server on Vulkan (2026-11-03)

Mac Pro 6,1, AMD FirePro D500 (Tahiti, PCI 1002:679e; the one driving the display), one display at 3440x1440 and 60 Hz
(the mouse scene at 1152x864, 75 Hz), mouse and keyboard through a KVM
switch at about 690 pointer events a second. **The GPU runs at the clocks
the firmware leaves it** (memory 150 MHz, engine 450 MHz, of 1,270 and 725
MHz): its own power management is not done yet, so every figure that
touches VRAM is several times slower than the hardware's. These are a
baseline, to measure again once the clocks are set.

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

Presenting a Vulkan frame of 3440x1440 (`minkar-vkcopy`):

| | Time | |
| --- | --- | --- |
| Since G4.5: the GPU copies the image into the drawable in VRAM | 2.65 ms | 7.5 GB/s |
| Before G4.5: the GPU copies it into system memory | 7.0 ms | 2.8 GB/s |
| then the CPU into VRAM | 6.7 ms | 2.9 GB/s |

What they show:

- Vulkan apps are where the Vulkan backend counts: compositing 14 times
  cheaper, a tenth of the window server's CPU, the app at the display's
  rate. With pixman the window server reads the app's drawable in VRAM
  through the CPU's window.
- In simple scenes pixman is still faster: the CPU draws in system memory
  while the GPU's memory runs at 150 MHz. To be measured again with the
  GPU's clocks.
- Page flips cost one refresh of latency (28 ms against pixman's 16 ms,
  which shows frames at once and may tear).
- The copy left in G4.5 (from the swapchain image to the drawable) takes
  2.65 ms here, against about 0.3 ms expected with the memory at its full
  clock.

Pending work these figures point to, for Minkar 21.0's performance gate:

- The GPU's clocks (si_dpm and the SMC), then these measurements again.
- Compositing just before the vertical blank, rather than at the first
  damage, to bring input-to-present near one refresh.
- Coalescing the pointer's moves within a frame: about 690 events a second
  cost the window server 17 to 18 % of a core.
- Rendering into the drawable without the copy left in G4.5, if the copy
  still matters with the GPU's clocks set.
