# Minkar — What API 19 has that Minkar lacks

[Español](MissingFrameworks.es.md)

The public frameworks and system services of API 19 that Minkar does not
provide yet, to choose what to implement after the blocks already planned
(WebKit W5, audio AU7–AU10, the GPU track). Status as of 16 Nov 2026.

How to read it:

* **Status:** *Plan* — already in [ROADMAP.md](ROADMAP.md); *Partial* — the
  framework exists with part of its API (the notes say which); *Decide* — no
  plan yet; *Out* — outside the decided scope (cloud services of the
  reference platform's vendor) or deprecated.
* **Apps:** how many of the reference apps import the framework
  (`tools/symbols/symbol-inventory.py`), and how many symbols. A framework
  no reference app imports can still matter: the apps measured are few.

The list of API 19 frameworks is taken from the public documentation, not
from the vendor's SDK (clean room). A name may be missing or extra; the
measurement is what decides.

## What Minkar has

AddressBook, AppKit, ApplicationServices (with PrintCore), AudioToolbox,
AudioUnit (the output unit), AVFoundation, AVKit, Carbon, CFNetwork, Cocoa,
Combine, CoreAudio, CoreData, CoreFoundation, CoreGraphics, CoreMedia,
CoreServices, CoreText, CoreVideo, DiskArbitration, ExceptionHandling,
Foundation, ImageIO, IOKit, IOSurface, MediaPlayer (now playing and remote
commands), OpenDirectory, OpenGL (Mesa, in views and layers), OSAKit,
PreferencePanes, Quartz, QuartzCore, QuickLook, Security (a subset),
ServiceManagement, System, SystemConfiguration, UniformTypeIdentifiers,
UserNotifications, VideoToolbox (decoding: H.264, HEVC, VP9, AV1; no
encoder), WebKit (on WPE WebKit's engine), XCTest, and JavaScriptCore
(private, in `/usr/lib/webkit`).

Present with what the hardware or a later block gives them: CoreMIDI
(clients and ports, no endpoints until AU8), Metal (no device until the GPU
track), Vision (no text recognition), CoreLocation (location services off),
LocalAuthentication (nothing to authenticate with yet),
AuthenticationServices (no sign-on providers), IOBluetooth (no Bluetooth
driver).

## By layer

The system in four layers, from the kernel up, and where each stands.

| Layer | What Minkar has | What is missing |
| --- | --- | --- |
| **Kernel and core OS** | XNU (Darwin 19.6.0: Mach and BSD), I/O Kit, HFS+, FAT and exFAT, the BSD network stack with Ethernet (I219, I211, Broadcom), HDA sound and HDMI audio, the sandbox policy, AMD displays (R1, R2 on the RX 6600) | USB audio (AU7), Wi-Fi and Bluetooth drivers, the GPU's engines (G1–G5, under way), enforced code signing (decided: static trust cache and Minkar's MAC kext), APFS (after 19.6.0) |
| **Core services** | CoreFoundation, CoreGraphics, CoreText, ImageIO, QuartzCore, CoreServices (LaunchServices, `AppleEvent`, FSEvents), CFNetwork, a Security subset, CoreAudio, CoreMedia, AVFoundation, VideoToolbox (decoding), OpenGL | Metal with a device (GPU track), Accelerate, CoreImage (most filters and kernels), Network, CryptoKit, ColorSync, a search indexer, the APSL Security sources, a PostScript interpreter (CGPSConverter is there and reports it cannot convert) |
| **Application frameworks** | Cocoa: AppKit and Foundation, parts of Carbon, WebKit (W2–W4), AVKit, PrintCore and printing | The reference apps' web views (W5), PDFKit and the rest listed below; SwiftUI comes through OpenSwiftUI (lane SU) |
| **User experience** | Atlas, with its own visual language: login window, menu bar, Dock, Files, Preferences, the notification center (N1, N2) | The desktop complete for daily use (MB2–MB3, D2–D3, K2–K3, apps A0–A10), search, accessibility (screen reader) |

## Missing frameworks

### Audio and video

| Framework | Status | Apps | Notes |
| --- | --- | --- | --- |
| AudioUnit (hosting third-party units), CoreAudioKit | Plan (AU9) | | The output unit exists; hosting and its views in AU9 |
| CoreMIDI | Plan (AU8) | | The framework exists; endpoints and the service in AU8 |
| VideoToolbox (encoding), MediaToolbox | Decide | | Decoding exists (FFmpeg in the app's process) |
| CoreMediaIO | Decide | | Cameras and capture devices |
| OpenAL | Decide | | Can sit on CoreAudio |
| MediaLibrary | Decide | | MediaPlayer exists |
| AudioVideoBridging | Decide | | Network audio (AVB) |
| SoundAnalysis, Speech | Decide | | |
| DVDPlayback | Out | | Deprecated |

### Graphics, 3D and computation

| Framework | Status | Apps | Notes |
| --- | --- | --- | --- |
| Metal (a device), MetalKit | Plan (GPU track) | | darling-metal and Indium; Metal's API exists without a device, and MetalKit's MTKView (since 19 Nov 2026, which Skim needs) draws for its delegate without a drawable, as on the reference without a device |
| Accelerate | Plan (AU9) | | vDSP, BLAS and LAPACK (OpenBLAS, BSD-3); used widely, worth moving earlier |
| CoreImage | Partial | | Skim needs it. Its own framework since 19 Nov 2026 (QuartzCore re-exports it): CIFilter with its registry, attributes and image units (CIPlugIn), the public constants, and a context drawing in linear light over CoreGraphics; built-in filters only those it draws (CIBoxBlur, CILinearGradient). Kernels (CIKernel) and the rest of the filters to come |
| MetalPerformanceShaders | Decide | | |
| SceneKit, SpriteKit, ModelIO, GameplayKit, RealityKit | Decide | | |
| ColorSync | Decide | | Little CMS is already planned |
| OpenCL, GLKit, GLUT, AGL | Decide | | Deprecated, still used by some apps |
| CoreML, NaturalLanguage; Vision's recognition | Decide | | Vision exists without recognition |

### User interface and app integration

| Framework | Status | Apps | Notes |
| --- | --- | --- | --- |
| SwiftUI | Plan (lane SU) | | Decided 2026-11-20 (D6): OpenSwiftUI and OpenAttributeGraph after a clean-room review (SU0b), installed as `SwiftUI.framework` (SU1–SU2), the designated binaries' symbols (SU3–SU4); SU0 reports which apps link it |
| PDFKit | Decide | | Atlas's document viewer needs it |
| QuickLookThumbnailing | Decide | | QuickLook exists |
| ScreenSaver, InputMethodKit, LinkPresentation, PencilKit | Decide | | |
| FinderSync, NotificationCenter, SafariServices | Decide | | Integration with Files and Atlas |
| ScriptingBridge, AppleScriptKit, AppleScriptObjC, Automator | Decide | | Need the scripting component (Phase 6) |
| ImageCaptureCore, ICADevices, TWAIN | Decide | | Scanners and cameras |
| SecurityInterface, SecurityFoundation | Decide | | |
| Collaboration, InstallerPlugins | Decide | | ExceptionHandling exists |

### Personal data and network services

| Framework | Status | Apps | Notes |
| --- | --- | --- | --- |
| Contacts, ContactsUI, EventKit | Decide | | AddressBook exists |
| CoreSpotlight | Decide | | Goes with a search indexer; `MDItem` exists |
| Photos, PhotosUI | Decide | | |
| MapKit; CoreLocation's provider | Decide | | Need a map or location provider; CoreLocation exists, its services off |
| FileProvider, FileProviderUI | Decide | | |
| MultipeerConnectivity | Decide | | |
| CloudKit, GameKit, StoreKit, Accounts, Social, PushKit, DeviceCheck, BusinessChat, VideoSubscriberAccount | Out | | Depend on the vendor's cloud services |
| SyncServices, CalendarStore | Out | | Deprecated |

### Security, networking and hardware

| Framework | Status | Apps | Notes |
| --- | --- | --- | --- |
| CryptoKit | Decide | | Used widely from Swift |
| CryptoTokenKit, PCSC; LocalAuthentication's and AuthenticationServices' providers | Decide | | The two frameworks exist, with nothing to authenticate with |
| GSS, Kerberos, LDAP | Decide | | |
| Network | Decide | | Used widely by Swift apps |
| NetworkExtension, NetFS | Decide | | |
| CoreWLAN, CoreBluetooth, IOBluetoothUI | Decide | | Need Wi-Fi and Bluetooth drivers; IOBluetooth exists without devices |
| IOUSBHost, ExternalAccessory, GameController, ForceFeedback, CoreHaptics | Decide | | |
| SystemExtensions, DriverKit, EndpointSecurity, ExecutionPolicy | Decide | | |
| Hypervisor | Decide | | |
| DirectoryService, JavaVM; Python, Ruby, Tcl and Tk as frameworks | Out | | Deprecated |

## Missing system services

| Service | Status | Notes |
| --- | --- | --- |
| Search indexing | Decide | `MDItem` exists, without an indexer |
| Printing to printers (a CUPS server) | Decide | PrintCore, NSPrinter and printing to PDF exist (16 Nov 2026); libcups is there, `cupsd` is not, so there are no printers |
| Wi-Fi and Bluetooth (drivers and services) | Decide | Nothing yet |
| Camera and video capture | Decide | |
| Keychain service with encryption | Plan | The current subset is replaced by the APSL Security sources |
| Backups (`tmutil`) | Plan (Phase 6) | |
| Enforced code signing | Plan | Static trust cache and Minkar's MAC kext |
| Accessibility (screen reader, accessibility server) | Decide | |
| Voice assistant, dictation, cloud storage, app store, device handoff | Out | The vendor's services |

## Suggested order

Now that WebKit's engine (W2–W4) and audio and media (AU1–AU6) are done, by
how much software needs each and how little special hardware it takes:

1. Accelerate and CoreImage.
2. Network and CryptoKit.
3. PDFKit and QuickLook (Atlas's apps).
4. Printers (`cupsd`; PrintCore and printing to PDF are done).
5. Wi-Fi and Bluetooth (much driver work; needed on laptops).

The measurement (column *Apps*) and more reference apps may change it.
