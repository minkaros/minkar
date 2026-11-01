# Menus plan

Status: **accepted** (2026-11-01). Phases 4 and 5 of [ROADMAP.md](ROADMAP.md).
Spanish version: [Menus.es.md](Menus.es.md).
Checked against `devel` at `4808c1000`.

This document is the decision record and the work plan for the menu bar,
menus, status items and the system menu. It follows the same rules as
[WindowServer-Rewrite.md](WindowServer-Rewrite.md) and details the menu,
status item and recent item rows of its section 10.2, and the system menu
side of its section 10.3.

## 1. Decision

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
10 lists it with the milestone that removes it.

## 2. Why: findings in the current code

1. **There is no menu bar today.** SystemUIServer, which draws it, is not
   built (`CoreServices/Makefile`: "FIXME: Dock, Filer, DMIHelper,
   SystemUIServer and loginwindow ... are not built yet").
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

## 3. Who does what (reference platform layout)

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
status item windows stay where they are. Kraz is always running, so it is
the front app whenever no other app is.

Because menus run in the app's process, a hung app cannot open its menus,
as on the reference platform; the window server shows the busy cursor over
it, and Force Quit (loginwindow) remains available.

## 4. Behavior to follow

### Menu bar

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

### Tracking

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

### Key equivalents

- In the documented order: the key window and its views
  (`performKeyEquivalent:`), then the main menu, each item validated first.
- The menu shows key equivalents with the modifier glyphs.
- Shortcuts reserved by the system are recognized by the window server and
  sent to their owner, not delivered as key events: Command-Tab to the Dock,
  Command-Option-Escape to loginwindow, Control-F2 to the front app's
  AppKit, which moves focus to the menu bar.

### Standard menus

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

### Other menus

- Context menus: `menuForEvent:`, Control-click,
  `+popUpContextMenu:withEvent:forView:`,
  `-popUpMenuPositioningItem:atLocation:inView:`.
- Pop-up buttons (`NSPopUpButton`) with the same menu windows and tracking.
- Dock menus (`applicationDockMenu:`) through the Dock tile service of
  [Dock-Kraz.md](Dock-Kraz.md) (D2).

### Status items

- `-[NSStatusBar statusItemWithLength:]` with `NSVariableStatusItemLength`
  and `NSSquareStatusItemLength`; `button` (`NSStatusBarButton`), `menu`,
  `length`, `visible`, `autosaveName`, `behavior`.
- Each item is a window of its app at `kCGStatusWindowLevel`; the window
  server places them right to left, with the system Menu Extras; Command-drag
  reorders them and the order is kept.

### Accessibility

Menu bar, menus and items expose their accessibility roles, so assistive
tools and the accessibility API can read and press them.

## 5. Architecture

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

## 6. Dependencies

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
| All | The Gienah visual language (menu fonts, metrics, colors) | Phase 5, item 7 |
| Dock menus | Dock tile service | [Dock-Kraz.md](Dock-Kraz.md), D2 |

## 7. Milestones

Each milestone ends with tests that pass, the existing gates still green and
the roadmap updated with the evidence. Milestones are named MB*n* to keep
them apart from the gates M*n*.

- [ ] **MB0 — Menus in the app (part of WS4b).** Menu views moved into AppKit;
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
  restart. Gate M5 needs MB1.
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

## 8. Tests and measurements

- Model tests (validation order, key equivalent order, tag preservation) as
  command-line programs in the test suite.
- `boottest` with QMP: open menus, choose items, key equivalents, context
  menus, status items; `screendump` comparisons.
- Time from the click on a menu title to the menu on screen, measured the
  same way in the VM and on the physical machines.

## 9. Rules for this work

Same as section 8 of the WindowServer plan: English in everything committed
except the `.es.md` translations; no third-party product names; API and
symbol names as they are; new code MIT or BSD-2-Clause; clean room; small
commits with the diff shown first; evidence for every roadmap item.

## 10. Decisions and development shortcuts

| Question | Decision (target) | Development shortcut | Removed by |
| --- | --- | --- | --- |
| Who draws and tracks the app's menus | The app (AppKit), in its own process | None: SystemUIServer drew a copy, and it is not built | MB0 |
| System menu | Minkar logo, the reference platform's items, actions through loginwindow | Restart, Shut Down and Log Out through the minimal `loginwindow`; Sleep, Force Quit, Lock Screen and System Preferences disabled | MB3 |
| Status items | Windows of their apps, laid out by the window server | Basic API only | MB1 |
| Menu Extras | Bundles in SystemUIServer | None until SystemUIServer is built | MB1 |
| Recent items | LaunchServices' shared file lists | Kept per app by `NSDocumentController` in its preferences | MB2 |
| Services menu | Services daemon and pasteboard | Empty Services menu | MB4 |
| System status cluster | API 19: Menu Extras in SystemUIServer; API 20: a separate control-center app | None | Minkar 20.0 |
| Look | Gienah visual language | Plain system font and colors | Phase 5, item 7 |

Every shortcut is marked in the code with `// SHORTCUT(MBn):` and the
milestone that removes it.

## 11. References

Public descriptions of observed behavior only; no vendor code, headers or
binaries (clean room).

- "Who owns the menu bar", pkgdemon:
  <https://pkgdemon.github.io/apple-menu-ownership.html>
- The public AppKit documentation for `NSMenu`, `NSMenuItem`,
  `NSMenuDelegate`, `NSStatusBar`, `NSStatusItem`, `NSMenuItemValidation` and
  `NSServices`.
