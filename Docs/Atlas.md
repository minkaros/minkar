# Atlas plan

Status: **accepted** (2026-11-05). Phases 5 and 6 of [ROADMAP.md](ROADMAP.md).
Spanish version: [Atlas.es.md](Atlas.es.md).

Atlas is Minkar's desktop: the session (login, menu bar, Dock, Files,
System Preferences) and the applications a person needs every day. This
document is the single decision record and work plan for all of it. It
takes in the former menus plan (chapter 3, milestones MB0–MB4) and Dock
and Files plan (chapter 4, D0–D4 and K0–K4), whose milestones keep their
names, and adds the applications (chapter 5, A0–A10). It follows the same
rules as [WindowServer-Rewrite.md](WindowServer-Rewrite.md).

## 1. Scope and rules

- **Everything is a Cocoa app or bundle.** The session and the apps are
  written against the public Cocoa API, so they double as a continuous
  test of AppKit and Foundation; what one needs that the frameworks lack is
  added to the frameworks, never worked around in the app.
- **Our own visual language.** No third-party fonts, sounds or artwork,
  with one exception for icons (decided 2026-11-05): the icon themes
  Faience and Faenza by Matthieu James (GPL-3), kept as separate data files
  with their license, authors and SVG sources in `SystemLibrary/Icons`
  (installed in `/System/Library/Icons`), and without brand icons. They
  keep their GPL-3 license; no code links with them, and the rest of the
  rule stands (accepted by the maintainer: no icon set of the same scope
  exists under BSD or MIT). Folders
  are blue by default (Faience-Azur), with sand and orange as
  alternatives. App icons are 512×512 PNG in the bundle (AppKit reads no
  `.icns` yet). The menu bar is dark (charcoal, light text). The interface
  font is Inter and the fixed-pitch font Agave (both SIL OFL 1.1).
- **Generic names.** The only proper names are Minkar, the system, and
  Atlas, its desktop (decided 2026-11-05). Every app is named for what it
  does, with the common words other free desktops use (Files, Text Editor,
  System Monitor, Logs), never with another project's own names nor with a
  reference-platform app's name that is not a common word. The bundle is
  named without spaces (`TextEditor.app`), its identifier is
  `org.minkar.<Name>`, and the Spanish name comes with the app's
  translation (Archivos, Editor de texto). Files and Text Editor were
  formerly named Argos and Alcor.
- **On the command-line tools.** Where Minkar has the command
  (`top`, `log`, `screencapture`, `diskutil`, `hdiutil`,
  `system_profiler`, `tar`), the app is its interface and shares its code
  or its output, so both behave alike.
- **English and Spanish.** Every app is localized in both; English is the
  language of development, tests and screenshots.
- **Places.** The session's parts in `/System/Library/CoreServices`, the
  apps in `/System/Applications`, utilities in
  `/System/Applications/Utilities`.

## 2. Status

| Part | Chapter | Milestones | Done | Next |
| --- | --- | --- | --- | --- |
| Menus | 3 | MB0–MB4 | MB0 | MB1 (partial), MB2 |
| Dock | 4 | D0–D4 | D0, D1 | D2 |
| Files | 4 | K0–K4 | K1 (K0 partial) | K0, K2 |
| Applications | 5 | A0–A10 | — (Text Editor started) | A0, A1 |

The session's other parts (LoginWindow, System Preferences, the visual
language) are items of Phase 5 in [ROADMAP.md](ROADMAP.md).

## 3. Menus

### 3.1 Decision

- **Menus move back into each app.** AppKit draws the app's menu bar and its
  menus and tracks them in the app's own process, as on the reference
  platform. The window server only reserves the menu bar's place on each
  display and says which app is in front.
- **The menu model stays.** `NSMenu` and `NSMenuItem` are kept; the drawing
  and tracking code in SystemUIServer (`NSMainMenuView`, `SubmenuView`,
  `NSMenuWindow`) moves into AppKit and is reworked there.
- **Status items are rewritten** as windows of their own app.
- **SystemUIServer only hosts the system's Menu Extras.**
- **The first milestone, MB0, is part of WS4b:** it is the menu work section
  10.2 assigns to the switch, and WS4b is not closed until MB0 passes. What
  MB0 needs from the window server comes in WS4a. The rest
  (MB1 to MB4) is Phase 5.

**Guiding rule**, as for WindowServer: where the reference platform has a
known design (which process does what, public APIs, standard menus), the
menus follow it. A shortcut is allowed during development only when section
3.10 lists it with the milestone that removes it.

### 3.2 Why: findings in the current code

1. **There is no menu bar today.** SystemUIServer, which draws it, is not
   built (`CoreServices/Makefile` had a FIXME: the Dock, the old file
   browser (formerly Filer), DMIHelper, SystemUIServer and loginwindow were
   not built yet).
