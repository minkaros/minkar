# Atlas

Atlas is Minkar's desktop: the session (login, menu bar, Dock, Files, System Preferences)
and the apps a person needs every day. This document holds its design rules, how each
part is built, and the milestones still open; for contributors to the desktop. Last
reviewed: 2026-11-21. Phases 5 and 6 of [ROADMAP.md](ROADMAP.md).

Where the reference platform has a known design (which process does what, preference
keys, public APIs, standard menus), Atlas follows it. A shortcut is allowed during
development only when this document lists it with the milestone that removes it, and the
code marks it `// SHORTCUT(<milestone>):`.

## 1. Scope and rules

- **Everything is a Cocoa app or bundle**, written against the public Cocoa API, so the
  desktop is a continuous test of AppKit and Foundation. What an app needs that the
  frameworks lack is added to the frameworks, never worked around in the app.
- **Its own visual language.** No third-party fonts, sounds or artwork, with two
  exceptions for icons: the Faience and Faenza themes by Matthieu James (GPL-3), separate
  data files with their license and SVG sources in `SystemLibrary/Icons` (installed in
  `/System/Library/Icons`), without brand icons; and, for the menu bar's status items,
  Lucide's icons (stroke 2; ISC, MIT for those from Feather), pinned by commit and
  SHA-256 in `SystemLibrary/MenuBarIcons` (installed in
  `/System/Library/MenuBarIcons`), only the icons the menu bar uses. No code links with
  them.
- **Look.** Folders are blue (Faience-Azur), with sand and orange as alternatives. App
  icons are 512×512 PNG in the bundle (AppKit reads no `.icns` yet). The menu bar follows the
  appearance (General in System Preferences): light (pale, dark text) or dark
  (charcoal, light text); its template icons are drawn in its text color. The interface font is Inter and the fixed-pitch font Agave
  (both SIL OFL 1.1).
- **Cursors.** Atlas's own (`SystemLibrary/Cursors/gen-cursors.py`, drawn from their
  geometry when the system is built): black glyphs outlined in white with a soft shadow,
  the hands white outlined in black, colored badges for copy, link, help and not allowed,
  and a turning wheel for busy apps; at 1x and 2x, each as large as the reference's same
  cursor. Installed in `/System/Library/Cursors`.
- **Generic names.** The only proper names are Minkar, the system, and Atlas, its
  desktop. Every app is named for what it does, with the common words free desktops use
  (Files, Text Editor, System Monitor, Logs). The bundle has no spaces
  (`TextEditor.app`), its identifier is `org.minkar.<Name>`, and the Spanish name comes
  with the app's translation (Archivos, Editor de texto).
- **On the command-line tools.** Where Minkar has the command (`top`, `log`,
  `screencapture`, `diskutil`, `hdiutil`, `system_profiler`, `tar`), the app is its
  interface and shares its code or output.
- **English and Spanish.** Every app is localized in both; English is the language of
  development, tests and screenshots.
- **Places.** The session's parts in `/System/Library/CoreServices`, the apps in
  `/System/Applications`, utilities in `/System/Applications/Utilities`.
- **Tests.** Each milestone adds image tests (behavior through files, the pasteboard and
  `screendump`) and QMP interaction to the local suite, with the gates still green.

## 2. Status

| Part | Milestones | Done | Open |
| --- | --- | --- | --- |
| Menus (chapter 3) | MB0–MB4 | MB0, MB1, MB2, MB3 | MB4 (19.6.x) |
| Dock (chapter 4) | D0–D4 | D0, D1, D2, D3 | D4 (partial; 19.6.x) |
| Files (chapter 4) | K0–K4 | K0, K1, K2, K3 | K4 (19.6.x) |
| Applications (chapter 5) | A0–A10 | A0, A1, A2, A3, A4 | A5–A10 (19.6.x) |
| Notifications (5.4) | N0–N2 | N0, N1, N2 | — |
| Window and text services (5.5) | W0–W4 | — | W0–W3 (19.6.0), W4 (to decide) |

The session's other parts (LoginWindow, System Preferences, the visual language) are
items of Phase 5 in [ROADMAP.md](ROADMAP.md).

## 3. Menus

- **Menus live in each app.** AppKit draws the app's menu bar and menus and tracks them in
  the app's own process. The window server only reserves the menu bar's place on each
  display and says which app is in front.
- **Status items** are windows of their own app.
- **SystemUIServer** only hosts the system's Menu Extras.

### 3.1 Who does what

