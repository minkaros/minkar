# WindowServer rewrite plan

Status: **accepted** (2026-10-31). Phase 4 of [ROADMAP.md](ROADMAP.md).
Spanish version: [WindowServer-Rewrite.es.md](WindowServer-Rewrite.es.md).
Checked against `devel` at `21fb25034`.

This document is both the decision record and the work plan.
Read "Rules for this work" before the first commit.

## 1. Decision

Write a new WindowServer core and port the parts of the current one that are
worth keeping. Do not keep extending the current core, and do not throw all
of it away.

Before the new core exists, run the current WindowServer once, as it is, to
get the first pixels on screen (milestone WS0). That validates the kernel
input path, the framebuffer and AppKit drawing end to end, and gives the
project a graphical `boottest` gate early. After WS0 the current core only
receives fixes that WS0 needs.

**Guiding rule (decided 2026-10-31):** where the reference platform has a
known design (process layout, user, permissions, window movement), the new
WindowServer follows it, so nothing has to be redone later for
compatibility. A shortcut is allowed during development only when it is
written down in section 9 with the milestone that removes it.

## 2. Why: findings in the current code

Own code is about 2,800 lines (MIT, from ravynOS) in
`CoreServices/WindowServer`, plus about 440,000 lines of third-party input code: libinput, libevdev
and mtdev are no longer built; libxkbcommon is, for keyboard layouts. It has
never run on Minkar; its only test covers the keymap.

1. **Compositing cannot meet the performance requirement.**
   `-[WindowServer run]` (`WindowServer.m`) redraws the whole screen on every
   pass of a `poll()` with a 50 ms timeout: no damage tracking, no vsync, no
   synchronized double buffering. Clients draw into shared memory while the
   server reads it, with no fence, so frames tear. There is no place to plug
   IOSurface or a GPU backend ("FIXME: lock this to vsync").
2. **Hand-written IPC.** `message.h` defines fixed structs of about 128 KB
   (`unsigned char data[128*1024]`) that are copied whole for every message,
   a 60-byte input event included. Sends use a 50 ms timeout, so events are
   dropped under load. `message.h` itself says "refactor this to a .defs file
   and use mig". Window IDs are chosen by each client and are only unique
   together with the owner PID (`rpc.h`, `wsWindowListEntry`).
3. **Security.** Display capture creates its shared memory with
   `shmget(..., IPC_CREAT | 0666)` (`IOFramebufferDisplay.m`): any process can
   read and write the captured screen. Window surfaces are named POSIX shared
   memory objects.
4. **Window model.** States copied from X11/Wayland (`MAXVERT`, `MAXHORIZ`);
   no relative ordering (`orderWindow:relativeTo:`), child windows, sheets,
   alpha, shadows or Spaces. Window dragging is decided by the server from
   the title bar's geometry (`sendEventToApp:`).
5. **Input.** No device backend (`-[WSInput run:]` is empty), no IOHIDSystem
   reader, X keyboard layouts through libxkbcommon, no key repeat.
   `CGEventTapCreate`, `CGEventPost` and the other event tap functions in
   `Frameworks/CoreGraphics/CGEvent.m` are empty bodies of non-void functions
   (undefined behavior).
6. **Mixed responsibilities.** WindowServer launches LoginWindow and
   SystemUIServer and sets up the user environment (`launchShell:`,
   `setUpEnviron:`). Session start belongs to launchd and the login window.
7. **Core Animation.** `CARenderer` composites on the client with OpenGL;
   there is no layer tree the server can composite.

What is worth keeping:

- `IOFramebufferDisplay.m` (363 lines): the IOFramebuffer backend already
  ported to XNU (server connection, mode query, scanout mapping).
- `WSDisplay`: the display model (modes, gamma, mirroring, capture state).
- The logic of the ~60 Quartz Display Services RPCs in `WindowServer.m`
  (`rpcGetOnlineDisplayList:` to `rpcGetLastMouseDelta:`) and of the window
  list (`rpcWindowList:`), with their clients `CGDirectDisplay.m` and
  `CGWindowList.m`.
- `WSKeymap.c`, as a stopgap until layouts move to `UCKeyTranslate`.
- App activation (`activateApp:`, `switchApp`), as a reference.
- SystemUIServer and LoginWindow as clients.

## 3. What compatibility means here

The protocol between AppKit and WindowServer on the reference platform is
private, and Minkar ships its own AppKit and CoreGraphics. So the protocol
is ours to design. Compatibility is the public API and its behavior:

