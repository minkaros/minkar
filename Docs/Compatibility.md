# Compatibility

How far each designated app's published binary gets on Minkar, and which frameworks those
apps ask for that Minkar does not provide yet. For anyone choosing what to work on next.
Last reviewed: 2026-11-21. Spanish copy: [Compatibility.es.md](Compatibility.es.md).

Framework work is ordered by this page and the noisy-stub log, whatever API level a symbol
belongs to. The generated parts (between the `compat:` markers) come from `bmake progress`;
do not edit them by hand.

## The matrix

Gate BC (`bmake bc-check`; lane BC in [ROADMAP.md](ROADMAP.md)) installs each designated
app's published binary in the image as a user would, starts it as the session's user, and
records how far it got in [progress/compatibility.tsv](progress/compatibility.tsv)
(`BC_UPDATE=yes` writes it).

<!-- compat:matrix:begin -->
_Checked: 2026-11-24; ProductVersion 10.15; the gate took 17 minutes under KVM, with the apps disk._

7 of 17 apps show a window.

### The API 19 set (BC4)

| App | Version | Minimum | Architectures | Result | Menus | Reason |
| --- | --- | --- | --- | --- | --- | --- |
| Firefox | 160.0 | 10.15 | arm64, x86_64 | `window` | yes | as minkar; a window at 0,22, 1280x708 |
| HexFiend | 2.18.1 | 10.14.6 | arm64, x86_64 | `window` | yes | as minkar; a window at 167,275, 367x442 |
| KeepingYouAwake | 1.6.8 | 10.13 | arm64, x86_64 | `no-window` | no | as minkar; running after 40s without a document window (only a panel, 416x169, at level 8); 2026-11-25 01:01:42.412 KeepingYouAwake[385:10495a7c0] unexpected character 76 'v' at 7 |
| Platypus | 5.4.1 | 10.11 | arm64, x86_64 | `window` | yes | as minkar; a window at 308,127, 664x546 |
| Sloth | 3.4 | 10.13 | arm64, x86_64 | `window` | yes | as minkar; a window at 322,97, 892x633 |
| TextMate | 2.0.23 | 10.12 | arm64, x86_64 | `window` | yes | as minkar; a window at 426,164, 427x425 |
| Thunderbird | 160.0 | 10.15 | arm64, x86_64 | `window` | no | as minkar; a window at 0,22, 1280x708 |

### The next designated apps (BC5 on)

| App | Version | Minimum | Architectures | Result | Menus | Reason |
| --- | --- | --- | --- | --- | --- | --- |
| Cyberduck | 9.6.0 | 10.13 | arm64, x86_64 | `at-start` | no | as minkar; exit status 0, after its run loop started;  |
| IINA | 1.5.1 | 11 | arm64, x86_64 | `min-version` | no | requires 11, Minkar reports 10.15; run anyway: no-load: as minkar; dyld: Library not loaded: /System/Library/PrivateFrameworks/PIP.framework/Versions/A/PIP Referenced from: /Applications/IINA.app/Contents/MacOS/IINA Reason: image not found  |
| iTerm2 | 3.7.8 | 13.0 | arm64, x86_64 | `min-version` | no | requires 13.0, Minkar reports 10.15; run anyway: signature: as minkar; MinkarCodeIntegrity: refused /Applications/iTerm.app/Contents/MacOS/iTerm2: restricted entitlement keychain-access-groups outside the trust cache |
| KeePassXC | 2.8.0 | 12.0 | x86_64 | `min-version` | no | requires 12.0, Minkar reports 10.15; run anyway: signature: as minkar; MinkarCodeIntegrity: refused /Applications/KeePassXC.app/Contents/MacOS/KeePassXC: restricted entitlement keychain-access-groups outside the trust cache |
| LibreOffice | 26.8.3 | 11.0.0 | x86_64 | `min-version` | no | requires 11.0.0, Minkar reports 10.15; run anyway: no-load: as minkar; dyld: Library not loaded: /System/Library/Frameworks/Kerberos.framework/Versions/A/Kerberos Referenced from: /Applications/LibreOffice.app/Contents/Frameworks/libmergedlo.dylib Reason: image not found  |
| Skim | 1.7.19 | 10.13 | arm64, x86_64 | `window` | yes | as minkar; a window at 334,104, 480x382 |
| Transmission | 4.1.4 | 12.0 | arm64, x86_64 | `min-version` | no | requires 12.0, Minkar reports 10.15; run anyway: no-load: as minkar; dyld: Library not loaded: /System/Library/Frameworks/QuickLookUI.framework/Versions/A/QuickLookUI Referenced from: /Applications/Transmission.app/Contents/MacOS/Transmission Reason: image not found  |
| UTM | 5.0.9 | 12.0 | arm64, x86_64 | `min-version` | no | requires 12.0, Minkar reports 10.15; run anyway: install: ditto from the disk image failed |
| VLC | 3.0.24 | 10.7.5 | arm64, x86_64 | `no-window` | no | as minkar; running after 40s without a document window (only a panel, 472x195, at level 8); 2026-11-25 01:08:43.774 VLC[1891:1030c47c0] PDF unimplemented: /work/build/minkar-tc/Frameworks/Onyx2D/O2PDFOperators.m 580 SMask not implemented, name=None |
| VSCodium | 1.143.17232 | 12.0 | x86_64 | `min-version` | no | requires 12.0, Minkar reports 10.15; run anyway: install: hdiutil attach: hdiutil: attach failed - no mountable file systems |

