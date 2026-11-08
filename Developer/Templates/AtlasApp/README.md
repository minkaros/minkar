# Atlas app template

`Sample` is the app Atlas's applications (Terminal, System Monitor, Logs
and the others of `Docs/Atlas.md`, chapter 5) start from. It has what
every one of them shares (milestone A0):

- **The standard menus**, made in code with the reference platform's
  commands and keys: the app menu (About, Preferences… ⌘,, Services,
  Hide, Hide Others, Show All, Quit), File (New, Open…, Open Recent,
  Close, Save, Save As…, Revert To: Last Opened and Browse All
  Versions…), Edit, Window and Help (⌘?).
- **The About window** AppKit shows, from `Info.plist`
  (`CFBundleShortVersionString`, `CFBundleVersion`,
  `NSHumanReadableCopyright`) and `Credits.rtf`.
- **A Preferences window** (`SamplePreferences`), one window kept for
  the app, its settings in `NSUserDefaults`.
- **Documents** (`SampleDocument`), an `NSDocument` that autosaves in
  place and keeps versions: Revert To and Browse All Versions… work
  without code of the app's.
- **English and Spanish**: every text goes through `NSLocalizedString`,
  its key the English text; `es.lproj/Localizable.strings` has the
  Spanish, and `*.lproj/InfoPlist.strings` the localized copyright.

To start an app: copy this folder under `CoreServices/` (or wherever the
app belongs), rename `Sample` (the files, the classes' prefix, `APP` in
the `Makefile`, `CFBundleExecutable`, `CFBundleName` and
`CFBundleIdentifier`), set its document types in `Info.plist` (or remove
`CFBundleDocumentTypes` and `SampleDocument` for an app without
documents), and add the app to its stage's `Makefile`.

The image tests (`atlas-app`) build it with a test `main` and check its
About window, its menus in Spanish and a document saved and opened
again.