| Area | Public API | Today |
| --- | --- | --- |
| Quartz Display Services | `CGGetActiveDisplayList`, modes, gamma, capture, configuration | Mostly there; port it |
| Quartz Window Services | `CGWindowListCopyWindowInfo`, `CGWindowListCreateImage` | Partial |
| Quartz Event Services | `CGEventTapCreate`, `CGEventPost`, `CGEventSourceRef` | Empty |
| Display streams | `CGDisplayStream` | Missing |
| Window semantics | `NSWindow` levels, ordering, key and main window, child windows, sheets, collection behavior, alpha, shadows | Minimal |
| Screens | `NSScreen` frames, visible frames, backing scale | Partial |
| Cursors | `NSCursor`, cursor rects, hide and show, warp | Partial |
| Core Animation | Layer trees rendered by the server | Missing |

Non-goals: cloning the reference platform's private window server SPI;
Spaces and an overview UI in the first version (the model keeps room for
them); HDR; more than one GPU.

## 4. Target architecture

### Process

- One daemon, laid out as on the reference platform: the executable is
  `/System/Library/PrivateFrameworks/SkyLight.framework/Resources/WindowServer`,
  started by launchd from `org.minkar.WindowServer.plist`, with its Mach
  service in `MachServices`. It only serves; it launches nothing.
  `SkyLight` is used as an identifier for compatibility, like any other
  framework name.
