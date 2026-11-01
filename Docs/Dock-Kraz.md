# Dock and Kraz plan

Status: **accepted** (2026-10-31). Phase 5 of [ROADMAP.md](ROADMAP.md).
Spanish version: [Dock-Kraz.es.md](Dock-Kraz.es.md).
Checked against `devel` at `8dab4060c`.

This document is the decision record and the work plan for the Dock and for
Kraz, the file manager that replaces Filer. It follows the same rules as
[WindowServer-Rewrite.md](WindowServer-Rewrite.md).

## 1. Decision

- **Kraz is written new.** Nothing of Filer is kept.
- **The Dock gets a new core**, and three parts of the current one are
  ported: the preference keys and tile data format, the desktop picture
  window, and the tile layout arithmetic as a reference.

**Guiding rule**, as for WindowServer: where the reference platform has a
known design (which process does what, preference keys, public APIs), the
Dock and Kraz follow it. A shortcut is allowed during development only when
section 10 lists it with the milestone that removes it.

## 2. Why: findings in the current code

### Filer (`CoreServices/Filer/FilerMain.m`, 94 lines)

A window titled "Filer - /Applications" with no view in it. Files dropped on
it are copied into the folder, deleting what was there first without asking;
it can only open `.app` bundles (`LSOpenCFURLRef`). There is no browsing,
icon, list or column view, Trash or "Open With". Nothing to keep.

### Dock (`CoreServices/Dock/`, about 1,300 lines)

1. **Tied to the legacy window server.** It activates apps with
   `_windowServerRPC(kWSApplicationActivate)` and structs from
   `WindowServer/Legacy/rpc.h`, and listens to notifications the legacy
   server relays (`NSApplicationDidLaunch`, `NSApplicationDidQuit`,
   `WSWindowDidChangeState`). Section 10.2 of the WindowServer plan removes
   those relays.
2. **Does not build.** `main.m` uses `NSAutoreleasePool new`/`drain` under
   `-fobjc-arc`, and the Makefile points to old paths (`gnu/lib/libobjc2`,
   `${OBJTOP}/tmp`). `CoreServices/Makefile` lists it as not built.
3. **Endless redraw.** `-[DockView drawRect:]` ends with
   `[self setNeedsDisplay:YES]`, so it redraws forever and burns CPU. It also
   draws with `_frame` instead of `bounds`, and code calls `drawRect:`
   directly.
4. **Rebuilds its window on every change** (`relocate` closes it and makes a
   new one), which flickers.
5. **Model and view mixed.** `DockItem` is an `NSView` that holds paths,
   windows and flags.
6. **Linux leftovers:** `AppDir`/`AppRun` support.
7. **Missing behavior:** drag to reorder, add or remove; context menus;
   magnification; autohide (the key is read, never used); the app's
   `NSDockTile` badge, content and menu (no IPC); real thumbnails of
   minimized windows (a generic PNG today); stacks; a working Trash
   (`Trash.app` logs "does nothing yet").

Worth keeping:

- `DockTileData.m` and the preference keys (`persistent-apps`, `tilesize`,
  `autohide`...), which match the reference platform's Dock preferences.
- `DesktopWindow`, the desktop picture window at the desktop level. On the
  reference platform the Dock draws the desktop picture and the file manager
  draws the desktop icons.
- `fitWindowToItems` (fitting tiles to the screen on any side), as a
  reference.

## 3. Who does what (reference platform layout)

| Job | Process |
| --- | --- |
| Dock tiles, running-app indicators, minimized windows, stacks, Trash tile | Dock |
| Desktop picture, one window per display at `kCGDesktopWindowLevel` | Dock |
| App switcher (Command-Tab) | Dock, which gets Command-Tab from the window server's symbolic hotkey table (today the legacy WindowServer draws one in `WSInput.m`; it goes away with `Legacy/` in WS4b) |
| File browser windows, desktop icons at `kCGDesktopIconWindowLevel`, Trash contents and emptying, "Open With", Get Info | Kraz |
| Starting the Dock and Kraz at login, and again if they exit | loginwindow (Phase 5; launchd until then) |

Spaces, an overview of windows and an app launcher grid are out of scope for
this plan; the Dock's model keeps room for them.

## 4. Dock architecture

- **Model.** `DockModel` with sections: persistent apps, recent and running
  apps, persistent others (folders and stacks), the Downloads stack, Trash.
  `DockTile` is plain data (URL, bundle ID, kind, flags, badge, windows).
  Views only display the model.
- **Views.** One Dock window per display that shows it, laid out by the model;
  tiles as Core Animation layers (bounce, magnification, autohide slide), so
  nothing is redrawn while nothing changes. The look follows the Gienah
  visual language guide.
- **Data sources.** `NSWorkspace` notifications
  (`NSWorkspaceDidLaunchApplicationNotification`,
  `NSWorkspaceDidTerminateApplicationNotification`,
  `NSWorkspaceDidActivateApplicationNotification`, hide and unhide) and
  `NSRunningApplication`; icons through LaunchServices and
  `-[NSWorkspace iconForFile:]`; activation with
  `-[NSRunningApplication activateWithOptions:]`.
