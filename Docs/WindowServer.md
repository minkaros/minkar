# WindowServer

How Minkar's window server works today: its process, protocol, compositing and input,
how it is tested, and the shortcuts and milestones still open. For anyone changing
SkyLight, CoreGraphics' window and display services, or AppKit's side of them. Last
reviewed: 2026-11-27. Part of Phase 4 of [ROADMAP.md](ROADMAP.md).

Where the reference platform has a known design (process layout, user, permissions,
window movement), the window server follows it. The protocol between AppKit and the
server is private on the reference platform, so Minkar's is its own; compatibility is the
public API (Quartz Display, Window and Event Services, `CGDisplayStream`, `NSWindow`,
`NSScreen`, `NSCursor`) and its behavior. Out of scope: the reference platform's private
window server interface, Spaces, HDR.

## Process

- One daemon, `/System/Library/PrivateFrameworks/SkyLight.framework/Resources/WindowServer`,
  started at boot by launchd (`org.minkar.WindowServer.plist`) as `_windowserver`
  (UID 88). It only serves; it launches nothing. `-textconsole` at boot keeps the text
  console instead.
- `loginwindow` owns the session: it starts the session's apps (the Dock and Files,
  again if they exit) and the user's login items, and ends the session (Log Out, Restart, Shut Down) with
  `reboot(2)`. The window server is told nothing beyond its clients going away.
- The kernel grants the framebuffer and IOHIDSystem's server connection only to a
  process with the `com.apple.hid.system.server-access` entitlement from the static trust
  cache ([ADR 0001](ADR/0001-Kernel-Entitlements.md)).

## Protocol and client side

- `PrivateFrameworks/SkyLight/IPC/WindowServer.defs` (MIG) carries connections, windows,
  surfaces, displays, events, cursors, capture, the menu bar, hotkeys and measurements.
  Messages carry only their data; large data travels as out-of-line memory or memory
  entries. Both sides check `kSLSInterfaceVersion` (27 today).
- The client is `SkyLight.framework`, re-exported by CoreGraphics, so the private `CGS*`
  and `SLS*` functions third-party apps call resolve where they expect them. That subset
  comes from public information and observed behavior only.
- CoreGraphics (`CGDirectDisplay`, `CGWindowList`, `CGEvent`) and AppKit (`NSApplication`,
  `NSWindow`, `NSScreen`, `NSEvent`, `NSCursor`) speak only to it.
- Every request is checked against the caller's audit token; a client's exit releases
  what it held.

## Model

`Server/WSConnection` (PID, UID and audit token of each client), `WSWindowList` (each
window's server-assigned `CGWindowID`, owner, level, order, frame, alpha, shadow, parent
and children, collection behavior, title, sharing state; ordering within and across
levels; child windows and sheets; minimized windows). The model changes only on its
queue; the render path works on a snapshot taken at frame start, so it never holds a lock
while drawing.

The server also keeps the front app (which may have no window), the activation order,
key and main windows, the menu bar's place on each display, the status area's layout,
drag regions, cursor rects and the symbolic hotkey table (`WSHotKeys`).

## Surfaces and compositing

- A window's backing store is a `WSSurface`: two buffers, a Mach memory entry or an
  IOSurface (`iosurfaced` keeps the registry). The client draws into the back buffer and
  commits it with its damage; AppKit sends only the rects its views drew.
- `WSCompositor` draws through a backend interface (`WSCompositorBackend.h`):
  `WSPixmanBackend` on the CPU (VMs and bring-up) and `WSVulkanBackend` on the GPU (a
  compute shader into a frame in VRAM, shown by page flips at the vertical blank).
  `WSCompositor=pixman|vulkan` picks one.
- Only damaged regions are composited and presented. A display link per display paces
  frames and sleeps after two idle ticks; damage wakes it. The cursor is drawn last, with
  its own damage.
- `CAMetalLayer`'s drawables are IOSurfaces in VRAM composited where they are, so Vulkan
  apps present at the display's rate without CPU copies ([GPU.md](GPU.md)).