2. **Menus drawn in another process.** `-[NSApplication
   sendMenusToWindowServer]` copies the whole main menu, strips targets and
   delegates (`_menuEnumerateAndChange:`), archives it with
   `NSKeyedArchiver` and sends it (`CODE_MENU_FOR_APP`) through the legacy
   server (`WindowServer/Legacy/WindowServer.m`) to SystemUIServer, which
   unarchives and draws it (`AppDelegate.m`, `NSMainMenuView.m`,
   `SubmenuView.m`). A click comes back as a tag (`CODE_ITEM_CLICKED`) and
   the app looks the item up by tag. The legacy server goes away in WS4b, and
   this path with it.
3. **The app's tags are overwritten.** Before sending, `_tagAllMenus` sets
   every item's tag to an internal ID, so apps that use `tag`,
   `itemWithTag:` or `indexOfItemWithTag:` break.
4. **Validation and dynamic menus cannot work.** SystemUIServer holds a copy
   without targets or delegates, so nothing can ask the app at the moment a
   menu opens: `validateMenuItem:`, `validateUserInterfaceItem:`,
   `menuNeedsUpdate:`, `menuWillOpen:`, alternates with Option, titles that
   change. Every change sends a new full copy (`sendMenusToWindowServer` is
   called from six places in `NSApplication.m`).
5. **Size limit.** The archive is cut to the 128 KB message
   (`MIN([d length], sizeof(msg.data))`, `data[128*1024]` in `message.h`),
   so a large menu arrives as a broken archive.
6. **Context menus are not implemented.** `+[NSMenu popUpContextMenu:
   withEvent:forView:]` is `#if 0` with `NSUnimplementedMethod()`.
7. **Status items are Windows code.** `NSStatusBar.m` and `NSStatusItem.m`
   import `<windows.h>` and `Win32Window` (tray icons of another platform);
   `-[NSApplication _addStatusItem:]` archives the item and sends it to
   SystemUIServer (`CODE_ADD_STATUS_ITEM`).
8. **Leftovers.** D-Bus menu IDs in `NSMenu` and `NSMenuItem`
   (`DBusItemID`).
9. **The system menu** lives in SystemUIServer (`MenuView.m`): Sleep and
   Force Quit only log "not implemented yet"; System Preferences logs the
   same message but then opens `System Preferences.app` with
   `LSOpenCFURLRef`; Restart, Shut Down and Log Out end in the exit codes
   that section 10.3 of the WindowServer plan removes.

Worth keeping:

- `NSMenu` and `NSMenuItem` as the model: items, submenus, autoenabling,
  `update`, `performKeyEquivalent:` with validation, delegate.
- `NSMainMenuView`, `SubmenuView` and `NSMenuWindow`: drawing, overflow,
  submenu placement and `trackForEvent:`, as the starting point of AppKit's
  menu views.
- `ClockView` and `ExtrasView` in SystemUIServer, as the first Menu Extras.

### 3.3 Who does what (reference platform layout)

| Job | Who |
| --- | --- |
| The menu bar's place on each display, its height, the front app, the status area layout | Window server |
| The front app's menu bar: system menu, app menu, the app's menus, drawn in a window at `kCGMainMenuWindowLevel` | The app (AppKit) |
| Opening and tracking menus, submenus and context menus, in windows at `kCGPopUpMenuWindowLevel` | The app (AppKit), in its own process |
| Status items (`NSStatusItem`), each a window at `kCGStatusWindowLevel` | The app that owns them |
| System Menu Extras (clock, volume, network, displays...), as loadable bundles | SystemUIServer |
| From API 20: the system status cluster (network, sound, battery, displays...) and its popovers | A separate control-center app; SystemUIServer keeps search, notifications and the older Menu Extras |
| What the system menu's items do (restart, shut down, log out, sleep, force quit) | loginwindow |
| The Services menu's contents | The services daemon (pasteboard server) |

Each part of the bar is its own borderless window, and the window server
composites them into one strip. When the front app changes, the window
server orders out the old app's menu bar window and orders in the new one's;
status item windows stay where they are. Files is always running, so it is
the front app whenever no other app is.

Because menus run in the app's process, a hung app cannot open its menus,
as on the reference platform; the window server shows the busy cursor over
it, and Force Quit (loginwindow) remains available.

### 3.4 Behavior to follow

#### Menu bar

- One menu bar per display, each showing the front app's menus; the app
  menu in bold with the app's name.
- The system menu first, with the Minkar logo (no third-party artwork):
  About This Computer, System Preferences…, Recent Items, Force Quit…,
  Sleep, Restart…, Shut Down…, Lock Screen, Log Out *user*…. Its contents
  come from the system, the same in every app.
- The app menu's standard items: About *App*, Preferences…, Services,
  Hide *App*, Hide Others, Show All, Quit *App*; the app can add its own.
- Apps that do not use AppKit, or that do not set a main menu, still get
  the system menu and an app menu with Quit.
- The app's menus, then the status area on the right; when space runs out
  the app's menus are cut from the end, never overlapping the status area.
- Hidden in full screen and shown when the pointer reaches the top; optional
  auto-hide.