| Job | Who |
| --- | --- |
| The menu bar's place on each display, its height, the front app, the status area layout | Window server |
| The front app's menu bar (system menu, app menu, the app's menus) in a window at `kCGMainMenuWindowLevel` | The app (AppKit) |
| Opening and tracking menus, submenus and context menus, in windows at `kCGPopUpMenuWindowLevel` | The app (AppKit) |
| Status items (`NSStatusItem`), each a window at `kCGStatusWindowLevel` | The app that owns them |
| System Menu Extras (clock, volume, network, displays, notifications) as loadable bundles | SystemUIServer |
| From API 20: the system status cluster and its popovers | A separate control-center app |
| What the system menu's items do (restart, shut down, log out, sleep, force quit) | loginwindow |
| The Services menu's contents | The services daemon (pasteboard server) |

Each part of the bar is its own borderless window, composited by the window server into
one strip. When the front app changes, the window server swaps the menu bar windows;
status item windows stay. Files is always running, so it is the front app when no other
is. A hung app cannot open its menus; the window server shows the busy cursor over it,
and Force Quit stays available.

### 3.2 Behavior to follow

**Menu bar**

- One menu bar per display, showing the front app's menus; the app menu in bold with the
  app's name.
- The system menu first, with the Minkar logo: About This Computer, System Preferences…,
  Recent Items, Force Quit…, Sleep, Restart…, Shut Down…, Lock Screen, Log Out *user*….
  The same in every app.
- The app menu's standard items: About *App*, Preferences…, Services, Hide *App*, Hide
  Others, Show All, Quit *App*. Apps without a main menu still get the system menu and an
  app menu with Quit.
- The app's menus, then the status area on the right; when space runs out the app's menus
  are cut from the end.
- Hidden in full screen and shown when the pointer reaches the top; optional auto-hide.
- `-[NSScreen visibleFrame]` excludes the menu bar on every display that shows one.

**Tracking**

- Press, drag and release selects an item; a click opens a menu that stays open until
  another click.
- Keyboard: arrows, Return, Escape, type-select; Control-F2 moves focus to the menu bar.
- Submenus open after a short delay and stay open while the pointer moves towards them.
- Long menus scroll. Holding Option shows alternate items (`isAlternate`).
- `NSMenuDelegate`'s calls; validation when a menu opens and before a key equivalent
  fires, through `validateMenuItem:` or `validateUserInterfaceItem:`.

**Key equivalents**

- In the documented order: the key window and its views (`performKeyEquivalent:`), then
  the main menu, each item validated first.