- A window's layers (`SLSWindowSetLayers`): a view whose layer tree is only IOSurfaces
  and clipping bounds (Firefox's tiles and video) gives the server its surfaces, drawn
  under the window's pixels at every frame, one to one or scaled (bilinear) and flipped.
  BGRA surfaces as they are; 4:2:0 YCbCr ones (`420v`, `420f`) converted with the matrix
  their `IOSurfaceYCbCrMatrix` names. Under them goes the window's background color, or
  the material of the visual effect or glass view they are in; a rounded window cuts them
  to its shape. The app draws nothing for a new frame of its surfaces.
- Displays (`WSDisplay`): the GOP framebuffer (`IOGOPFramebuffer`), `MinkarVirtioGPU`
  (each frame's rects transferred and flushed) and `MinkarAMDFramebuffer`. Modes, gamma
  and transfer tables, fades, mirroring and arrangement across several displays; permanent
  configurations in `/var/db/WindowServer/Displays.plist`.
- Capture: `CGDisplayCreateImage`, `CGWindowListCreateImage`, display capture (the
  client gets a surface) and `CGDisplayStream` (frames in IOSurfaces; dirty rects only,
  moved rects are not reported). The window list gives each window's title
  (`kCGWindowName`) as its owner set it (`SLSSetWindowTitle`).

## Input

- `WSInput` reads IOHIDSystem (`kIOHIDServerConnectType`, the shared `EvGlobals` queue,
  a Mach notification port) on its own high-priority queue: cursor position and
  acceleration, modifiers, key repeat, click counts, hit testing.
- Keys go to the key window's owner; mouse events to the window under the pointer, or to
  the one that got the mouse down until the mouse up. Each client has a bounded queue,
  mouse moves are coalesced, and the server never blocks on a client.
- Event taps (`WSTaps`: `CGEventTapCreate`, `CGEventPost`, `CGGetEventTapList`) and
  `CGEventSource` state.
- Windows move server-side from the drag regions each window declares (the title bar,
  and the background with `movableByWindowBackground`) and from
  `performWindowDragWithEvent:`, so a window follows the pointer while its app is busy.
- Reserved shortcuts go from the hotkey table to their owner: Command-Tab to the Dock,
  Command-Option-Escape to loginwindow, Control-F2 to the front app.
- Drags between apps: `SLSPostDragEvent` delivers entered, updated, exited and perform
  events to the window's owner; the data is on the shared drag pasteboard.

## Measurements and debugging

`wsctl` lists windows and connections, changes modes and shows `wsctl stats`: frame
time, frames presented and late, damaged area, and input-to-present latency (from the HID
event's time to the present that answers it), the same way in the VM and on the physical
machines. The figures are in [GPU.md](GPU.md#measurements).

## Tests

- The server's local tests and `skylight-test` (model, ordering, levels, damage) as
  command-line programs; `eventtap-test`.
- Gate `M4` (`boottest GATE=M4`, 54 markers): AppKit windows drawn, composited, clicked,
  typed into and dragged, through QMP input and `screendump` comparisons with a
  per-pixel tolerance; on the standard VGA and virtio-vga.
- Local image tests such as `multi-display`, `display-reconfigure`, `hotkeys`,
  `pointer-capture` and `cursors`.

## Open milestones

- [ ] **WS6 — GPU compositing.** Done on PC 2: the Vulkan backend, copy-free
  `CAMetalLayer` presentation, page flips (GPU track G4). Left: the compositing
  performance gate on a reference PC (the display's refresh rate with several animated
  windows, WindowServer CPU no higher than the reference), part of a release after 25.x.
- Later: whole layer trees committed by clients (opacity, transforms, animations) and
  composited by the server (its own ADR), Spaces, a hardware cursor.

WS0–WS5 are done (`cce7da92bc`, `030dba37c7`, `8512fe60ba`, `98237bf14e`, `e31dd0ef53`,
`9d510ec6c2`, `9a472063c1`).

## Development shortcuts still open

Each is marked in the code as `// SHORTCUT(<removed by>):`; a milestone closes only when
`git grep -n 'SHORTCUT(<milestone>)'` finds nothing.

| Question | Decision (target) | Development shortcut | Removed by |
| --- | --- | --- | --- |
| Capturing and posting events | Screen recording, input monitoring and accessibility permissions, through a permission service with its own ADR | A built-in policy: root and the console user's processes are allowed (taps, other apps' window images, display streams); everything else denied | M5 |
| Keyboard layouts | Hardware key codes from the server; the client turns them into characters (`UCKeyTranslate`-style, Text Input Sources) | `WSKeymap` and libxkbcommon in SkyLight | Phase 5 |
| Ending the session | The login window asks apps to quit, ends the session and restarts or shuts down | A minimal `loginwindow` without a login window or confirmation panel | Phase 5 (LoginWindow) |
| System font | Atlas's own font | Inter (SIL OFL 1.1) | Phase 5 (Atlas) |
| Standard cursors | Atlas's cursor set | Plain shapes drawn in `NSCursorImage.m` | Phase 5 (Atlas) |

The shortcuts of the menus, the Dock and Files are in [Atlas.md](Atlas.md).