- The login window and the user session are started by launchd (LoginWindow
  as a daemon on the console; the session's agents after login). The code
  in `launchShell:` and `setUpEnviron:` moves there.
- Runs as the `_windowserver` user (UID 88), as on the reference platform.
  The launchd job sets the user; the kernel grants the framebuffer and HID
  user clients to it. Root only until WS4a (section 9).

### Modules (in `PrivateFrameworks/SkyLight/`, section 10.1)

| Module | Responsibility |
| --- | --- |
| `IPC/` | `WindowServer.defs` (MIG): connection, window, surface, display, event, cursor and capture subsystems. Messages carry only their data; large data travels as out-of-line memory or memory entries. |
| `Server/` | `main`, dispatch queues, connection manager. Each client connection records PID, UID and audit token from the Mach trailer. |
| `Model/` | `WSConnection`, `WSWindow` (server-assigned global `CGWindowID`, owner, level, order, frame, alpha, opacity, shadow, parent and children, collection behavior, title, sharing state), `WSWindowList` (ordering within and across levels), `WSSpace` (one space now). |
| `Surfaces/` | `WSSurface`: a window's backing store. First a Mach memory entry (`mach_make_memory_entry_64`) sent as a port, never a named shared memory object; later an IOSurface (roadmap step 4) behind the same interface. Two buffers per window; the client draws into the back one and commits it with its damage and a seed; the server latches committed buffers at frame time. |
| `Compositor/` | `WSCompositor` over a `WSRenderBackend` interface: create a texture from a surface, draw a quad with alpha, transform and clip, present with damage, report vsync. Per-window damage plus moves and order changes give the screen damage; only damaged regions are composited and presented. Backends: `WSPixmanBackend` first (pixman is in the tree), `WSVulkanBackend` later (GPU track G4). |
| `Displays/` | `IOFramebufferDisplay` and `WSDisplay` ported: modes, gamma, mirroring, configuration, capture. A display link per display: GOP has no vsync, so a timer at the mode's refresh rate paces frames; virtio-gpu and KMS give real flips later. |
| `Input/` | IOHIDSystem reader (`kIOHIDServerConnectType`, the shared `EvGlobals` queue, a Mach notification port) → event pipeline: cursor position and acceleration, modifier state, key repeat, keymap (`WSKeymap` for now), hit testing, event taps, routing. Keys go to the key window's owner; mouse events go to the window under the pointer, or to the window that got the mouse down until the mouse up. Each client has a bounded queue; mouse moves are coalesced; the server never blocks on a client. |
| `Cursor/` | The cursor drawn last as an overlay, with its own damage; hardware cursor later. |
| `Capture/` | `CGDisplayCreateImage`, `CGWindowListCreateImage`, display capture (the client gets a surface, not a world-readable segment); `CGDisplayStream` later. |
| `Debug/` | `wsctl`: list windows and connections, frame statistics, damage, input latency. |

### Threading

- IPC on a serial queue; input on a high-priority queue; one render thread
  per display, driven by its display link.
- The model is changed only on the model queue. The render thread works on
  an immutable snapshot taken at frame start, so it never holds a lock while
  drawing.

### Window movement

As on the reference platform, the server moves windows by itself, so a
window follows the pointer even while its app is busy:

- Each window tells the server its drag regions (the title bar and, with
  `movableByWindowBackground`, the background), and updates them when its
  layout changes. A mouse down in a drag region starts a server-side move.
- `-[NSWindow performWindowDragWithEvent:]` starts the same move from the
  app.
- The app is told the new frame (`NSWindowDidMoveNotification` and the
  usual delegate calls) when the move ends, and as it goes.
- The server no longer guesses from the title bar's geometry.

### Core Animation

Version 1 composites each window's surface; AppKit renders its layers into
it on the client. Version 2 lets a client send layer-tree commits to the
server's compositor (needed for smooth animation at the performance gates).
`WSSurface` and `WSCompositor` keep that door open; the design comes in its
own ADR after M4.

### Client side

- The client side lives in the same private framework,
  `PrivateFrameworks/SkyLight.framework`, which wraps the MIG interface:
  connection, windows, surfaces, events. CoreGraphics re-exports it, as on
  the reference platform, so the private `CGS*` and `SLS*` functions that
  third-party apps call (main connection, window alpha, level, order...)
  resolve where those apps expect them. That subset is implemented from
  public information and observed behavior only; the wire protocol stays
  ours (MIG) and is not meant to match the reference.
- CoreGraphics (`CGDirectDisplay.m`, `CGWindowList.m`, `CGEvent.m`) and
  AppKit (`NSApplication`, `NSWindow`, `NSScreen`, `NSEvent`, `NSCursor`) use
  it. AppKit's public headers stop including WindowServer headers
  (`<AppKit/NSDisplay.h>` pulls in `<WindowServer/message.h>` today).

### Security

- Every request is checked against the caller's audit token.
- Privacy permissions as on the reference platform since API 19:
  - **Screen recording:** `CGDisplayCreateImage`, `CGWindowListCreateImage`,
    display streams and other apps' window titles in
    `CGWindowListCopyWindowInfo` (without it, `kCGWindowName` is left out).
    `CGPreflightScreenCaptureAccess` and `CGRequestScreenCaptureAccess`.
  - **Input monitoring:** listen-only event taps that see keyboard events.
    `CGPreflightListenEventAccess` and `CGRequestListenEventAccess`.
  - **Accessibility / posting events:** filtering taps and `CGEventPost`.
    `CGPreflightPostEventAccess`, `CGRequestPostEventAccess` and
    `AXIsProcessTrusted`.
- WindowServer asks one permission service for every check, keyed by the
  client's code identity (bundle ID, path) and the user. The service is a
  privacy database daemon with its own ADR; until it exists, a built-in
  policy answers (section 9). The check points and the API answers are the
  final ones from WS3 on.
- Surfaces are shared only through ports given to their owner; no
  world-readable or world-writable memory.

## 5. Milestones

Each milestone ends with tests that pass, `boottest GATE=M2` still green,
and the roadmap updated with the evidence.

Progress:

- [x] WS0 — First pixels with the current server (2026-10-31)
- [x] WS1 — Skeleton and interface (2026-10-31)
- [x] WS2 — Windows, surfaces and the software compositor (2026-11-01)
- [x] WS3 — Input (2026-11-01)
- [x] ADR 0001 — Kernel entitlement validation, needed by WS4a (2026-11-01)
- [ ] WS4a — Server behavior
- [ ] WS4b — The switch (with MB0 of [Menus.md](Menus.md))
- [ ] WS5 — IOSurface and virtio-gpu
- [ ] WS6 — GPU compositing

### WS0 — First pixels with the current server

Depends on the Phase 4 kernel work: IOHIDSystem event delivery (the dispatch
loop under the undefined `NEW_HID`, the event queues) and the
`ApplePS2Mouse` driver. Those are separate tasks with their own commits.

- Install WindowServer's launchd job; boot to a WindowServer that draws on
  `IOGOPFramebuffer`, with a demo client window.
- Minimal fixes only, and the capture memory not world-accessible (`0666`).
- New `boottest` gate `M4-draw`: a QMP `screendump` compared with a
  reference image (with a tolerance), and a key and a mouse click injected
  through QMP that reach the demo window as `NSEvent`s.

State on 2026-10-31:

- [x] (a) IOHIDSystem event delivery (`e2df542919`): the event server
  connection maps the shared `EvGlobals` queue and is told through its
  Mach port; pointing devices reach it. The `NEW_HID` dispatch loop stays
  off: it feeds the event system's queues, which are not built and are not
  what WindowServer reads. Test `hid-events` 9/0.
- [x] (b) `ApplePS2Mouse` (`2aa9a1da6e`): moves, three buttons, the
  wheel. Test `ps2-mouse` 12/0.
- [x] (c) WindowServer: `IOGOPFramebuffer` gives the current mode's pixel
  information (`f10f778c78`); the input backend reads the queue and the
  server no longer crashes at start (`ab71ce75a4`); the capture memory is
  the capturing client's only (`e2731c426f`); the framebuffer's server
  connection is closed when its client goes away (`9f46a761ac`); window
  surface names fit XNU's limit (`dc931eeeb0`); the launchd job is
  installed disabled, with the demo client WindowServerDemo
  (`f56c70bb88`). AppKit needed three fixes (`70b62fe8bc`), Onyx2D one
  for fonts (`6888034ad2`), WindowServer one for apps without a bundle
  (`0e3d06ec35`). Test `windowserver` 16/0.
- [x] (d) Gate `M4-draw` (`cce7da92bc`): the screen matches the
  reference (0 of 786432 pixels differ, twice in a row), and a click
  (USB tablet) and a key (PS/2 keyboard) reach the demo's window as
  `NSEvent`s. Full suite 3023 passed before (d); M2 passes.

WS0 is done (2026-10-31).

Shortcuts taken (section 9): root may open IOHIDSystem's server
connection, since no policy module validates code signatures yet and no
process has entitlements (`SHORTCUT(WS4a)` in `IOHIDUserClient.cpp`,
removed on 2026-11-01 with ADR 0001); the
legacy server's threads get 4 MB stacks for its 128 KB messages
(`SHORTCUT(WS4b)` in `main.m`); the launchd job runs the compositor only
(`-x`), since LoginWindow is not built (`SHORTCUT(WS4b)` in its plist);
Inter is the provisional system font until Gienah has its own
(`SHORTCUT(Phase 5)` in `SystemLibrary/Fonts/Makefile`).

Known issues left for later milestones: the window decorations are the
images inherited from ravynOS, which look like the reference platform's
(Gienah replaces them); POSIX shared memory objects ignore `fchown`, so a
surface is the server's user's only (the surfaces become memory entries
in WS2); an `NSEvent` is over-released when an app without windows
exits; `CGLayer.h` cannot be included from C.

### WS1 — Skeleton and interface

- This document accepted; `WindowServer.defs` and the client side in `SkyLight.framework`; the new server installed in `SkyLight.framework/Resources`.
- New core built next to the old one; the old one moves to `Legacy/` and
  stays the default.
- Display Services and the window list ported to MIG, with a test client
  that calls every one and checks the replies.
- Repository layout of section 10.1.

WS1 is done (2026-10-31). The new server runs next to the legacy one,
which stays the default; it checks in `org.minkar.WindowServer.next`
until WS4b and is not started by launchd yet. Commits: layout
(`9e48d1f62a`), interface, client and server (`f9028e286a`), display
queries (`0e4d7293ff`), modes and configurations (`8dab4060cc`), transfer
tables (`37996817e4`), connections, capture, cursor and window list
(`41e37b7441`), mirroring and fades (`030dba37c7`). `skylight-test`
calls every routine and checks the replies: 115 of 115 pass in QEMU, as
do gates M2 and M4-draw.

Each client passes a port of its own when it connects; the server is
told when that port dies and releases what the process held: windows,
captures, its cursor hide count, its fade, its app-only display changes
and the transfer tables it changed.

Shortcuts taken: a permanent display configuration lasts for the session
only (`SHORTCUT(WS5)`, with real mode changes); the transfer tables and the
fade blend are kept by the server, and the compositor does not apply them
yet (`SHORTCUT(WS4a)`); the cursor position
only moved by warps until the server read input (removed in WS3).

Left for later milestones: display IDs come from the registry entry and
last one boot (derived from the EDID in WS5, with virtio-gpu); mirroring
is checked with one display only (refusals and turning it off; several
displays come later, section 5); `CGDisplayGetDrawingContext` comes with
capture through surfaces (WS4a) and display streams in WS5; the
CoreGraphics functions keep calling the legacy server until the switch
(WS4b).

### WS2 — Windows, surfaces and the software compositor

- Window model with global `CGWindowID`s, levels and ordering.
- Surfaces as memory entries with two buffers and commits with damage.
- `WSPixmanBackend`, damage-driven compositing, display link at the
  refresh rate, cursor overlay.
- `NSWindow` drawing through `SkyLight.framework`.
- Tests: two overlapping AppKit windows, ordering changes and moves checked
  with `screendump`; frame statistics from `wsctl` (no frame while nothing
  changes; only damaged regions redrawn).

WS2 is done (2026-11-01). Commits: pixman as a system library
(`6ca5e31aae`), surfaces and the compositor (`1aa140cdab`), AppKit and
CoreGraphics through SkyLight (`b957c09fac`), gate M4-compose
(`8512fe60ba`). A window's surface is two buffers in memory the server
shares with its owner through a Mach memory entry; commits carry damage,
and the client keeps its back buffer equal to the front one. One
compositor per display composites only the damaged area with pixman, at
the mode's refresh rate and only when there is damage, cursor last.
`wsctl stats` gives frames, idle ticks, commits, damaged area and frame
times. AppKit uses the new server when the legacy one is not running.
Tests: `skylight-test` 128/0; the local compositor test (overlap, raise,
move, half alpha, cursor, no frame while idle); gate M4-compose (two
AppKit windows, a raise and a move, 0 pixels differing). Two older bugs
fixed on the way: run loops spun on `select` (`61cb01ce77`), and the
`NSEvent` over-release of WS0 (`1508bd4d3c`).

Shortcuts taken: AppKit damages its whole window at each flush
(`SHORTCUT(WS5)`, removed with IOSurface, when AppKit tracks its
dirty rects into the surface); the display link ticks at the refresh rate
even when idle, though no frame is drawn then (`SHORTCUT(WS5)`, removed
with real flips).

### WS3 — Input

- IOHIDSystem reader, routing, key repeat, coalescing, mouse capture during
  drags, key and main window, client-started moves.
- `CGEventTapCreate`, `CGEventTapEnable`, `CGEventPost`,
  `CGGetEventTapList` implemented.
- Tests: QMP-injected keys and clicks reach the right window; a
  listen-only tap sees them; a filtering tap can drop one; `CGEventPost`
  reaches the key window.

WS3 is done (2026-11-01). Commits: input from IOHIDSystem and routing
(`c3763a06a8`), server-side moves, taps and posting (`03bb7ed773`),
CoreGraphics' events (`fa7aae5e38`), AppKit (`13f99802d5`,
`d0c63753c2`), gate M4-compose with input (`98237bf14e`). The server
reads IOHIDSystem and routes: mouse events to the window under the
pointer, or to the one that got the mouse down until every button is up;
keys to the key window; a click makes its window key. It counts clicks,
repeats held keys, coalesces moves in each connection's bounded queue and
never waits on a client. A window is moved by the server from a mouse
down in its title bar. `CGEventTapCreate`, `CGEventTapEnable`,
`CGEventPost` and `CGGetEventTapList` work; a filtering tap that does not
answer within a second is disabled. Tests: the local input test (moves,
clicks, double clicks, key repeat, a drag out of the window, a listen-only
tap seeing injected input), `eventtap-test` 15/0 (a posted key reaching
the key window, a filtering tap dropping a key, a tap disabled by
timeout), gate M4-compose (a click, a key and a title-bar drag through
AppKit, 0 pixels differing). Found on the way: an event freed under
`nextEventMatchingMask:` (`13f99802d5`), `CGEventMaskBit` defined as its
argument, and `CGLayer.h` not usable from C (`b4f8ed015d`, the known
issue of WS0).