Results, from the least far to the furthest:

- `install`: the archive could not be installed
- `signature`: the kernel refused its signature
- `no-load`: dyld could not load it (a library or a symbol is missing)
- `before-main`: it ended before its libraries' initializers had run
- `at-start`: it ended after them, before showing a window
- `no-window`: it runs, but showed no window
- `min-version`: its minimum version is above the one Minkar reports, and it did not show a window (the reason says how far it got)
- `not-run`: the gate did not reach it
- `window`: it shows a window
<!-- compat:matrix:end -->

## Frameworks the designated apps ask for

Every framework a designated app imports that Minkar does not have, and the ones it has
only in part, from
[progress/frameworks.tsv](progress/frameworks.tsv): how many of the 17 apps import it, and
how many of the distinct symbols they import Minkar exports (a noisy stub counts as
exported). To refresh the data, run `tools/symbols/symbol-inventory.py` over the apps and
then `tools/compat/matrix.py --frameworks <its report> Docs/progress/frameworks.tsv`.

Status: **Plan** — in the roadmap; **Partial** — the framework exists with part of its
API; **Decide** — no plan yet; **Out** — outside the decided scope (the vendor's cloud
services) or deprecated.

<!-- compat:frameworks:begin -->
| Framework | Apps | Symbols in Minkar | Status |
| --- | ---: | ---: | --- |
| Metal | 9 | 17 of 60 | Plan (GPU track) |
| VideoToolbox | 6 | 38 of 78 | Partial |
| CoreImage | 5 | 41 of 49 | Partial |
| LocalAuthentication | 5 | 1 of 1 | Partial |
| MediaPlayer | 5 | 17 of 25 | Partial |
| Accelerate | 4 | 0 of 47 | Plan (AU9) |
| CoreLocation | 4 | 7 of 7 | Partial |
| CoreMIDI | 4 | 24 of 25 | Plan (AU8) |
| QuickLookThumbnailing | 4 | 0 of 5 | Decide |
| QuickLookUI | 4 | 0 of 5 | Decide |
| ScreenCaptureKit | 4 | 0 of 14 | Decide |
| Accessibility | 3 | 0 of 3 | Decide |
| AuthenticationServices | 3 | 8 of 38 | Partial |
| ColorSync | 3 | 0 of 10 | Plan |
| CryptoKit | 3 | 0 of 45 | Decide |
| IOBluetooth | 3 | 1 of 8 | Partial |
| MetalKit | 3 | 2 of 7 | Plan (GPU track) |
| Network | 3 | 0 of 14 | Decide |
| SwiftUI | 3 | 0 of 1022 | Plan (lane SU) |
| Vision | 3 | 2 of 21 | Partial |
| AudioUnit | 2 | 14 of 15 | Plan (AU9) |
| Kerberos | 2 | 0 of 27 | Decide |
| OpenDirectory | 2 | 0 of 8 | Partial |
| ScriptingBridge | 2 | 0 of 2 | Decide |
| AccessoryAccess | 1 | 0 of 3 | Decide |
| AppIntents | 1 | 0 of 216 | Decide |
| AVFAudio | 1 | 0 of 5 | Decide |
| Charts | 1 | 0 of 61 | Decide |
| Contacts | 1 | 0 of 10 | Decide |
| CoreBluetooth | 1 | 0 of 10 | Decide |
| CoreDisplay | 1 | 0 of 1 | Decide |
| CoreHaptics | 1 | 0 of 9 | Decide |
| CoreMediaIO | 1 | 0 of 1 | Decide |
| CoreML | 1 | 0 of 9 | Decide |
| CoreWLAN | 1 | 0 of 1 | Decide |
| CryptoTokenKit | 1 | 0 of 1 | Decide |
| DeviceCheck | 1 | 0 of 1 | Out |
| DiskImageKit | 1 | 0 of 33 | Decide |
| ForceFeedback | 1 | 0 of 9 | Decide |
| FoundationModels | 1 | 0 of 70 | Decide |
| GameController | 1 | 0 of 12 | Decide |
| GSS | 1 | 0 of 7 | Decide |
| Hypervisor | 1 | 0 of 19 | Decide |
| JavaRuntimeSupport | 1 | 0 of 23 | Decide |
| MediaAccessibility | 1 | 0 of 1 | Decide |
| MetalFX | 1 | 0 of 4 | Decide |
| ParavirtualizedGraphics | 1 | 0 of 5 | Decide |
| PCSC | 1 | 0 of 13 | Decide |
| PIP (private) | 1 | 0 of 2 | Decide |
| SafariServices | 1 | 0 of 3 | Decide |
| SecurityInterface | 1 | 0 of 3 | Decide |
| StoreKit | 1 | 0 of 3 | Out |
| Symbols | 1 | 0 of 6 | Decide |
| TipKit | 1 | 0 of 28 | Decide |
| Virtualization | 1 | 0 of 63 | Decide |
| VisionKit | 1 | 0 of 40 | Decide |
| vmnet | 1 | 0 of 20 | Decide |
<!-- compat:frameworks:end -->

