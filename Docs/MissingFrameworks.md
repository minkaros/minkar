# Minkar — What API 19 has that Minkar lacks

[Español](MissingFrameworks.es.md)

The public frameworks and system services of API 19 that Minkar does not
provide yet, to choose what to implement after the blocks already planned
(WebKit W2–W5, audio and media AU1–AU10). Status as of 10 Nov 2026.

How to read it:

* **Status:** *Plan* — already in [ROADMAP.md](ROADMAP.md); *Decide* — no
  plan yet; *Out* — outside the decided scope (cloud services of the
  reference platform's vendor, SwiftUI) or deprecated.
* **Apps:** how many of the reference apps import the framework
  (`tools/symbols/symbol-inventory.py`), and how many symbols. A framework
  no reference app imports can still matter: the apps measured are few.

The list of API 19 frameworks is taken from the public documentation, not
from the vendor's SDK (clean room). A name may be missing or extra; the
measurement is what decides.

## What Minkar has

AddressBook, AppKit, ApplicationServices, AudioToolbox (no sound output
yet), Carbon, CFNetwork, Cocoa, Combine, CoreData, CoreFoundation,
CoreGraphics, CoreServices, CoreText, CoreVideo, DiskArbitration,
Foundation, ImageIO, IOKit, IOSurface, OpenDirectory, OpenGL, OSAKit,
PreferencePanes, Quartz, QuartzCore, Security (a subset), ServiceManagement,
System, SystemConfiguration, UniformTypeIdentifiers, UserNotifications,
WebKit (the API; its engine is W2), XCTest, and JavaScriptCore (private, in
`/usr/lib/webkit`).

## By layer

The system in four layers, from the kernel up, and where each stands.

| Layer | What Minkar has | What is missing |
| --- | --- | --- |
| **Kernel and core OS** | XNU (Darwin 19.6.0: Mach and BSD), I/O Kit, HFS+, FAT and exFAT, the BSD network stack with Ethernet (I219, I211) | Audio drivers (AU1, AU7), Wi-Fi and Bluetooth drivers, the GPU drivers (G1–G5, under way), enforced code signing (decided: static trust cache and Minkar's MAC kext), APFS (after 19.6.0) |
| **Core services** | CoreFoundation, CoreGraphics, CoreText, ImageIO, QuartzCore, CoreServices (LaunchServices, `AppleEvent`, FSEvents), CFNetwork, a Security subset | CoreAudio and CoreMedia/AVFoundation (AU1–AU6), Metal (GPU track), Accelerate, CoreImage, Network, CryptoKit, ColorSync, a search indexer, the APSL Security sources |
| **Application frameworks** | Cocoa: AppKit and Foundation (imported symbols 99.9 % against the reference apps), parts of Carbon, the WebKit API | WebKit's engine (W2–W5), PDFKit, QuickLook and the rest listed below; SwiftUI is out of scope for API 19–21 |
| **User experience** | Atlas, with its own visual language: login window, menu bar, Dock, Files, Preferences | The desktop complete for daily use (MB2–MB3, D2–D3, K2–K3, apps A0–A10), search, notification banners, accessibility (screen reader) |

## Missing frameworks

### Audio and video

| Framework | Status | Apps | Notes |
| --- | --- | --- | --- |
| CoreAudio | Plan (AU2) | | Audio service and the `AudioObject` API |
| AudioUnit | Plan (AU3, AU9) | | Output unit; hosting third-party units in AU9 |
| CoreMIDI | Plan (AU8) | | Framework and service |
| CoreAudioKit | Plan (AU9) | | Audio unit views |
| AVFoundation, AVKit, CoreMedia | Plan (AU5) | | On FFmpeg and dav1d |
| VideoToolbox, MediaToolbox | Decide | | Fit in AU5 |
| CoreMediaIO | Decide | | Cameras and capture devices |
| OpenAL | Decide | | Can sit on CoreAudio |
| MediaPlayer, MediaLibrary | Decide | | |
| AudioVideoBridging | Decide | | Network audio (AVB) |
| SoundAnalysis, Speech | Decide | | |
| DVDPlayback | Out | | Deprecated |

### Graphics, 3D and computation

| Framework | Status | Apps | Notes |
| --- | --- | --- | --- |
| Metal, MetalKit | Plan (GPU track) | | darling-metal and Indium |
| Accelerate | Plan (AU9) | | vDSP, BLAS and LAPACK (OpenBLAS, BSD-3); used widely, worth moving earlier |
| CoreImage | Decide | | Used widely by image apps |
| MetalPerformanceShaders | Decide | | |
| SceneKit, SpriteKit, ModelIO, GameplayKit, RealityKit | Decide | | |
| ColorSync | Decide | | Little CMS is already planned |
| OpenCL, GLKit, GLUT, AGL | Decide | | Deprecated, still used by some apps |
| CoreML, Vision, NaturalLanguage | Decide | | |

### User interface and app integration

| Framework | Status | Apps | Notes |
| --- | --- | --- | --- |
| SwiftUI | Out | | Decided for API 19–21; OpenSwiftUI watched |
| PDFKit | Decide | | Atlas's document viewer needs it |
| QuickLook, QuickLookThumbnailing | Decide | | Quartz already has part of it |
| ScreenSaver, InputMethodKit, LinkPresentation, PencilKit | Decide | | |
| FinderSync, NotificationCenter, SafariServices | Decide | | Integration with Files and Atlas |
| ScriptingBridge, AppleScriptKit, AppleScriptObjC, Automator | Decide | | Need the scripting component (Phase 6) |
| ImageCaptureCore, ICADevices, TWAIN | Decide | | Scanners and cameras |
| SecurityInterface, SecurityFoundation | Decide | | |
| Collaboration, ExceptionHandling, InstallerPlugins | Decide | | |

### Personal data and network services

| Framework | Status | Apps | Notes |
| --- | --- | --- | --- |
| Contacts, ContactsUI, EventKit | Decide | | AddressBook exists |
| CoreSpotlight | Decide | | Goes with a search indexer; `MDItem` exists |
| Photos, PhotosUI | Decide | | |
| MapKit, CoreLocation | Decide | | Need a map or location provider |
| FileProvider, FileProviderUI | Decide | | |
| MultipeerConnectivity | Decide | | |
| CloudKit, GameKit, StoreKit, Accounts, Social, PushKit, DeviceCheck, BusinessChat, VideoSubscriberAccount | Out | | Depend on the vendor's cloud services |
| SyncServices, CalendarStore | Out | | Deprecated |

### Security, networking and hardware

| Framework | Status | Apps | Notes |
| --- | --- | --- | --- |
| CryptoKit | Decide | | Used widely from Swift |
| LocalAuthentication, AuthenticationServices, CryptoTokenKit, PCSC | Decide | | |
| GSS, Kerberos, LDAP | Decide | | |
| Network | Decide | | Used widely by Swift apps |
| NetworkExtension, NetFS | Decide | | |
| CoreWLAN, CoreBluetooth, IOBluetooth, IOBluetoothUI | Decide | | Need Wi-Fi and Bluetooth drivers |
| IOUSBHost, ExternalAccessory, GameController, ForceFeedback, CoreHaptics | Decide | | |
| SystemExtensions, DriverKit, EndpointSecurity, ExecutionPolicy | Decide | | |
| Hypervisor | Decide | | |
| DirectoryService, JavaVM; Python, Ruby, Tcl and Tk as frameworks | Out | | Deprecated |

## Missing system services

| Service | Status | Notes |
| --- | --- | --- |
| Audio service | Plan (AU2) | |
| Search indexing | Decide | `MDItem` exists, without an indexer |
| Printing (CUPS and PrintCore) | Decide | Nothing yet |
| Wi-Fi and Bluetooth (drivers and services) | Decide | Nothing yet |
| Camera and video capture | Decide | |
| Keychain service with encryption | Plan | The current subset is replaced by the APSL Security sources |
| Notification banners | Decide | The API exists; nothing is shown |
| Backups (`tmutil`) | Plan (Phase 6) | |
| Enforced code signing | Plan | Static trust cache and Minkar's MAC kext |
| Accessibility (screen reader, accessibility server) | Decide | |
| Voice assistant, dictation, cloud storage, app store, device handoff | Out | The vendor's services |

## Suggested order

After W2 and AU1–AU6, by how much software needs each and how little
special hardware it takes:

1. Accelerate and CoreImage.
2. Network and CryptoKit.
3. PDFKit and QuickLook (Atlas's apps).
4. Printing (CUPS).
5. Wi-Fi and Bluetooth (much driver work; needed on laptops).

The measurement (column *Apps*) and more reference apps may change it.