Shortcuts taken: input is read and routed on the IPC queue
(`SHORTCUT(WS4a)`); the built-in policy of section 9 decides posting and
filtering, root or the console's user (`SHORTCUT(M5)`: the privacy service
ADR comes before gate M5); `CGEventSource` does not ask the server for the
HID state yet (`SHORTCUT(WS4a)`). The keymap and libxkbcommon are still the legacy
server's, linked from there until WS4a moves them to SkyLight (then a
`SHORTCUT(Phase 5)`, section 9).

### WS4a — Server behavior

`Legacy/` is still present and stays the default. Needs ADR 0001
([ADR/0001-Kernel-Entitlements.md](ADR/0001-Kernel-Entitlements.md)).

- **`_windowserver` (UID 88).** The user and group added to the base
  system's password and group files; `UserName` set in the launchd job.
  It depends on ADR 0001: without the entitlement check WindowServer cannot
  open IOHIDSystem's server connection as a non-root user.
- **Drag regions:** each window declares its drag regions (the title bar
  and, with `movableByWindowBackground`, the background), and a mouse down
  in one starts a server-side move.
- **Keymap in SkyLight:** `WSKeymap` and libxkbcommon move from `Legacy/`
  to SkyLight. On the reference platform the window server delivers
  hardware key codes and the client turns them into characters from the
  current keyboard layout (layout data and a `UCKeyTranslate`-style call in
  the app's process); libxkbcommon is not that design, so the move is a
  shortcut (`SHORTCUT(Phase 5)`, section 9).
- **Symbolic hotkeys,** as on the reference platform: the server recognizes
  the system-wide shortcuts in a table kept in the preferences domain
  `org.minkar.symbolichotkeys` (the reference platform's key structure) and
  sends each one to its owner instead of delivering it as a key event:
  Command-Tab to the Dock, Command-Option-Escape to loginwindow, Control-F2
  to the front app's AppKit (focus to the menu bar). The server draws no
  app switcher; the Dock does (D4 of [Dock-Kraz.md](Dock-Kraz.md)).
- **Running applications through LaunchServices:** a LaunchServices session
  service keeps the list of running apps; each app checks in from
  `NSApplication` at launch, its exit is noticed when it happens, and the
  window server tells the service when the front app changes. It posts
  `NSWorkspaceDidLaunchApplicationNotification`,
  `NSWorkspaceDidTerminateApplicationNotification` and
  `NSWorkspaceDidActivateApplicationNotification`. A basic
  `NSRunningApplication` reads the same list: `runningApplications`,
  `currentApplication`, `processIdentifier`, `bundleIdentifier`,
  `localizedName`, `isActive`, `activateWithOptions:`. The rest of
  `NSRunningApplication` comes with D0 of [Dock-Kraz.md](Dock-Kraz.md).
- **What MB0 needs** ([Menus.md](Menus.md)): the menu bar's place and
  height on each display (`NSScreen` `visibleFrame` leaves it out), notices
  to each app when it becomes or stops being the front app, the status area
  layout (status item windows and Menu Extras, order kept), and pointer
  capture while a menu is open.
- **What the Dock needs** ([Dock-Kraz.md](Dock-Kraz.md)): as on the
  reference platform the Dock owns minimized windows. AppKit's
  `miniaturize:` asks the Dock, which runs the animation and keeps the
  tile; the window server orders the window out and back in, tells the Dock
  when a window is minimized or restored, and gives it the window's image
  through `CGWindowListCreateImage` with a single window ID. The animation
  and the tile are D2 and D3 of the Dock plan.
- `CGWindowListCopyWindowInfo` with the reference dictionary keys,
  `CGWindowListCreateImage`, child windows, sheets, alpha, software
  shadows, `NSCursor` and cursor rects, display capture through surfaces
  and `CGDisplayGetDrawingContext`.
- Shortcuts of earlier milestones removed here: input read and routed on
  its own high-priority queue, not the IPC queue (WS3); `CGEventSource`
  asks the server for the HID state (WS3); the compositor applies the
  transfer tables and the fade (WS1).

### WS4b — The switch

- **MB0** of [Menus.md](Menus.md): menus and status items drawn by each
  app's AppKit (section 10.2). WS4b closes only when MB0 passes.
- The relay codes removed from `message.h` (section 10.2).
- Session start moved out of WindowServer.
- The minimal `loginwindow` of section 10.3 (session end and `reboot(2)`,
  shortcut until Phase 5).
- The new core becomes the default, started by launchd; the CoreGraphics
  functions stop calling the legacy server. `Legacy/` is removed, and so
  are the libinput, libevdev and mtdev trees.
- Gate M4 in QEMU: several AppKit apps open windows, are dragged, take
  keyboard input and redraw without artifacts, plus MB0's menu tests.

### WS5 — IOSurface and virtio-gpu

- IOSurface (roadmap step 4) behind `WSSurface`.
- virtio-gpu 2D display (roadmap step 3) with real flips and resolution
  changes from `CGDisplaySetDisplayMode`.
- `CGDisplayStream`.

### WS6 — GPU compositing

- `WSVulkanBackend` (GPU track G4), selected per display.
- Compositing performance gate on the reference PC, required for
  Minkar 21.0: display refresh rate without dropped frames with several
  animated windows, and WindowServer CPU no higher than the reference.

Later: layer trees composited by the server (its own ADR), several
displays, Spaces, hardware cursor.

## 6. Measurements built in from WS2

- Frame time, frames presented and skipped, damaged area per frame.
- Input-to-present latency: timestamp at the HID event, at delivery, at the
  commit that answers it and at present.
- Exposed by `wsctl stats`, so the performance gates can be measured the
  same way in the VM and on the physical machines.

## 7. Tests

- Model tests (ordering, levels, damage regions) as command-line programs
  in the existing test suite.
- Integration through `boottest`: `M4-draw` from WS0, `M4-compose` from
  WS2, and gate `M4` from WS4b.
- QMP `screendump` comparisons with a per-pixel tolerance; QMP input
  injection for keyboard and mouse.

## 8. Rules for this work

- Everything committed is in English, except the `.es.md` translations of
  the documentation. No third-party product names in code,
  comments, documentation or commit messages; API and symbol names stay as
  they are.
- New code under MIT or BSD-2-Clause. Clean room: no code from closed
  sources of the reference platform; public documentation and headers of
  the public API only.
- Small commits, one subject each; show the diff before committing.
- Kernel changes (IOHIDSystem, `ApplePS2Mouse`, IOSurface, virtio-gpu) are
  separate commits from WindowServer changes.
- Mark a roadmap item done only with evidence (commit, test, `boottest`
  output) and cite it.
- Keep `boottest GATE=M2` green after every milestone; keep the old server
  working until WS4b switches.
- Development shortcuts are marked in the code with the milestone that
  removes them:
  `// SHORTCUT(<removed by>): <what and why>; taken in <milestone>.`
  (for example `SHORTCUT(WS4a)`, `SHORTCUT(Phase 5)`, `SHORTCUT(D4)`), and
  each one has a row in its plan's shortcut table (section 9 here, section
  10 of [Menus.md](Menus.md) and of [Dock-Kraz.md](Dock-Kraz.md)). A
  milestone closes only when `git grep -n 'SHORTCUT(<milestone>)'` finds
  nothing.

## 9. Decisions and development shortcuts

Decided 2026-10-31, following the guiding rule in section 1:

| Question | Decision (target) | Development shortcut | Removed by |
| --- | --- | --- | --- |
| User WindowServer runs as | `_windowserver` (UID 88), set by the launchd job (`UserName`) | Root | WS4a (needs ADR 0001) |
| Where it lives | `SkyLight.framework/Resources/WindowServer`; client in `SkyLight.framework`, re-exported by CoreGraphics | `CoreServices/WindowServer.app` for WS0 (current server) | WS1 |
| Capturing and posting events | Screen recording, input monitoring and accessibility permissions, through a permission service | Built-in policy: root and the console user's processes are allowed; everything else denied (`SHORTCUT(M5)`) | M5 (the privacy service ADR comes before gate M5) |
| Moving windows | Server-side moves from client-declared drag regions, plus `performWindowDragWithEvent:` | Title bar region only | WS4a |
| Keyboard layouts | The server delivers hardware key codes; the client turns them into characters from the current layout (layout data and a `UCKeyTranslate`-style call in the app), with Text Input Sources | `WSKeymap` and libxkbcommon in SkyLight, from WS4a (`SHORTCUT(Phase 5)`) | Phase 5 (Text Input Sources and our own layout files) |
| Ending the session | The login window asks apps to quit, ends the session and then restarts or shuts down | A minimal `loginwindow` that only ends the session and calls `reboot(2)` | Phase 5 (LoginWindow) |
| App switcher (Command-Tab) | The Dock draws it; the window server's symbolic hotkey table delivers Command-Tab to it | No switcher after the legacy server goes (WS4b): apps are switched by clicking their windows (`SHORTCUT(D4)`) | D4 ([Dock-Kraz.md](Dock-Kraz.md)) |
| Legacy server's thread stacks | Gone with the legacy server and `message.h` | 4 MB stacks for its 128 KB messages (`SHORTCUT(WS4b)` in `Legacy/main.m`, taken in WS0) | WS4b |
| What WindowServer's launchd job starts | The window server only; the session starts elsewhere (section 10.3) | The compositor only (`-x`), since LoginWindow is not built (`SHORTCUT(WS4b)` in the plist, taken in WS0) | WS4b |
| System font | Gienah's own font | Inter (SIL OFL 1.1), four faces (`SHORTCUT(Phase 5)` in `SystemLibrary/Fonts/Makefile`, taken in WS0) | Phase 5 (Gienah) |
| Permanent display configurations | Stored and applied again at the next session | They last for the session (`SHORTCUT(WS5)`, taken in WS1) | WS5 (real mode changes) |
| Transfer tables and fades | Applied to the screen by the compositor, and by gamma hardware where there is some | Kept and returned, not applied (`SHORTCUT(WS4a)`, taken in WS1) | WS4a |
| Display link while idle | Frames paced by the display's flips; nothing wakes while nothing changes | A timer at the refresh rate ticks even when idle, drawing nothing (`SHORTCUT(WS5)` in `WSCompositor.m`, taken in WS2) | WS5 (real flips) |
| AppKit's damage | AppKit sends the rects it redrew | The whole window at each flush (`SHORTCUT(WS5)` in `NSWindow.m`, taken in WS2) | WS5 (IOSurface) |
| Input queue | Input read and routed on its own high-priority queue, over a model snapshot | On the IPC queue (`SHORTCUT(WS4a)` in the server's `main.m`, taken in WS3) | WS4a |
| `CGEventSource` HID state | Asked of the window server | Not asked: buttons, keys and flags read as up (`SHORTCUT(WS4a)` in `CGEvent.m`, taken in WS3) | WS4a |
| Kernel entitlement validation | A static trust cache of system binaries and our own MAC policy kext (section 9.1) | Root may open IOHIDSystem's server connection (`SHORTCUT(WS4a)` in `IOHIDUserClient.cpp`) | Removed 2026-11-01 (`8366b8d0f6`), before WS4a |

### 9.1 Kernel entitlement validation

XNU honors a process's entitlements only when its code signature is marked
valid (`CS_VALID`, checked in `csblob_get_entitlements`), and a code integrity
policy module sets that mark. The reference platform's module is closed
source, so no Minkar process has entitlements today. Decided 2026-10-31:

- **Trust cache.** The build computes the CDHash of every signed system
  binary and writes a static trust cache (format of `kern/trustcache.h`) that
  ships in the boot image.
- **Policy kext.** Our own MAC policy module marks a binary whose CDHash is in
  the trust cache as `CS_VALID` and as a platform binary.
- **Two kinds of entitlements, as on the reference platform.** Restricted
  entitlements (private ones and hardware access, such as
  `com.apple.hid.system.server-access`) are honored only for platform
  binaries. Unrestricted ones (hardened runtime, sandbox) are honored with any
  valid signature, ad hoc included. Binaries outside the trust cache still
  run, unless they carry a restricted entitlement: then the exec is refused,
  as on the reference platform (ADR 0001).
- **Names.** Entitlement names stay as the kernel and the frameworks check
  them.
- **Later.** Certificate-based signatures, verified with corecrypto, for what
  is signed outside the build and for third-party apps (Phase 7); the policy
  kext keeps a place for that check. A user-space validation daemon only if
  provisioning profiles or expensive checks need it.
- **Milestone.** Its own ADR first (`Docs/ADR/0001-Kernel-Entitlements.md`,
  accepted 2026-11-01), then an implementation before WS4a, with tests: a
  system binary in the trust cache gets a restricted entitlement; a re-signed
  copy outside the cache is refused; an ad-hoc signed binary keeps its
  unrestricted entitlements. The root shortcut in `IOHIDUserClient.cpp`
  (`SHORTCUT(WS4a)`) is removed then. Done on 2026-11-01 (`57c6d5734b`, `48ee7a4288`, `24412d4182`, `c1cda1f1ae`, `f77748bbb7`, `8366b8d0f6`):
  see the end of the ADR.

Every shortcut is marked in the code as section 8 says, with the milestone
that removes it, so each milestone's shortcuts can be listed with
`git grep`.

## 10. Restructuring around WindowServer

The rewrite also changes what surrounds WindowServer: what lives in its
directory, the messages it relays between apps, and who manages the session.
Each part follows the reference platform's layout.

### 10.1 Repository layout (WS1)

| Today | After |
| --- | --- |
| `CoreServices/WindowServer/` holds the server and LoginWindow, SystemUIServer and LoadingWindow | The new server in `PrivateFrameworks/SkyLight/` (server installed in `SkyLight.framework/Resources`, client library in the framework); the current server in `CoreServices/WindowServer/Legacy/` until WS4b |
| LoginWindow, SystemUIServer, LoadingWindow inside the server's directory | Apps of their own in `CoreServices/`: `loginwindow.app`, `SystemUIServer.app`, next to `Dock.app` and the file manager; LoadingWindow becomes part of loginwindow |
| libinput, libevdev and mtdev trees | Removed in WS4b |

### 10.2 Messages WindowServer relays today (WS4a and WS4b)

`message.h` carries messages that are not a window server's job; AppKit
(`NSApplication.m`) and SystemUIServer (`AppDelegate.m`) use them. Each moves
to where the reference platform keeps it:

| Today, through WindowServer | After |
| --- | --- |
| The app sends its serialized menu to SystemUIServer (`CODE_MENU_FOR_APP`), which draws it and sends clicks back (`CODE_ITEM_CLICKED`) | Each app's AppKit draws its own menu bar and menus, in windows at `kCGMainMenuWindowLevel` and the menu levels, while it is the front app |
| Status items sent to SystemUIServer (`CODE_ADD_STATUS_ITEM`, `CODE_STATUS_ITEM_ADDED`) | Each `NSStatusItem` is a window of its own app at `kCGStatusWindowLevel`; WindowServer only lays out the status area. SystemUIServer hosts the system's Menu Extras |
| App launched or exited, forwarded to the Dock and SystemUIServer (`CODE_APP_LAUNCHED`, `CODE_APP_EXITED`) | LaunchServices' session service and the `NSWorkspace` notifications (`NSWorkspaceDidLaunchApplicationNotification`, `NSWorkspaceDidTerminateApplicationNotification`, `NSWorkspaceDidActivateApplicationNotification`), from WS4a (section 5) |
| Recent items (`CODE_ADD_RECENT_ITEM`) | LaunchServices' shared file lists, through `NSDocumentController` |

The menu, status item and recent item rows are planned in detail in
[Menus.md](Menus.md).

WindowServer keeps only what is its own: the front app, activation order,
key and main windows, and the menu bar's place on screen. In WS4b the
relay codes are removed from `message.h`, and `message.h` itself goes away
with the legacy server.

### 10.3 Session lifecycle (WS4b, completed in Phase 5)

Today restart, shut down and log out are exit codes: SystemUIServer exits
with `EXIT_RESTART`, `EXIT_SHUTDOWN` or `EXIT_LOGOUT` (`common.h`,
`SystemUIServer/MenuView.m`), and WindowServer reacts to them; shutting down
only logs "should shut down!".

As on the reference platform, the login window manages the session:

- launchd starts `loginwindow` on the console; it shows the login window
  and, after login, starts the user's session (the user's launchd domain
  and `LaunchAgents`).
- Restart, shut down and log out are requests to `loginwindow` (the reference sends them as scripting events; here a Mach message first). It asks each app
  to quit (`applicationShouldTerminate:`), waits, ends the session and then
  calls `reboot(2)` with the matching flags.
- WindowServer is told nothing beyond its clients going away.

Phase 4 only needs the minimal `loginwindow` of section 9 (session end and
`reboot(2)`), marked `// SHORTCUT(Phase 5):`, the milestone that removes
it; the full login window is Phase 5.

The system menu that sends these requests is planned in [Menus.md](Menus.md)
(MB0 and MB3).