Status and notes, written by hand: the table takes the status from here, and lists a
framework Minkar has only when it has a line here. A missing framework without a line is
Decide. Symbols missing from the frameworks Minkar has are in the noisy-stub log.

- **Accelerate**: Plan (AU9) — vDSP, BLAS and LAPACK on OpenBLAS (BSD-3).
- **AudioUnit**: Plan (AU9) — the output unit exists; hosting third-party units and CoreAudioKit in AU9.
- **AuthenticationServices**: Partial — no sign-on providers.
- **ColorSync**: Plan — on Little CMS.
- **CoreImage**: Partial — CIFilter's registry, image units and a context over CoreGraphics; kernels and most filters left.
- **CoreLocation**: Partial — location services off.
- **CoreMIDI**: Plan (AU8) — clients and ports; endpoints and the MIDI service in AU8.
- **DeviceCheck**: Out — the vendor's cloud service.
- **IOBluetooth**: Partial — no Bluetooth driver.
- **LocalAuthentication**: Partial — nothing to authenticate with yet.
- **MediaPlayer**: Partial — now playing and remote commands.
- **Metal**: Plan (GPU track) — the API exists without a device; darling-metal and Indium.
- **MetalKit**: Plan (GPU track) — `MTKView` draws for its delegate without a drawable.
- **OpenDirectory**: Partial — the C interface exists; the Objective-C classes the apps import do not.
- **ScriptingBridge**: Decide — needs the scripting component (Phase 6).
- **StoreKit**: Out — the vendor's cloud service.
- **SwiftUI**: Plan (lane SU) — Minkar's own public layer on a SwiftCrossUI-based engine; see [SwiftUI.md](SwiftUI.md).
- **VideoToolbox**: Partial — decoding (H.264, HEVC, VP9, AV1) through FFmpeg; no encoder.
- **Vision**: Partial — no text recognition.