- `-[NSScreen visibleFrame]` excludes the menu bar on every display that
  shows one.

#### Tracking

- Press, drag and release selects an item; a click opens a menu that stays
  open until another click.
- Keyboard: arrows, Return, Escape, type-select, Control-F2 moves focus to
  the menu bar.
- Submenus open after a short delay and stay open while the pointer moves
  towards them.
- Long menus scroll, with arrows at the edges.
- Holding Option shows alternate items (`isAlternate`).
- `NSMenuDelegate`: `menuNeedsUpdate:`, `numberOfItemsInMenu:`,
  `menu:updateItem:atIndex:shouldCancel:`, `menuWillOpen:`,
  `menuDidClose:`, `menu:willHighlightItem:`.
- Validation runs when a menu opens and before a key equivalent fires:
  the item's target, or the responder chain for a nil target, through
  `validateMenuItem:` or `validateUserInterfaceItem:`.

#### Key equivalents

- In the documented order: the key window and its views
  (`performKeyEquivalent:`), then the main menu, each item validated first.
- The menu shows key equivalents with the modifier glyphs.
- Shortcuts reserved by the system are recognized by the window server and
  sent to their owner, not delivered as key events: Command-Tab to the Dock,
  Command-Option-Escape to loginwindow, Control-F2 to the front app's
  AppKit, which moves focus to the menu bar.

#### Standard menus

- **Window menu** (`windowsMenu`): every window listed automatically,
  Minimize, Zoom, Bring All to Front.
- **Open Recent**: `NSDocumentController` with LaunchServices' shared file
  lists (section 10.2 of the WindowServer plan).
- **Help menu** (`helpMenu`): a search field that finds menu items (and
  help books, later).
- **Services menu** (`servicesMenu`): built from the `NSServices` entries in
  apps' `Info.plist` files by the services daemon, sent through the
  pasteboard.
- **View menu**: "Enter Full Screen" added for windows that support it.

#### Other menus

- Context menus: `menuForEvent:`, Control-click,
  `+popUpContextMenu:withEvent:forView:`,
  `-popUpMenuPositioningItem:atLocation:inView:`.
- Pop-up buttons (`NSPopUpButton`) with the same menu windows and tracking.
- Dock menus (`applicationDockMenu:`) through the Dock tile service of
  chapter 4 (D2).

#### Status items

- `-[NSStatusBar statusItemWithLength:]` with `NSVariableStatusItemLength`
  and `NSSquareStatusItemLength`; `button` (`NSStatusBarButton`), `menu`,
  `length`, `visible`, `autosaveName`, `behavior`.
- Each item is a window of its app at `kCGStatusWindowLevel`; the window
  server places them right to left, with the system Menu Extras; Command-drag
  reorders them and the order is kept.

#### Accessibility

Menu bar, menus and items expose their accessibility roles, so assistive
tools and the accessibility API can read and press them.

### 3.5 Architecture

- **AppKit:**
  - `NSMenuBarWindow`: one per display at `kCGMainMenuWindowLevel`, shown
    while the app is in front, with an `NSMenuBarView` (from
    `NSMainMenuView`).
  - `NSMenuWindow` and `NSMenuView` (from `SubmenuView`) for menus and
    submenus at `kCGPopUpMenuWindowLevel`.
  - The tracking loop in the app, in `NSEventTrackingRunLoopMode`, with the
    pointer captured by the window server while a menu is open.
  - The system menu built from a shared definition in a system framework;
    its actions are requests to loginwindow (section 10.3 of the WindowServer
    plan) and to the System Preferences app.
  - `NSStatusBar` and `NSStatusItem` rewritten for this platform.
  - Removed: `-[NSApplication sendMenusToWindowServer]`, `_tagAllMenus`,
    `_menuEnumerateAndChange:`, `_addStatusItem:`, `addRecentItem:` through
    the window server, the handling of `CODE_MENU_FOR_APP`,
    `CODE_ITEM_CLICKED`, `CODE_ADD_STATUS_ITEM`, `CODE_STATUS_ITEM_ADDED`
    and `CODE_ADD_RECENT_ITEM`, and `DBusItemID`.
- **Window server (SkyLight):** reserves the menu bar's place per display and
  publishes its height; tells each app when it becomes or stops being the
  front app; lays out the status area (status item windows and Menu Extras)
  and keeps the order; handles the reserved shortcuts; captures the pointer
  for an open menu. This is the part section 10.2 keeps in the window server.
- **SystemUIServer:** a host for Menu Extra bundles (`.menu`) in the status
  area; the menu drawing code and the `CODE_*` handling are removed. Built
  with the current build rules.
- **Services daemon:** builds the services list from `NSServices`, runs a
  service through the pasteboard (`NSPerformService`,
  `NSUpdateDynamicServices`).

### 3.6 Dependencies

