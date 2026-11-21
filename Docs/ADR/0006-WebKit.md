# ADR 0006: WebKit

Which web engine Minkar's `WebKit.framework` uses, for anyone working on WebKit or an app
with a web view. Status: accepted, 2026-11-05; recorded 2026-11-21.

## Context

Many designated apps show HTML in a web view (`WKWebView`, or the older `WebView`): release
notes, help, previews. They need the Cocoa API of `WebKit.framework`. The reference
platform's port of WebKit depends on closed frameworks; the open ports each bring a
toolkit.

## Decision

- **Engine:** WPE WebKit 2.54 (LGPL and BSD), the port made for embedding, built as
  `PLATFORM(WPE)` on the Darwin services WTF already knows (Mach virtual memory, threads,
  libdispatch), with a Minkar platform for its WPE Platform API. It draws with the Skia it
  carries.
- **API:** Minkar's own `WebKit.framework` with the Cocoa API on top: `WKWebView` and its
  configuration, preferences, user scripts and delegates, and `WebView`, `WebFrame` and
  their delegates on the same engine.
- **Dependencies:** fetched at pinned versions with their SHA-256 and built as dynamic
  libraries (glib, libsoup and the LGPL parts are never linked statically).
- **Media:** WebKit's audio and video go through Minkar's AVFoundation and CoreAudio
  ([ADR 0005](0005-Audio-And-Media.md)).
- **The browser** is Firefox, run from its published binary; WebKit serves the apps' web
  views, not a browser of Minkar's.

## Consequences

- WebKit W0–W5 are part of Minkar 19.6.0 ([ROADMAP.md](../ROADMAP.md#webkit)).
- The toolchain had to move to clang 18 or later (LLVM 21, 2026-11-09).