**No designated app imports these yet** (they leave the list when one does): Accounts,
AGL, AppleScriptKit, AppleScriptObjC, AudioVideoBridging, Automator, BusinessChat,
CalendarStore, CloudKit, Collaboration, ContactsUI, CoreAudioKit, CoreSpotlight,
DirectoryService, DriverKit, DVDPlayback, EndpointSecurity, EventKit, ExecutionPolicy,
ExternalAccessory, FileProvider, FileProviderUI, FinderSync, GameKit, GameplayKit, GLKit,
GLUT, ICADevices, ImageCaptureCore, InputMethodKit, InstallerPlugins, IOBluetoothUI,
IOUSBHost, JavaVM, LDAP, LinkPresentation, MapKit, MediaLibrary, MediaToolbox,
MetalPerformanceShaders, ModelIO, MultipeerConnectivity, NaturalLanguage, NetFS,
NetworkExtension, NotificationCenter, OpenAL, OpenCL, PencilKit, Photos, PhotosUI, PushKit,
Python, RealityKit, Ruby, SceneKit, ScreenSaver, SecurityFoundation, Social, SoundAnalysis,
Speech, SpriteKit, SyncServices, SystemExtensions, Tk, TWAIN, VideoSubscriberAccount.

## What Minkar provides

AddressBook, AppKit, ApplicationServices (with PrintCore), AudioToolbox, AudioUnit (the
output unit), AVFoundation, AVKit, Carbon, CFNetwork, Cocoa, Combine, CoreAudio, CoreData,
CoreFoundation, CoreGraphics, CoreImage (part), CoreMedia, CoreServices, CoreText,
CoreVideo, DiskArbitration, ExceptionHandling, Foundation, ImageIO, IOKit, IOSurface,
MediaPlayer, OpenDirectory (part), OpenGL (Mesa, in views and layers), OSAKit, PDFKit,
PreferencePanes, Quartz, QuartzCore, QuickLook, Security (a subset), SecurityInterface (the lock), ServiceManagement,
System, SystemConfiguration, UniformTypeIdentifiers, UserNotifications, VideoToolbox
(decoding), WebKit (on WPE WebKit's engine), XCTest, and JavaScriptCore (private).
Present but waiting on hardware or a later block: CoreMIDI, Metal, MetalKit, Vision,
CoreLocation, LocalAuthentication, AuthenticationServices, IOBluetooth.

## System services Minkar lacks

| Service | Status | Notes |
| --- | --- | --- |
| Search indexing | Plan (19.6.x, K4) | `MDItem` and `mdls`, `mdfind` (it searches the disk), `mdutil`; no indexer yet (decided 2026-11-23) |
| Printing to printers | Done | `cupsd` and its commands, the Printers pane, printing through PrintCore to IPP Everywhere printers that take PDF |
| Wi-Fi and Bluetooth | Decide | No drivers yet |
| Camera and video capture | Decide | |
| Authorization (authd, rights, administrator's password) | Done | APSL authd and client, Atlas's Authorization Agent, `AuthorizationExecuteWithPrivileges`, the panes' lock (SecurityInterface) |
| Keychain with encryption | Plan | The current Security subset is replaced by the APSL Security sources |
| Backups (`tmutil`) | Plan (19.6.x) | Moved with backups (decided 2026-11-23) |
| Accessibility (screen reader, accessibility server) | Decide | |
| Voice assistant, dictation, cloud storage, app store, device handoff | Out | The vendor's services |