| Needed by | Dependency | Where |
| --- | --- | --- |
| MB0 | Front app, activation order, the menu bar's place, status area layout, pointer capture | WindowServer plan, WS4a (section 10.2) |
| MB0 | Minimal `loginwindow` taking restart, shut down and log out requests | WindowServer plan, section 10.3 (WS4b) |
| MB0 | Symbolic hotkey table with delivery to owners (Control-F2 to the front app) | WindowServer plan, WS4a |
| MB1 | SystemUIServer on the current build rules | `CoreServices/Makefile` FIXME |
| MB2 | LaunchServices' shared file lists, `NSWorkspace` | Phase 3 (LaunchServices); WindowServer plan 10.2 |
| MB3 | The full LoginWindow (sleep, force quit, lock screen) | Phase 5, item 1 |
| MB3 | System Preferences app | Phase 5, item 6 |
| MB4 | Services daemon | Phase 5 |
| All | The Atlas visual language (menu fonts, metrics, colors) | Phase 5, item 7 |
| Dock menus | Dock tile service | Chapter 4, D2 |

### 3.7 Milestones

Each milestone ends with tests that pass, the existing gates still green and
the roadmap updated with the evidence. Milestones are named MB*n* to keep
them apart from the gates M*n*.

- [x] **MB0 — Menus in the app (part of WS4b; done 2026-11-02, gate M4, `9d510ec6c2`).** Menu views moved into AppKit;
  one menu bar window per display for the front app; tracking by click,
  drag and keyboard; submenus; validation and the delegate at open time;
  key equivalents in the documented order; context menus and pop-up
  buttons; the app's tags left alone. The system menu in AppKit with About
  This Computer, Restart, Shut Down and Log Out as requests to the minimal
  `loginwindow`. Status items as windows of their app with the basic API
  (`statusItemWithLength:`, `button`, `menu`, `length`). The old path
  removed from AppKit and SystemUIServer, so WS4b can drop the relay codes.
  Tests, added to gate M4: a QMP click opens the File menu and an item's
  action runs in the app; a disabled item (by validation) does nothing; a
  Command-key equivalent fires its item; a context menu opens on
  Control-click; an app that uses tags keeps them; switching apps swaps the
  menu bar; a test app's status item appears and opens its menu; Log Out
  ends the session; `screendump`.
- [ ] **MB1 — Menu Extras and the full status item API.** SystemUIServer built
  and hosting `.menu` bundles: clock, volume, displays, network,
  notifications (Phase 5, item 3); `visible`, `autosaveName`, `behavior`;
  Command-drag reorders and the order is kept. Tests: two apps and the
  clock appear in order and open their menus; the order survives a
  restart. Gate M5 needs MB1. *Partial (4 Nov 2026):* a new SystemUIServer
  with the clock, built in, which follows the 24-hour setting and the time
  zone at once (`0fa453aa16`, `61f9c7ae99`); the `.menu` bundles and the
  rest of the status item API are missing.
- [ ] **MB2 — Standard menus and details.** Window menu, Open Recent, Help menu
  search over menu items, alternates with Option, long menus scrolling,
  type-select, Control-F2, the View menu's full screen item, accessibility
  roles.
- [ ] **MB3 — The full system menu.** With the Phase 5 LoginWindow and System
  Preferences: Sleep, Force Quit window, Lock Screen, System Preferences…,
  Recent Items. Tests: Restart asks apps to quit before rebooting the VM;
  Force Quit ends a hung test app.
- [ ] **MB4 — Services and the menu bar's behavior.** Services daemon and menu;
  hiding in full screen; auto-hide.

Gate M6 (Minkar 19.6.0, a desktop for daily use) needs MB2 and MB3. MB4
comes in 19.6.x.

### 3.8 Tests and measurements

- Model tests (validation order, key equivalent order, tag preservation) as
  command-line programs in the test suite.
- `boottest` with QMP: open menus, choose items, key equivalents, context
  menus, status items; `screendump` comparisons.
- Time from the click on a menu title to the menu on screen, measured the
  same way in the VM and on the physical machines.

### 3.9 Rules for this work

Same as section 8 of the WindowServer plan: English in everything committed
except the `.es.md` translations; no third-party product names; API and
symbol names as they are; new code MIT or BSD-2-Clause; clean room; small
commits with the diff shown first; evidence for every roadmap item.

### 3.10 Decisions and development shortcuts

