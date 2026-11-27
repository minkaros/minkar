# A guide to the source tree

Where things are in Minkar's tree, and in what order they are built. Last reviewed:
2026-11-27. The top-level `Makefile` builds these stages in turn (`bmake
ONLY_STAGE=<stage> <stage>` builds one alone):

`BSD/hier`, `Developer`, `Kernel`, `Libraries`, `Frameworks`,
`Kernel/Extensions`, `CoreServices`, `BSD`, `Frameworks/WebKit`,
`SystemLibrary`; then `bmake image` makes the disk image.

[BUILDING.md](BUILDING.md) says how to build; [ROADMAP.md](ROADMAP.md) what
is done and what comes next; [Structure.md](Structure.md) which parts are
planned to leave the tree, and when.

## Kernel

* `xnu` - the kernel (Darwin 19.6.0): Mach (`osfmk`), BSD (`bsd`), IOKit
  (`iokit`), libkern, the platform expert (`pexpert`), the MAC framework
  (`security`), and their build rules (`makedefs`, `config`, `SETUP`).
* `Extensions` - the kernel extensions, built in their own stage after
  `Libraries` and `Frameworks`, whose headers they use:
  * IOKit families: `IOACPIFamily`, `IOATAFamily`, `IOAudioFamily`,
    `IOGraphics`, `IOHIDFamily`, `IONetworkingFamily`, `IOPCIFamily`,
    `IOStorageFamily`, `IOUSBFamily`.
  * Minkar's drivers: `MinkarAHCIPort` (SATA), `MinkarNVMe`,
    `MinkarXHCIPort` and `MinkarUSB` (USB, HID), `MinkarE1000` (Intel
    Ethernet), `MinkarBGE` (Broadcom Ethernet), `MinkarHDA` (sound),
    `MinkarVirtio`, `MinkarDRM` (AMD GPUs: displays and the GPU's
    engines; [GPU.md](GPU.md#rdna2-pc-1)), `MinkarDiskImage`,
    `MinkarSandbox` (the sandbox policy), `MinkarCodeIntegrity`.
  * File systems: `hfs`, `msdosfs`, `exfat`; `AppleFileSystemDriver`.
  * Platform: `AppleAPIC`, `AppleI386PCI`, `AppleIntelPIIXATA`,
    `ApplePS2Controller`, `corecrypto`, `pthread`.
* `IOKitUser` - IOKit's user-space library (`IOKit.framework`).
* `kext_tools` - `kextcache` and the tools that build the kernelcache.

## Libraries

The libraries under `/usr/lib`, most of them parts of `libSystem`:

* `Libsystem` - libSystem and its parts (`libsystem_c`, `libsystem_kernel`,
  `libsystem_malloc`, `libsystem_platform`, `libsystem_pthread`,
  `libsystem_info`, `libsystem_notify`, `libsystem_sandbox`, `liblaunch`,
  `libdispatch`, `CommonCrypto`, ...).
* `dyld` - the dynamic linker.
* `objc4` - the Objective-C runtime.
* `swift` - the Swift 6.4.2 runtime and overlays, in `/usr/lib/swift`.
* `ICU`, `libiconv`, `zlib`, `mbedtls`, `openbsm`, `OpenPAM`, `OpenSSH`,
  `MiscLibs`, `CrashReporterClient`, `libfirehose_kernel`, `bootstrap`.

## Frameworks

The Cocoa API and the frameworks under it, in `/System/Library/Frameworks`.
Some came from Cocotron; Minkar completes them to the API of the reference
system ([Compatibility.md](Compatibility.md#frameworks-the-designated-apps-ask-for) lists what is missing).

* Core: `CoreFoundation` (CF-Lite), `Foundation`, `CoreServices`
  (LaunchServices, FSEvents, AE), `CFNetwork`, `SystemConfiguration`,
  `DiskArbitration`, `IOSurface`, `Security`, `OpenDirectory`,
  `ServiceManagement`, `UniformTypeIdentifiers`.
* Drawing and the interface: `CoreGraphics` on `Onyx2D` (the 2D renderer),
  `CoreText`, `ImageIO`, `QuartzCore`, `AppKit`, `Cocoa`, `Quartz`,
  `QuickLook`, `PreferencePanes`, `ApplicationServices` (with `PrintCore`),
  `Carbon`, `OSAKit`.
* 3D: `OpenGL` (CGL, Mesa underneath), `Metal`.
* Media: `CoreAudio`, `AudioToolbox`, `AudioUnit`, `CoreMIDI`, `CoreMedia`,
  `CoreVideo`, `VideoToolbox`, `AVFoundation`, `AVKit`, `MediaPlayer`, `Accelerate`,
  `ScreenCaptureKit`.
* Others: `WebKit` (the Cocoa API over WPE WebKit), `UserNotifications`,
  `AddressBook`, `AuthenticationServices`, `LocalAuthentication`, `Accessibility`,
  `CoreLocation`, `IOBluetooth`, `Vision`, `ScriptingBridge`, `Combine` (OpenCombine),
  `ExceptionHandling`, `XCTest`.

`PrivateFrameworks` holds `SkyLight` (the window server's client library,
and the window server itself under `SkyLight/Server`), `CoreUtils` and
`ktrace`.

## CoreServices

The system's services and its desktop, Atlas ([Atlas.md](Atlas.md)):

* Services: `WindowServer` (with SkyLight), `launchservicesd`, `pboard`
  (the pasteboard), `iosurfaced`, `notificationd` (the notification
  center), `distnoted` (distributed notifications), `audiod`, `mediadecoder`,
  `fseventsd`, `logd`, `directoryd`, `dirhelper`, `fontcache`.
* The desktop and its apps: `loginwindow`, `Dock`, `SystemUIServer` (the
  menu bar's extras, notifications), `Files` (the file manager),
  `SystemPreferences`, `Terminal`, `TextEditor`, `SystemMonitor`, `Logs`.

## BSD

The Unix commands and their libraries ([Userland.md](Userland.md),
[Userland.md](Userland.md#command-line-tools)): `bin`, `sbin`, `usr.bin`, `usr.sbin`, `libexec`,
`lib` (libraries, among them the external ones the media and WebKit use:
FFmpeg, dav1d, Mesa, WPE WebKit and its dependencies), `etc`, `share` (also
the `mk` files bmake builds with), `hier` (the file system's layout) and
`syslog`; `launchd` is in `sbin`.

## Developer

The SDK and the toolchain apps are built with, built first for the host:

* `Default.xctoolchain` - LLVM 21.1.6 (Swift's fork), Swift 6.4.2,
  swift-build, `cctools` (ld64, lipo, otool), `codesign`, `mig`, `iig`,
  `ibtool`, `plktool`, `dtrace_ctf`, `gmake`, `rpcgen`, `unifdef`, `xar`;
  [tools/toolchain/MANIFEST](../tools/toolchain/MANIFEST) records where each
  comes from.
* `Minkar.platform`, `Minkar.sdk` and `InternalSDK` - the SDK.
* `Templates` - project templates (the Atlas app template).

## SystemLibrary

What goes to `/System/Library` and `/Library` that is not built elsewhere:
`LaunchDaemons`, `Fonts` (pinned by their manifests), `Sounds`, `Icons`,
`Colors`, `Desktop_Pictures`, the user template, and `SystemVersion.plist`,
filled in from the top-level `Makefile`.

## Image, Tests and tools

* `Image` - the disk image (`mkdiskimage.py`), the EFI loader's patches and
  the boot test (`boottest.py`).
* `Tests` - gates built as part of the tree: `M3` (command-line Swift
  packages), `W0`, `W1`, `W2` (WebKit) and `Gallery`.
* `tools` - build and check tools: `toolchain` (the toolchain's manifest),
  `check` (sysroot and command reports), `ci`, `symbols` (the reference
  apps' imports, the coverage measured against them), `progress` (the
  progress pages), `drm-import` (register tables from amdgpu),
  `mktrustcache.py`, symbolication and crash and panic reports.