- Shortcuts reserved by the system go from the window server to their owner, numbered as
  the reference numbers them (its com.apple.symbolichotkeys domain): Command-Tab and
  Option-Command-D to the Dock, Command-Option-Escape to loginwindow, Control-F2 and
  Command-` to the front app, Shift-Command-3 (with Control: the pasteboard) and
  Control-Space, Control-Option-Space to SystemUIServer. The user changes them in the Keyboard
  pane's Shortcuts tab; the app switcher and Force Quit are fixed, as on the reference.

**Standard menus**

- Window menu: every window listed, Minimize, Zoom, Bring All to Front.
- Open Recent: `NSDocumentController` with LaunchServices' shared file lists.
- Help menu: a search field that finds menu items (and help books, later).
- Services menu: built from the `NSServices` entries in apps' `Info.plist` by the services
  daemon.
- View menu: Enter Full Screen for windows that support it.

**Other menus and status items**

- Context menus (`menuForEvent:`, Control-click, `popUpContextMenu:withEvent:forView:`),
  pop-up buttons, Dock menus (`applicationDockMenu:`).
- `NSStatusBar`/`NSStatusItem` with `button`, `menu`, `length`, `visible`, `autosaveName`,
  `behavior`; the window server lays items out right to left with the Menu Extras;
  Command-drag reorders them and the order is kept.
- Menu bar, menus and items expose their accessibility roles.

### 3.3 Architecture

- **AppKit:** `NSMenuBarWindow` (one per display, with an `NSMenuBarView`),
  `NSMenuWindow` and `NSMenuView` for menus and submenus; the tracking loop runs in
  `NSEventTrackingRunLoopMode` with the pointer captured by the window server; the system
  menu comes from a shared definition, and its actions are requests to loginwindow and to
  System Preferences; `NSStatusBar` and `NSStatusItem` are Minkar's own.
- **Window server (SkyLight):** reserves the menu bar's place per display and publishes its
  height; tells each app when it becomes or stops being the front app; lays out the status
  area; handles the reserved shortcuts; captures the pointer for an open menu.
- **SystemUIServer:** hosts Menu Extra bundles (`.menu`) from
  `/System/Library/CoreServices/Menu Extras`, listed in its `menuExtras` defaults.
- **Services daemon:** builds the services list from `NSServices` and runs a service through
  the pasteboard (`NSPerformService`, `NSUpdateDynamicServices`).

### 3.4 Milestones

- [x] MB0 — Menus in each app, the system menu, basic status items. `9d510ec6c2`
- [x] MB1 — Menu Extras as `.menu` bundles and the full status item API. Local test `menubar-extras`.
- [x] MB2 — Window menu, Open Recent, Help search, alternates, scrolling, Enter Full Screen, accessibility. Tests `menus-mb2`, `menus`.
- [x] MB3 — The full system menu (19.6.0): System Preferences…, Recent Items,
  Force Quit… and Command-Option-Escape, Sleep (the displays sleep where the machine
  cannot), Lock Screen and Control-Command-Q (the login window for the session's user,
  above everything, until its password is given). Tests: Restart asks apps to quit before
  rebooting; Force Quit ends a hung test app. Local tests `system-menu`, `loginwindow`,
  `lock-screen`.
- [ ] **MB4 — Services and the menu bar's behavior** (19.6.x): the services daemon and
  menu; hiding in full screen; auto-hide.

Measured the same way in the VM and on the physical machines: the time from a click on a
menu title to the menu on screen.

### 3.5 Decisions and development shortcuts

| Question | Decision (target) | Development shortcut | Removed by |
| --- | --- | --- | --- |
| System menu | Minkar logo, the reference platform's items, actions through loginwindow, Lock Screen | — | MB3, done |
| Confirming Restart, Shut Down and Log Out | loginwindow's panel, with its countdown | The app that asks shows an alert; Option skips it (`NSMenuBarWindow.m`) | Phase 5 (LoginWindow) |
| About This Computer | Atlas's own window | A plain alert with the system's name, version, kernel, processor, memory and display (`NSMenuBarWindow.m`) | Phase 5 |
| Services menu | Services daemon and pasteboard | An empty Services menu | MB4 |
| System status cluster | API 19: Menu Extras in SystemUIServer; API 20: a separate control-center app | None | The app set that raises `ProductVersion` to 11.0 |
| Look | Atlas visual language | 22-point items, the system font and colors (`NSMenuView.h`) | Phase 5, item 7 |

### 3.6 References

Public descriptions of observed behavior only; no vendor code, headers or binaries.

- "Who owns the menu bar", pkgdemon: <https://pkgdemon.github.io/apple-menu-ownership.html>
- The public AppKit documentation for `NSMenu`, `NSMenuItem`, `NSMenuDelegate`,
  `NSStatusBar`, `NSStatusItem`, `NSMenuItemValidation` and `NSServices`.

## 4. Dock and Files

Files is Minkar's own file manager; the Dock has its own core, with the reference
platform's preference keys and tile data format.

### 4.1 Who does what

| Job | Process |
| --- | --- |
| Dock tiles, running-app indicators, minimized windows, stacks, Trash tile | Dock |
| Desktop picture, one window per display at `kCGDesktopWindowLevel` | Dock |
| App switcher (Command-Tab, from the window server's symbolic hotkey table) | Dock |
| File browser windows, desktop icons at `kCGDesktopIconWindowLevel`, Trash contents and emptying, Open With, Get Info | Files |
| Starting the Dock and Files at login, and again if they exit | loginwindow |

Spaces, a window overview and an app launcher grid are out of scope; the Dock's model
keeps room for them.

### 4.2 Dock architecture

- **Model.** `DockModel` with sections: persistent apps, recent and running apps,
  persistent others (folders and stacks), the Downloads stack, Trash. `DockTile` is plain
  data (URL, bundle ID, kind, flags, badge, windows). Views only display the model.
- **Views.** One Dock window per display, tiles as Core Animation layers (bounce,
  magnification, autohide slide), so nothing is redrawn while nothing changes.
- **Data sources.** `NSWorkspace` launch, terminate, activate, hide and unhide
  notifications and `NSRunningApplication`; icons through LaunchServices; activation with
  `activateWithOptions:`.
- **Minimized windows.** The Dock owns them: AppKit's `miniaturize:` asks the Dock, which
  runs the animation (`mineffect`) and keeps the tile with the window's image from
  `CGWindowListCreateImage`; the window server orders the window out and in and tells the
  Dock.
- **Dock tile service.** `org.minkar.dock.server` (MIG) for `NSDockTile` (badge, content
  view), `requestUserAttention:` (bounce) and `applicationDockMenu:`.
- **Preferences.** Domain `org.minkar.dock`, with the reference platform's keys:
  `persistent-apps`, `persistent-others`, `tilesize`, `magnification`, `largesize`,
  `orientation`, `autohide`, `autohide-delay`, `autohide-time-modifier`, `mineffect`,
  `show-recents`; tile data in the reference format (`tile-data`, `file-data`,
  `_CFURLString`).
- **Desktop pictures.** One window per display keyed by the display ID;
  `setDesktopImageURL:forScreen:options:error:` stores the choice and the Dock follows it.
- **App switcher.** The Dock claims Command-Tab and Command-Shift-Tab, shows the regular
  apps in the order they were used, and activates the one chosen when Command is released.

### 4.3 Files architecture

- **App.** `org.minkar.Files`, installed as `/System/Library/CoreServices/Files.app`;
  always running in a session.
- **Windows.** Toolbar, sidebar (favorites, volumes from DiskArbitration, locations), path
  bar, and icon, list and column views; navigation history, sorting, per-folder view
  settings.
- **Opening.** Through LaunchServices: default app per UTI, Open With, apps by bundle.
- **File operations.** Copy, move, duplicate, rename, new folder, alias; with progress,
  cancel and undo; off the main thread. Drags between apps go through the window server
  (`SLSPostDragEvent`) and the shared drag pasteboard.
- **Trash.** `~/.Trash` and `/.Trashes/<uid>` on other volumes; Put Back from the
  `org.minkar.trash.put-back` attribute `trashItemAtURL:` sets, so any app's trashed items
  can be put back.
- **Desktop.** A window per display at `kCGDesktopIconWindowLevel` with the mounted
  volumes (with eject) and `~/Desktop`, followed through FSEvents and NSWorkspace's volume
  notifications.
- **Reveal.** `open -R`, `activateFileViewerSelectingURLs:`, `LSRevealInFiles`.
- **Disk images.** A double-clicked image is attached and its volume opened.

### 4.4 Milestones

- [x] D0 — `NSRunningApplication` complete and the desktop picture API. `cf62bba724`, `606d680705`
- [x] D1 — The Dock's new core, preferences in the reference keys, desktop pictures. `8cfe7115db`, `f6c2d254be`
- [x] D2 — Reordering, tile menus, autohide, magnification, Open at Login, minimizing into tiles. `5c083e0b97`; test `dock-minimize`
- [x] D3 — Dock tile service, minimized windows, the app switcher. Local test `dock-d3`.
- [ ] **D4 — Stacks and Trash** (19.6.x). Done: the Trash tile (follows `~/.Trash`
  without polling; open, empty after asking; app tiles dropped on it leave the Dock).
  Left: stacks, files dropped on it, Put Back, other volumes' `.Trashes`.
- [x] K0 — Prerequisites: fseventsd and FSEvents (`a2a333d45f`) with each volume's history,
  device-relative streams and exclusions (`d87116be1c`; test `fsevents`); LaunchServices'
  opening and Open With (`0331f57894`); the AppKit views apps build file browsers with,
  checked with a test app (`06176ca807`, `f07b54ac00`; test `appkit-file-views`).
- [x] K1 — Browsing: sidebar, icon, list and column views, opening, Get Info, reveal. `896749b380`, `906f12eae4`
- [x] K2 — File operations with progress and undo, drag and drop, Trash with Put Back, Open With. Tests `files-ops`, `drag-apps`.
- [x] K3 — Desktop icons and volumes with eject, disk image double-click. Test `desktop-icons`.
- [ ] **K4 — Later work** (19.6.x): search on the metadata index (`mdfind`), tags, previews.

Known bug: ejecting a volume by dropping it on the Dock's Trash fails in the
`desktop-icons` test (found 2026-11-21).

Measured the same way in the VM and on the physical machines: idle CPU of the Dock and
Files (near zero), and the time from a Dock click to the app's first window.

### 4.5 Decisions and development shortcuts

| Question | Decision (target) | Development shortcut | Removed by |
| --- | --- | --- | --- |
| Dock preferences | Domain `org.minkar.dock`, reference keys and tile data format | — | — |
| Who draws the desktop | The Dock the pictures, Files the icons | — | — |
| Minimizing animation | The reference platform's genie | Drawn as a scale effect | Atlas visual language |
| Look | Atlas visual language guide | Plain system controls and colors | The guide |

## 5. Applications

The apps a desktop for daily use needs, Minkar's own and in Cocoa. Shared foundations
first (A0), so every app gets the same About window, preferences, help, localization and
document handling. A0–A4 are required for Minkar 19.6.0; A5–A10 and the second set follow
in 19.6.x; a web browser (Firefox) and an IDE are long-term work.

### 5.1 The apps

| Milestone | App (Spanish name) | What it does | Built on |
| --- | --- | --- | --- |
| A0 | (foundations) | About and Preferences windows, Help menu, Spanish and English, `NSDocument` | AppKit |
| A1 | Terminal (Terminal) | VT100/xterm emulator over a pty, tabs, scrollback, Agave | libvterm (MIT), CoreText |
| A2 | Text Editor (Editor de texto) | Plain text and RTF, find and replace, encodings, line endings, printing | `NSDocument`, Cocoa Text System |
| A3 | System Monitor (Monitor del sistema) | Processes, CPU, memory, energy, disk, network; quit and force quit | libproc, `sysctl` |
| A4 | Logs (Registros) | Logs live and past, with searches; crash and panic reports | `logd`, `log`, `os_log` |
| A5 | Screenshot (Captura de pantalla) | Screen, window or selection to a file or the pasteboard; the system shortcuts | `screencapture`, `CGWindowList` |
| A6a | Image Viewer (Visor de imágenes) | Images: zoom, rotation, slideshow, export | ImageIO |
| A6b | Document Viewer (Visor de documentos) | PDF: pages, zoom, search, export | PDFKit |
| A7 | Disks (Discos) | Disks, partitions and volumes: mount, eject, erase, check, disk images | DiskArbitration, `diskutil`, `hdiutil` |
| A8 | System Information (Información del sistema) | Hardware and software report | `system_profiler`, IOKit |
| A9 | Calculator (Calculadora) | Basic, scientific and programmer modes | AppKit |
| A10 | Archive Manager (Gestor de archivadores) | zip, tar and xz archives from Files | libarchive, `ditto` |

The second set, also 19.6.x: Fonts (CoreText font management), Passwords and Keys
(Security), Color Picker (ColorSync on Little CMS), Script Editor (`osascript`), Help (the
Help menu), Characters (ICU).

### 5.2 Milestones

- [x] A0 — App foundations and the template `Developer/Templates/AtlasApp`. `5a721456ef`, `fbef0a5927`; test `atlas-app`
- [x] A1 — Terminal on libvterm 0.3.3, with tabs and Preferences. `d7b7c4f405`; test `terminal`
- [x] **A2 — Text Editor for daily use** (19.6.0). Done: plain text, open and save,
  `open -t`; find and replace through AppKit's Find panel (`10594b5eb5`, test
  `text-editor-find`); encodings and line endings kept, and chosen in Save As (test
  `text-editor-encodings`); rich text, opened and saved as RTF, Make Rich/Plain Text
  (`01ee8bd8e6`, test `text-editor-rtf`); the ruler (`087039cdfd`, test `appkit-text-ruler`);
  printing through AppKit's print panel, to a printer or a PDF (test `text-editor-print`).
  Tests: a round trip in each encoding, find and replace, an RTF document reopened.
- [x] A3 — System Monitor. `5a1c89ed68`; test `system-monitor`. Per-process energy waits for the power driver's data.
- [x] A4 — Logs on `log --style ndjson`. `033761cf48`; test `logs`. Nothing writes crash reports yet.
- [ ] **A5 — Screenshot.** Whole screen, window and selection; Command-Shift-3 and
  Command-Shift-4 through the symbolic hotkey table; files on the desktop named by date;
  the pasteboard. Test: the shortcut makes a PNG that matches `screendump`.
- [ ] **A6a — Image Viewer.** PNG, JPEG, TIFF, GIF and WebP through ImageIO. Test: each
  format matches a reference image.
- [ ] **A6b — Document Viewer.** PDF through PDFKit: pages, zoom, search, selection and
  copy. Tests: a page against a reference image; a word found.
- [ ] **A7 — Disks.** Volumes and partitions, mount and eject, erase (HFS+, FAT, exFAT),
  check, disk images. Test: an image created, erased, mounted and ejected from the app.
- [ ] **A8 — System Information.** The `system_profiler` report, browsable. Test: the
  hardware overview matches `system_profiler`.
- [ ] **A9 — Calculator.** Basic, scientific and programmer modes, paper tape, keyboard
  entry. Test: calculations through QMP keys.
- [ ] **A10 — Archive Manager.** Expand zip, tar, gzip and xz from Files and compress a
  selection. Test: an archive made and expanded from Files.

### 5.3 Dependencies of the open apps

| Needed by | Dependency |
| --- | --- |
| A2 | RTF reading and writing, printing (`NSAttributedString`, `NSPrintOperation`) |
| A5 | `CGWindowListCreateImage`; the window server's symbolic hotkeys |
| A6a, A6b | ImageIO and PDFKit |
| A7 | Erase and check from a program (DiskArbitration, `newfs_*`, `fsck_*`) |
| All | Icons: the maintainer's designs |

### 5.4 Notification center (N0–N2)

`NSUserNotificationCenter` and `UserNotifications` hand notifications to the session
service `notificationd` over Mach; SystemUIServer presents them (banners at the top right,
alerts, the list as the rightmost Menu Extra with Clear and Do Not Disturb), and the Dock
draws badges. Settings live in `org.minkar.notificationcenter` and the Notifications pane.
Scheduled notifications are kept by notificationd on disk.

- [x] N0 — Delivery, banners and sounds. Test `notifications`.
- [x] N1 — The list, the settings pane and Do Not Disturb. `2910fb0cb8`
- [x] N2 — Alerts with actions and replies, scheduled notifications, badges. `7b55914d9a`, `51e1316988`

Left for later: a click on a notification shown after its app quit does not start the
app.

### 5.5 Window and text services the system adds (W0–W4)

The system adds items to every app's menus; their services are not in Atlas yet. W0–W3
are part of Minkar 19.6.0.

- [x] **W0 — Window tabs.** `NSWindowTab`, `NSWindowTabGroup`, `tabbingMode`,
  `tabbingIdentifier`, `addTabbedWindow:ordered:`, `userTabbingPreference`, the tab bar,
  and the Window menu's tab items. Gate: two documents merge into one window with two tabs
  and split again, from the menu and by dragging a tab. Done (23 Nov 2026): the tab bar
  (choose, close, +, drag out, onto another window, within the bar), Prefer tabs from the
  Dock pane, Show Tab Bar, Show Previous/Next Tab, Move Tab to New Window, Merge All
  Windows; tests `appkit-window-tabs`, `text-editor-tabs`. Not yet: Show All Tabs (the
  overview).
- [x] **W1 — Tiling.** Tile Window to Left/Right of Screen, Move Window to Left/Right Side
  of Screen, holding the zoom button, two full-screen windows side by side. Gate: two apps
  share the screen and leave it as they came. Done (23 Nov 2026): the Window menu's items
  (Move with Option), the zoom button's menu held, a tiled window back to its frame by
  Exit Full Screen or the zoom button; test `window-tiling`. Not yet: choosing the other
  half's window and moving the divider (they need spaces).
- [x] **W2 — Window restoration.** `NSWindowRestoration`, `restorable`,
  `encodeRestorableStateWithCoder:`, restoration at launch, Quit and Keep Windows. Gate:
  Text Editor quit with two documents comes back with both, in their places. Done (23 Nov
  2026): the state saved as the app quits keeping its windows (Quit and Keep Windows,
  Option-Command-Q, or the General pane's choice) in `~/Library/Saved Application State`,
  the windows made again at launch by their restoration classes; test
  `text-editor-restore`. Not yet: saving while the app runs (after a crash), NSDocument's
  own restoration, views' restorable state.
- [ ] **W3 — Emoji & Symbols.** The Edit menu's item and a character panel inserting
  through `insertText:`. Gate: a character chosen is inserted into Text Editor.
- [ ] **W4 — Dictation.** *To decide:* it needs an on-device speech recognizer; until then
  the item is not shown.

## 6. Gates

- **M5:** from boot, a graphical session starts, an app opens from the Dock, Files
  browses, and the resolution changes from System Preferences, in the Atlas visual
  language; MB1 is part of it.
- **M6 and Minkar 19.6.0** (a desktop for daily use): MB2 and MB3, D2 and D3, K2 and K3,
  the apps A0–A4, notifications N0–N1 and the window services W0–W3; the browser is not
  required.
- **19.6.x:** MB4, D4, K4, N2, the apps A5–A10 and the second set.