| Question | Decision (target) | Development shortcut | Removed by |
| --- | --- | --- | --- |
| Who draws and tracks the app's menus | The app (AppKit), in its own process | None: SystemUIServer drew a copy, and it is not built | MB0 |
| System menu | Minkar logo, the reference platform's items, actions through loginwindow | Restart, Shut Down and Log Out through the minimal `loginwindow`; Sleep, Force Quit, Lock Screen and System Preferences disabled | MB3 |
| Confirming Restart, Shut Down and Log Out | `loginwindow`'s panel, with its countdown | The app that asks shows an alert; Option held skips it (`SHORTCUT(Phase 5)` in `NSMenuBarWindow.m`) | Phase 5 (LoginWindow) |
| About This Computer | Atlas's own window | A plain alert with the system's name and version, kernel, processor, memory and display (`SHORTCUT(Phase 5)` in `NSMenuBarWindow.m`) | Phase 5 |
| Status items | Windows of their apps, laid out by the window server | Basic API only | MB1 |
| Menu Extras | Bundles in SystemUIServer | None until SystemUIServer is built | MB1 |
| Recent items | LaunchServices' shared file lists | Kept per app by `NSDocumentController` in its preferences | MB2 |
| Services menu | Services daemon and pasteboard | Empty Services menu | MB4 |
| System status cluster | API 19: Menu Extras in SystemUIServer; API 20: a separate control-center app | None | Minkar 20.0 |
| Look | Atlas visual language | SystemUIServer's former look: 22-point items, the system font and colors (`SHORTCUT(Phase 5)` in `NSMenuView.h`) | Phase 5, item 7 |

Every shortcut is marked in the code with `// SHORTCUT(MBn):` and the
milestone that removes it.

### 3.11 References

Public descriptions of observed behavior only; no vendor code, headers or
binaries (clean room).

- "Who owns the menu bar", pkgdemon:
  <https://pkgdemon.github.io/apple-menu-ownership.html>
- The public AppKit documentation for `NSMenu`, `NSMenuItem`,
  `NSMenuDelegate`, `NSStatusBar`, `NSStatusItem`, `NSMenuItemValidation` and
  `NSServices`.

## 4. Dock and Files

### 4.1 Decision

- **Files is written new.** Nothing of the old file browser (formerly
  Filer) is kept.
- **The Dock gets a new core**, and three parts of the current one are
  ported: the preference keys and tile data format, the desktop picture
  window, and the tile layout arithmetic as a reference.

**Guiding rule**, as for WindowServer: where the reference platform has a
known design (which process does what, preference keys, public APIs), the
Dock and Files follow it. A shortcut is allowed during development only when
section 4.10 lists it with the milestone that removes it.

### 4.2 Why: findings in the current code

#### The old file browser (formerly Filer; 94 lines)

A window titled with its name and "/Applications", with no view in it. Files dropped on
it are copied into the folder, deleting what was there first without asking;
it can only open `.app` bundles (`LSOpenCFURLRef`). There is no browsing,
icon, list or column view, Trash or "Open With". Nothing to keep.

#### Dock (`CoreServices/Dock/`, about 1,300 lines)

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

### 4.3 Who does what (reference platform layout)

| Job | Process |
| --- | --- |
| Dock tiles, running-app indicators, minimized windows, stacks, Trash tile | Dock |
| Desktop picture, one window per display at `kCGDesktopWindowLevel` | Dock |
| App switcher (Command-Tab) | Dock, which gets Command-Tab from the window server's symbolic hotkey table (today the legacy WindowServer draws one in `WSInput.m`; it goes away with `Legacy/` in WS4b) |
| File browser windows, desktop icons at `kCGDesktopIconWindowLevel`, Trash contents and emptying, "Open With", Get Info | Files |
| Starting the Dock and Files at login, and again if they exit | loginwindow (Phase 5; launchd until then) |

Spaces, an overview of windows and an app launcher grid are out of scope for
this plan; the Dock's model keeps room for them.

### 4.4 Dock architecture

- **Model.** `DockModel` with sections: persistent apps, recent and running
  apps, persistent others (folders and stacks), the Downloads stack, Trash.
  `DockTile` is plain data (URL, bundle ID, kind, flags, badge, windows).
  Views only display the model.
- **Views.** One Dock window per display that shows it, laid out by the model;
  tiles as Core Animation layers (bounce, magnification, autohide slide), so
  nothing is redrawn while nothing changes. The look follows the Atlas
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
  switcher (`WSInput.m`) goes away with `Legacy/` in WS4b, so until D3 there
  is none.

### 4.5 Files architecture

- **App.** Bundle ID `org.minkar.Files`, installed as
  `/System/Library/CoreServices/Files.app`; always running in a session
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
  and `selectFile:inFileViewerRootedAtPath:` open a Files window with the
  item selected.
- **DMG double-click** (Phase 6): mounts the image and opens its volume
  (K3, for gate M6).
- **Later.** Search on the metadata index (`mdfind`, Phase 5), tags,
  previews.

### 4.6 Dependencies

