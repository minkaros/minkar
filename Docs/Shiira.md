# Shiira: analysis for Minkar

Status: analysis, 11 November 2026. Nothing has been imported yet.
Spanish version: [Shiira.es.md](Shiira.es.md).

Shiira is a web browser for the Cocoa API, written in Objective-C on
WebKit by The Shiira Project (Japan, 2004–2009). This document says what
it would take to bring it into Minkar, modernised, as a browser of Atlas's
own. Firefox stays the reference app for coverage and the browser of
Minkar 19.6.0 (Docs/Atlas.md). Shiira does not replace it.

## 1. What there is

| | |
|---|---|
| Last release | 2.3 (build 090811), August 2009; the project stopped then |
| Last published source | 2.2 (`Shiira2.2src.zip`, OSDN mirror `ftp.iij.ad.jp/pub/osdn.jp/shiira/26462/`, SHA-256 `531211b9…cd243`) |
| Source of 2.3 | not published as an archive; it may be in the project's repository (to ask the owner) |
| Size | 84 000 lines of Objective-C: Shiira 58 600, HMDTAppKit 21 300, HMDTWebKit 2 200, HMDTFoundation 1 800 (206 `.m` files) |
| Licence | BSD, two clauses ("Redistribution and use…"), in 405 of the 414 source files; 9 files carry only "All rights reserved" |
| Binaries | i386 and PowerPC only; Minkar runs x86_64 code only, so it has to be built from source |
| Project | Xcode 2.4 (`objectVersion 42`), SDK 10.4u |
| Interface | 86 NIBs in English (17 windows), 12 languages; `MainMenu.nib` is the old typedstream format (`NSIBObjectData`), the rest keyed archives |

## 2. What it uses

Frameworks linked by 2.3: Cocoa, WebKit, CoreData, QuartzCore, Quartz,
OpenGL, Security, Carbon, CoreServices, ApplicationServices,
ExceptionHandling, and two bundled third-party frameworks, Sparkle
(updates) and Growl (notifications), as prebuilt i386/PPC binaries.

Of the 564 Cocoa names the source uses (classes, constants, protocols),
127 are not in Minkar's SDK headers today:

- **WebKit and DOM (about 100):** `WebView` is not there yet, nor
  `WebHistory`, `WebArchive`, `WebBackForwardList`, `WebResource` and the
  DOM classes (`DOMHTMLInputElement`, `DOMHTMLFormElement`,
  `DOMHTMLAnchorElement`…). Minkar's WebKit.framework (on WPE) is planned
  after Auto Layout (Docs/ROADMAP.md, W items).
- **Private WebKit API:** `WebIconDatabase` (25 uses), `WebInspector` (12,
  with its own copy of the private header), `WebHTMLView`,
  `_webFrameForDOMDocument`. Current WebKit removed or hides these: they
  have to be replaced, not provided.
- **Core Image transitions:** `CICopyMachineTransition`,
  `CIPageCurlTransition` (tab effects): decorative, can be dropped.
- **AppKit:** a few old constants (`NSUnifiedTitleAndToolbarWindowMask`,
  scroller part names) and Shiira's own categories.

Carbon and other C calls: `SetSystemUIMode` (full screen, 5),
`SecKeychain*InternetPassword` (saved passwords), `FSPathMakeRef`,
`LSSetDefaultHandlerForURLScheme` and friends (default browser).
Older Cocoa API it relies on: `NSURLDownload` (63 uses, downloads),
`NSCalendarDate` (18), `NSArchiver`/`NSUnarchiver`, `NSAnimation`,
bindings with array and tree controllers, CoreData (bookmarks, history,
RSS).

## 3. What modernising means

1. **Build.** Replace the Xcode 2.4 projects with Minkar's build (as the
   reference apps: CMake and Ninja, or bmake), x86_64, clang 21, ARC
   optional (manual retain/release builds as is).
2. **64-bit.** `int`/`unsigned` where the API now takes `NSInteger` and
   `NSUInteger`, `float` where it takes `CGFloat`; format strings.
3. **Out:** Sparkle (Minkar updates the system itself), Growl (to
   `NSUserNotification`), ExceptionHandling and `atos` (to the system's
   crash reports), Carbon (`SetSystemUIMode` → presentation options;
   `FSPathMakeRef` → `NSURL`), Core Image transitions.
4. **Interface.** `MainMenu.nib` to a keyed NIB with Minkar's ibtool (or
   tested through `NSIBObjectData`, which AppKit has); Atlas's icons
   instead of Shiira's; English and Spanish first (Atlas's languages).
5. **Web engine — the main decision:**
   - **A. Keep the legacy `WebView` API.** Minkar's WebKit.framework
     would need `WebView` plus the DOM classes as Objective-C objects over
     WPE, which has no such API: a large layer only Shiira and old apps
     would use.
   - **B. Move Shiira to `WKWebView` (recommended).** Minkar's
     WebKit.framework is planned around `WKWebView` (WPE's model: several
     processes, asynchronous calls). DOM use (form filling, links, images)
     becomes `evaluateJavaScript:`; `WebIconDatabase` becomes the page's
     icon link; `WebInspector` becomes WPE's inspector; `NSURLDownload`
     becomes `WKDownload`/`NSURLSession`. It touches mostly
     `SRWebView`, the controllers of the browser window and the
     form/password code.
6. **Atlas.** Default browser through LaunchServices, downloads revealed
   in Files (`LSRevealInFiles`), passwords in Minkar's Security subset,
   its preferences next to the other apps', screenshots for review.

## 4. Name and logo

Minkar's naming rule: only Minkar and Atlas are proper names;
every other component has a generic name. With the owner's permission, the
maintainer may make Shiira an exception:

- **With permission** (written, from The Shiira Project or its author,
  covering name and logo): the app keeps "Shiira" and its icon; the
  permission is recorded in `TRADEMARKS.md`.
- **Without it:** a generic name ("Web Browser",
  `org.minkar.WebBrowser`) and an Atlas icon, with the credits and the
  BSD notice kept in the About window and in
  `/usr/share/licenses/shiira`.

Worth asking the owner at the same time: the 2.3 source (or the
repository), and the licence of the 9 files without the BSD text.

## 5. Effort and order

| Phase | Work | Depends on | Estimate |
|---|---|---|---|
| S0 | Import 2.2 (or 2.3) source pinned by SHA-256, build files, x86_64 build that compiles | — | 1 week |
| S1 | 64-bit, Carbon/Sparkle/Growl out, NIBs, starts in Minkar without web content | S0 | 1–2 weeks |
| S2 | `WKWebView` (option B): pages, tabs, history, bookmarks, downloads | WebKit.framework (W items) | 3–5 weeks |
| S3 | Atlas integration, English/Spanish, tests in the image, screenshots | S2 | 1–2 weeks |

S0 and S1 can start now, and they also exercise AppKit (bindings,
CoreData, old NIBs, toolbars, sheets) as the reference apps do. S2 waits
for WebKit.framework. Being a Cocoa app, Shiira tests Minkar's own
WebKit.framework, which Firefox (its own engine and toolkit) does not.

## 6. Recommendation

Go ahead with S0–S1 as a second-priority track, after the work in hand
(TextMate, Ruby/Python/Perl, W5), and choose option B for the engine.
Shiira would be Atlas's own light browser for 19.6.x or 20.0; Firefox
remains the browser for 19.6.0.

## Sources

- Shiira files: OSDN mirror, `https://ftp.iij.ad.jp/pub/osdn.jp/shiira/`
- https://en.wikipedia.org/wiki/Shiira
- https://osdn.net/projects/shiira/