- **Minimized windows.** As on the reference platform, the Dock owns them:
  AppKit's `miniaturize:` asks the Dock, which runs the animation
  (`mineffect`) and keeps the tile. The window server (WS4a) orders the
  window out and back in, tells the Dock when a window is minimized or
  restored, and gives it the window's image through
  `CGWindowListCreateImage` with a single window ID (the Dock is a system
  client with that permission). Restoring asks the window server to order
  the window in and activates its app.
- **Dock tile service.** A Mach service, `org.minkar.dock.server`, defined in
  MIG, for what apps send through `NSDockTile` (badge label, content view
  snapshot, application badge), `-[NSApplication requestUserAttention:]`
  (bounce) and `applicationDockMenu:` (the app's dock menu).
- **Preferences.** Domain `org.minkar.dock`, with the reference platform's
  keys and meanings: `persistent-apps`, `persistent-others`, `tilesize`,
  `magnification`, `largesize`, `orientation` (`bottom`, `left`, `right`;
  the current code's `Location` integer becomes this key), `autohide`,
  `autohide-delay`, `autohide-time-modifier`, `mineffect`, `show-recents`.
  Tile data in the reference format (`tile-data`, `file-data`,
  `_CFURLString`).
- **Desktop pictures.** `DesktopWindow` ported, one per display keyed by the
  display ID (`NSScreenNumber` in `-[NSScreen deviceDescription]`), not the
  fixed `"NSMainScreen"` key. `-[NSWorkspace setDesktopImageURL:forScreen:options:error:]`
  stores the choice and the Dock follows it.
- **App switcher.** The window server's symbolic hotkey table (WS4a) sends
  Command-Tab to the Dock instead of delivering it as a key event; the Dock
  shows the switcher and activates the chosen app. The legacy server's
  switcher (`WSInput.m`) goes away with `Legacy/` in WS4b, so until D4 there
  is none.

## 5. Kraz architecture

- **App.** Bundle ID `org.minkar.Kraz`, installed as
  `/System/Library/CoreServices/Kraz.app`; always running in a session
  (loginwindow starts it and starts it again if it exits).
- **Windows.** Toolbar, sidebar (favorites; volumes from DiskArbitration;
  locations), path bar, and three views: icons (`NSCollectionView`), list
  (`NSOutlineView`) and columns (`NSBrowser`). Navigation history, sorting,
  per-folder view settings.
- **Opening.** Through LaunchServices: default app per UTI, "Open With"
  (`LSCopyApplicationURLsForURL`), apps by bundle.
- **File operations.** Copy, move, duplicate, rename, make folder, alias;
  with progress, cancel and undo; on `NSFileManager`, run off the main
  thread.
- **Trash.** `~/.Trash` and `/.Trashes/<uid>` on other volumes, as on the
  reference platform; "Put Back" keeps each item's original location; empty
  Trash. `-[NSWorkspace recycleURLs:completionHandler:]` goes through the
  same code.
- **Desktop.** A window per display at `kCGDesktopIconWindowLevel` with the
  contents of `~/Desktop` and the mounted volumes (with eject).
- **Live updates.** Folder windows follow changes through file system
  events (see dependencies).
- **Reveal.** `open -R`, `-[NSWorkspace activateFileViewerSelectingURLs:]`
  and `selectFile:inFileViewerRootedAtPath:` open a Kraz window with the
  item selected.
- **Later.** Search on the metadata index (`mdfind`, Phase 5), double-click
  on a DMG (Phase 6), tags, previews.

## 6. Dependencies

| Needed by | Dependency | State today | Where |
| --- | --- | --- | --- |
| Dock | `NSWorkspace` launch, terminate and activate notifications and the basic `NSRunningApplication` | Missing (only the old `launchedApplications`) | LaunchServices session service, WindowServer plan WS4a |
| Dock | The rest of `NSRunningApplication` (icon, launch date, hide and unhide, `activationPolicy`, terminate) | Missing | AppKit, D0 |
| Dock | Window server side of minimizing (order out and in, notices to the Dock) and single-window capture | Missing | WindowServer plan, WS4a |
| Dock | Relayed messages removed, `kWSApplicationActivate` gone | Planned | WindowServer plan, section 10.2 (WS4b) |
| Dock | `NSDockTile` sending its state | Keeps values, sends nothing | AppKit, D3 |
| Dock | `setDesktopImageURL:forScreen:options:error:` | To check | AppKit, D1 |
| Kraz | LaunchServices bundle and UTI database, default apps, "Open With" | Phase 3, item 4 | Frameworks |
| Kraz | File system events: XNU's `/dev/fsevents` exists (`vfs_fsevents.c`); a daemon and the FSEvents API are missing | Missing | CoreServices, before K1 |
| Kraz | `NSCollectionView`, `NSOutlineView`, `NSBrowser` working well | Present, partly stubbed | AppKit, with K1 |
| Kraz | `NSWorkspace` icons, recycle, reveal | Partial | AppKit, K1-K2 |
| Both | loginwindow starting them at login | Phase 5 | loginwindow |
| Both | The Gienah visual language guide | Missing | Design, before D2 and K1 |

## 7. Milestones

Each milestone ends with tests that pass, `boottest GATE=M2` (and, once it
exists, gate M4) still green, and the roadmap updated with the evidence.

### Dock

- [ ] **D0 — Prerequisites.** The rest of `NSRunningApplication` (icon, launch
  date, hide and unhide, `activationPolicy`, terminate) on the list that
  LaunchServices' session service keeps from WS4a, and the desktop picture
  API. The launch, terminate and activate notifications and the basic
  `NSRunningApplication` come with WS4a. Tests: a test app's icon and launch
  date are read; hiding, unhiding and terminating it through
  `NSRunningApplication` work; the desktop picture choice is stored.
- [ ] **D1 — New core.** `DockModel`, preferences in the reference keys,
  persistent and running app tiles, launch and activate, running indicators,
  desktop pictures per display. The old code goes: `Trash.app`,
  `Downloads.app`, `AppDir`, the relayed notifications. Tests: model tests;
  launching an app shows its tile and indicator, quitting removes it; the
  preferences round-trip; `screendump` shows the Dock and the desktop
  picture; the Dock uses no CPU while idle.
- [ ] **D2 — Interaction.** Drag to reorder, add and remove; context menus
  (Keep in Dock, Open at Login, Show in Kraz, Quit) and the app's dock menu;
  autohide, magnification, orientation; the minimize and restore animation
  (`mineffect`) for `miniaturize:`.
- [ ] **D3 — Dock tile service and minimized windows.** `org.minkar.dock.server`:
  badges, content, attention bounce; minimized windows with thumbnails
  (after WS4a).
- [ ] **D4 — Stacks, Trash and the app switcher.** Downloads and folder stacks;
  Trash tile (empty or full, drop to trash, empty through Kraz); the app
  switcher in the Dock, which gets Command-Tab from the window server's
  symbolic hotkey table.

### Kraz

- [ ] **K0 — Prerequisites.** File system events daemon and API; LaunchServices
  open and "Open With"; the AppKit views checked with test apps.
- [ ] **K1 — Browsing.** Windows with sidebar, icon, list and column views,
  navigation, opening files and apps, basic Get Info. Filer becomes Kraz:
  bundle ID, `LSRevealInFiler` kept as a deprecated alias of the new name for
  one release, `open -R`, the Dock's default tiles. This is the "Kraz
  browses" part of gate M5.
- [ ] **K2 — File operations.** Copy, move, rename, duplicate with progress and
  undo; drag and drop between windows and onto the Dock; Trash with Put Back;
  "Open With".
- [ ] **K3 — Desktop and session.** Desktop icons and volumes with eject; reveal
  APIs; started and restarted by loginwindow.
- [ ] **K4 — Later work.** Search on the metadata index, DMG double-click
  (Phase 6), tags, previews.

Gate M5 ("a graphical session starts, an app opens from the Dock, Kraz
browses, and the resolution changes from System Preferences") needs D1 and
K1, besides loginwindow and System Preferences.

## 8. Tests and measurements

- Model tests (Dock sections and tile order, preference round-trip; Kraz
  file operations and Trash records) as command-line programs in the test
  suite.
- `boottest` with QMP: `screendump` comparisons and injected clicks (open an
  app from the Dock; navigate a folder in Kraz).
- Idle CPU of the Dock and of Kraz near zero; time from a Dock click to the
  app's first window, measured the same way in the VM and on the physical
  machines.

## 9. Rules for this work

Same as section 8 of the WindowServer plan: English in everything committed
except the `.es.md` translations; no third-party product names; API and
symbol names as they are; new code MIT or BSD-2-Clause; clean room; small
commits with the diff shown first; evidence for every roadmap item; no
`Co-Authored-By` lines.

## 10. Decisions and development shortcuts

| Question | Decision (target) | Development shortcut | Removed by |
| --- | --- | --- | --- |
| Dock preferences | Domain `org.minkar.dock`, reference keys and tile data format | — | — |
| Who draws the desktop | Dock: pictures; Kraz: icons | Desktop without icons until K3 | K3 |
| App switcher | Dock, Command-Tab from the window server's hotkey table | Legacy window server (`WSInput.m`) until WS4b; then none: apps are switched by clicking their windows (`SHORTCUT(D4)`) | D4 |
| Minimized windows | Thumbnails from the window server's capture | Generic window icon | D3 (after WS4a) |
| Starting the Dock and Kraz | loginwindow, restarting them if they exit | launchd agents | Phase 5 (loginwindow) |
| Look | Gienah visual language guide | Plain system controls and colors | Guide available |

Every shortcut is marked in the code with `// SHORTCUT(Dn):` or
`// SHORTCUT(Kn):` and the milestone that removes it.