| Needed by | Dependency | State today | Where |
| --- | --- | --- | --- |
| Dock | `NSWorkspace` launch, terminate and activate notifications and the basic `NSRunningApplication` | Missing (only the old `launchedApplications`) | LaunchServices session service, WindowServer plan WS4a |
| Dock | The rest of `NSRunningApplication` (icon, launch date, hide and unhide, `activationPolicy`, terminate) | Missing | AppKit, D0 |
| Dock | Window server side of minimizing (order out and in, notices to the Dock) and single-window capture | Missing | WindowServer plan, WS4a |
| Dock | Relayed messages removed, `kWSApplicationActivate` gone | Planned | WindowServer plan, section 10.2 (WS4b) |
| Dock | `NSDockTile` sending its state | Keeps values, sends nothing | AppKit, D3 |
| Dock | `setDesktopImageURL:forScreen:options:error:` | To check | AppKit, D1 |
| Files | LaunchServices bundle and UTI database, default apps, "Open With" | Phase 3, item 4 | Frameworks |
| Files | File system events: XNU's `/dev/fsevents` exists (`vfs_fsevents.c`); a daemon and the FSEvents API are missing | Missing | CoreServices, before K1 |
| Files | `NSCollectionView`, `NSOutlineView`, `NSBrowser` working well | Present, partly stubbed | AppKit, with K1 |
| Files | `NSWorkspace` icons, recycle, reveal | Partial | AppKit, K1-K2 |
| Both | loginwindow starting them at login | Phase 5 | loginwindow |
| Both | The Atlas visual language guide | Missing | Design, before D2 and K1 |

### 4.7 Milestones

Each milestone ends with tests that pass, `boottest GATE=M2` (and, once it
exists, gate M4) still green, and the roadmap updated with the evidence.

#### Dock

- [x] **D0 — Prerequisites.** The rest of `NSRunningApplication` (icon, launch
  date, hide and unhide, `activationPolicy`, terminate) on the list that
  LaunchServices' session service keeps from WS4a, and the desktop picture
  API. The launch, terminate and activate notifications and the basic
  `NSRunningApplication` come with WS4a. Tests: a test app's icon and launch
  date are read; hiding, unhiding and terminating it through
  `NSRunningApplication` work; the desktop picture choice is stored.
  *Done (4 Nov 2026):* icon, launch date, `activationPolicy`, terminate
  and the desktop picture API (`cf62bba724`, `41cf45da95`); hide and unhide
  through launchservicesd (`606d680705`); local test `desktop` (34 checks).
- [x] **D1 — New core.** `DockModel`, preferences in the reference keys,
  persistent and running app tiles, launch and activate, running indicators,
  desktop pictures per display. The old code goes: `Trash.app`,
  `Downloads.app`, `AppDir`, the relayed notifications. Tests: model tests;
  launching an app shows its tile and indicator, quitting removes it; the
  preferences round-trip; `screendump` shows the Dock and the desktop
  picture; the Dock uses no CPU while idle. *Done (4 Nov 2026):*
  `8cfe7115db`, `f6c2d254be`; local test `desktop` (27 checks).
- [ ] **D2 — Interaction.** Drag to reorder, add and remove; context menus
  (Keep in Dock, Open at Login, Show in Files, Quit) and the app's dock menu;
  autohide, magnification, orientation; the minimize and restore animation
  (`mineffect`) for `miniaturize:`. *Partial (4 Nov 2026, `5c083e0b97`):*
  dragging, the tile menus, autohide, magnification and orientation; Show
  in Files (`7e7ea6dfbb`); Open at Login and the minimize animation are
  missing.
- [ ] **D3 — Dock tile service, minimized windows and the app switcher.**
  `org.minkar.dock.server`: badges, content, attention bounce; minimized
  windows with thumbnails (after WS4a); the app switcher in the Dock, which
  gets Command-Tab from the window server's symbolic hotkey table.
- [ ] **D4 — Stacks and Trash.** Downloads and folder stacks; Trash tile
  (empty or full, drop to trash, empty through Files).
  *Partial (6 Nov 2026):* the Trash tile, last after a separator of its
  own: its icon follows `~/.Trash` through a vnode watch (no polling), a
  click opens it, its menu opens or empties it (asked first), and an app
  tile dropped on it leaves the Dock; tested in the `desktop` image test.
  Missing: stacks, files dropped on it, Put Back, other volumes'
  `.Trashes`.

#### Files

- [ ] **K0 — Prerequisites.** File system events daemon and API; LaunchServices
  open and "Open With"; the AppKit views checked with test apps.
  *Partial (4 Nov 2026):* fseventsd and FSEvents (`a2a333d45f`, test
  `fsevents`), LaunchServices' opening and the applications for a file
  (`0331f57894`, test `launchservices`); the AppKit views are not checked
  yet (Files draws its own).
- [x] **K1 — Browsing.** Windows with sidebar, icon, list and column views,
  navigation, opening files and apps, basic Get Info. Files takes the old
  file browser's place (formerly Filer):
  bundle ID, `open -R`, the Dock's default tiles. This is the "Files
  browses" part of gate M5. *Done (4 Nov 2026):* `896749b380`,
  `c9f602b43c`, `45c2c1ad9e`, `788fe67376`, `906f12eae4`; local tests
  `argos` (16) and `launchservices` (24).
- [ ] **K2 — File operations.** Copy, move, rename, duplicate with progress and
  undo; drag and drop between windows and onto the Dock; Trash with Put Back;
  "Open With".
- [ ] **K3 — Desktop and session.** Desktop icons and volumes with eject; reveal
  APIs; started and restarted by loginwindow; DMG double-click (Phase 6,
  DMG step 3).
- [ ] **K4 — Later work.** Search on the metadata index, tags, previews.

Gate M5 ("a graphical session starts, an app opens from the Dock, Files
browses, and the resolution changes from System Preferences") needs D1 and
K1, besides loginwindow and System Preferences.

Gate M6 (Minkar 19.6.0, a desktop for daily use) needs D2 and D3, and K2
and K3. D4 and K4 come in 19.6.x.

### 4.8 Tests and measurements

- Model tests (Dock sections and tile order, preference round-trip; Files
  file operations and Trash records) as command-line programs in the test
  suite.
- `boottest` with QMP: `screendump` comparisons and injected clicks (open an
  app from the Dock; navigate a folder in Files).
- Idle CPU of the Dock and of Files near zero; time from a Dock click to the
  app's first window, measured the same way in the VM and on the physical
  machines.

### 4.9 Rules for this work

Same as section 8 of the WindowServer plan: English in everything committed
except the `.es.md` translations; no third-party product names; API and
symbol names as they are; new code MIT or BSD-2-Clause; clean room; small
commits with the diff shown first; evidence for every roadmap item; no
`Co-Authored-By` lines.

### 4.10 Decisions and development shortcuts

| Question | Decision (target) | Development shortcut | Removed by |
| --- | --- | --- | --- |
| Dock preferences | Domain `org.minkar.dock`, reference keys and tile data format | — | — |
| Who draws the desktop | Dock: pictures; Files: icons | Desktop without icons until K3 | K3 |
| App switcher | Dock, Command-Tab from the window server's hotkey table | Legacy window server (`WSInput.m`) until WS4b; then none: apps are switched by clicking their windows (`SHORTCUT(D3)`) | D3 |
| Minimized windows | Thumbnails from the window server's capture | Generic window icon | D3 (after WS4a) |
| Starting the Dock and Files | loginwindow, restarting them if they exit | launchd agents | Phase 5 (loginwindow) |
| Look | Atlas visual language guide | Plain system controls and colors | Guide available |

Every shortcut is marked in the code with `// SHORTCUT(Dn):` or
`// SHORTCUT(Kn):` and the milestone that removes it.

## 5. Applications

### 5.1 Decision

- **The apps a desktop for daily use needs, ours and in Cocoa.** A first
  set is required for Minkar 19.6.0 (essential, 5.2); a second follows in
  19.6.x; the browser and an IDE are long-term work.
- **Each app has a milestone** (A*n*) with tests in the image, as the
  session's parts do, and its dependencies listed (5.4).
- **Shared foundations first** (A0), so every app gets the same About
  window, preferences, help, localization and document handling.

### 5.2 The apps

The names, confirmed on 2026-11-05 (English, then Spanish):

| Milestone | App | What it does | Built on |
| --- | --- | --- | --- |
| A0 | (foundations) | The app template: About window, Preferences window, Help menu, Spanish and English, `NSDocument` | AppKit |
| A1 | Terminal (Terminal) | Shells in windows and tabs: a VT100/xterm emulator over a pty, scrollback, selection and copy, Agave | pty, `NSTextView` or a text grid view, CoreText |
| A2 | Text Editor (Editor de texto) | Plain text and RTF documents, find and replace, encodings, line endings, printing | `NSDocument`, Cocoa Text System |
| A3 | System Monitor (Monitor del sistema) | Processes (CPU, memory, energy, disk, network), quit and force quit, sample; graphs | libproc, `sysctl`, `top`'s data |
| A4 | Logs (Registros) | The system's and the user's logs, live and past, with searches; crash and panic reports | `logd`, `log`, `os_log` |
| A5 | Screenshot (Captura de pantalla) | The screen, a window or a selection, to a file or the pasteboard; timer; the system shortcuts | `screencapture`, `CGWindowList` |
| A6a | Image Viewer (Visor de imágenes) | Images: zoom, rotation, slideshow, export | ImageIO (to be written) |
| A6b | Document Viewer (Visor de documentos) | PDF documents: pages, zoom, search, export | PDFKit (to be written) |
| A7 | Disks (Discos) | Disks, partitions and volumes: mount, eject, erase, check, disk images | DiskArbitration, `diskutil`, `hdiutil` |
| A8 | System Information (Información del sistema) | Hardware and software report, as `system_profiler` | `system_profiler`, IOKit |
| A9 | Calculator (Calculadora) | Basic, scientific and programmer modes | AppKit |
| A10 | Archive Manager (Gestor de archivadores) | Opens and makes zip, tar and xz archives from Files | `tar` (libarchive), `ditto` |

After 19.6.0 (19.6.x):

| App | What it does | Waits for |
| --- | --- | --- |
| Fonts (Tipografías) | Installs, previews and turns off fonts | CoreText font management |
| Passwords and Keys (Contraseñas y claves) | Passwords and certificates | Security (Phase 3, item 3) |
| Color Picker (Selector de color) | The color under the pointer, in several spaces | ColorSync (Little CMS) |
| Script Editor (Editor de scripts) | Writes and runs scripts | Open Scripting Architecture (`osascript`) |
| Help (Ayuda) | Apps' help books | the Help menu (MB2) |
| Characters (Caracteres) | Finds and inserts any character | Unicode data (ICU) |

Long term, outside the 19.6.0 gate: a web browser (Ladybird, BSD-2,
already with its own AppKit interface) and an IDE (CodeEdit depends on
SwiftUI, outside API 19–21).

### 5.3 Milestones

- [ ] **A0 — App foundations.** About window (Atlas's, with icon, version
  and copyright), standard Preferences and Help menus, both languages,
  `NSDocument` with autosave and versions on the file system, the app
  template that A1–A10 start from. Tests: each app's About window, a
  localized menu, a document saved and reopened.
- [ ] **A1 — Terminal.** VT100 and xterm sequences (colors, cursor
  movement, alternate screen), UTF-8, resizing (`SIGWINCH`), scrollback,
  selection, copy and paste, Agave and its size, several windows and tabs,
  `open -a Terminal`. Tests: `vttest`'s basic screens, a `vim` session
  driven through QMP, `screendump` comparisons.
- [ ] **A2 — Text Editor for daily use.** Started 4 Nov 2026 (plain text, open
  and save, `open -t`). Missing: RTF, find and replace, encodings and line
  endings, printing, the ruler. Tests: a file round trip in each encoding,
  find and replace, an RTF document reopened.
- [ ] **A3 — System Monitor.** Process list with sorting and search, CPU,
  memory, disk and network tabs with graphs, process info, quit and force
  quit. Tests: a test process appears with its PID and is ended from the
  app.
- [ ] **A4 — Logs.** `log show` and `log stream` in a window, filters by
  process, subsystem and level, crash reports from `/Library/Logs`. Tests:
  an `os_log` message from a test program appears with its subsystem.
- [ ] **A5 — Screenshot.** Whole screen, window and selection,
  Command-Shift-3 and Command-Shift-4 through the symbolic hotkey table,
  files on the desktop named by date, the pasteboard. Tests: the shortcut
  makes a PNG that matches `screendump`.
- [ ] **A6a — Image Viewer.** Images through ImageIO (PNG, JPEG, TIFF,
  GIF, WebP), written for it. Tests: each format opens and matches a
  reference image.
- [ ] **A6b — Document Viewer.** PDF through PDFKit, written for it: pages,
  zoom, search, selection and copy. Tests: a PDF page renders against a
  reference image; a word is found.
- [ ] **A7 — Disks.** Volumes and partitions, mount and eject, erase (HFS+,
  FAT, exFAT), check, create and attach disk images. Tests: an image is
  created, erased, mounted and ejected from the app.
- [ ] **A8 — System Information.** The report `system_profiler` makes,
  browsable. Tests: the hardware overview matches `system_profiler`.
- [ ] **A9 — Calculator.** Basic, scientific and programmer modes, paper
  tape, keyboard entry. Tests: a set of calculations through QMP keys.
- [ ] **A10 — Archive Manager.** Expand zip, tar, gzip and xz archives from Files
  (double-click) and compress a selection. Tests: an archive made and
  expanded from Files.

### 5.4 Dependencies

| Needed by | Dependency | Where |
| --- | --- | --- |
| A1 | Fast text drawing in a grid; key events with all modifiers | AppKit, CoreText (Phase 5, item 7) |
| A2 | RTF reading and writing, printing | AppKit (`NSAttributedString`, `NSPrintOperation`) |
| A3 | Per-process CPU, memory, disk and network figures | libproc and `sysctl` (Phase 2) |
| A4 | Reading the log store from a program | `logd` (done 3 Nov 2026) |
| A5 | Window images; the system shortcuts | `CGWindowListCreateImage`; WindowServer's symbolic hotkeys |
| A6a, A6b | ImageIO and PDFKit | to be written (Phase 4 graphics stack) |
| A7 | Erase and check from a program | DiskArbitration, `newfs_*`, `fsck_*` |
| All | Icons | the maintainer's designs (1) |

### 5.5 Tests

As for the session: each milestone adds image tests (the program's
behavior through its files, its pasteboard and `screendump`) and QMP
interaction to the local suite, and the apps on the 19.6.0 list are opened
by the M6 gate.

## 6. Gates

- **M5** (Phase 5, unchanged): from boot, a graphical session starts, an
  app opens from the Dock, Files browses, and the resolution changes from
  Preferences, with the visual language K1 needs; MB1 is part of it.
- **M6 and Minkar 19.6.0** (a desktop for daily use): MB2 and MB3, D2 and
  D3, K2 and K3, and the essential apps A0–A10; the browser is not
  required.
- **19.6.x:** MB4, D4, K4 and the apps of 5.2's second table.
