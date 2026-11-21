# Guía del árbol de fuentes

*Traducción de [SourceMap.md](SourceMap.md); el documento en inglés es el de referencia.*

Dónde está cada cosa en el árbol de Minkar y en qué orden se compila. El
`Makefile` principal compila estas fases una tras otra (`bmake
ONLY_STAGE=<fase> <fase>` compila una sola):

`BSD/hier`, `Developer`, `Kernel`, `Libraries`, `Frameworks`,
`Kernel/Extensions`, `CoreServices`, `BSD`, `Frameworks/WebKit`,
`SystemLibrary`; después `bmake image` crea la imagen de disco.

[BUILDING.es.md](BUILDING.es.md) explica cómo compilar;
[ROADMAP.es.md](ROADMAP.es.md), qué está hecho y qué viene después;
[Structure.es.md](Structure.es.md), qué partes está previsto sacar del árbol, y
cuándo.

## Kernel

* `xnu` - el kernel (Darwin 19.6.0): Mach (`osfmk`), BSD (`bsd`), IOKit
  (`iokit`), libkern, el platform expert (`pexpert`), el framework MAC
  (`security`) y sus reglas de compilación (`makedefs`, `config`, `SETUP`).
* `Extensions` - las extensiones del kernel, compiladas en su propia fase
  después de `Libraries` y `Frameworks`, cuyas cabeceras usan:
  * Familias de IOKit: `IOACPIFamily`, `IOATAFamily`, `IOAudioFamily`,
    `IOGraphics`, `IOHIDFamily`, `IONetworkingFamily`, `IOPCIFamily`,
    `IOStorageFamily`, `IOUSBFamily`.
  * Los drivers de Minkar: `MinkarAHCIPort` (SATA), `MinkarNVMe`,
    `MinkarXHCIPort` y `MinkarUSB` (USB, HID), `MinkarE1000` (Ethernet de
    Intel), `MinkarBGE` (Ethernet de Broadcom), `MinkarHDA` (sonido),
    `MinkarVirtio`, `MinkarDRM` (GPU de AMD: pantallas y los motores de la
    GPU; [GPU-RDNA2.es.md](GPU-RDNA2.es.md)), `MinkarDiskImage`,
    `MinkarSandbox` (la política del sandbox), `MinkarCodeIntegrity`.
  * Sistemas de archivos: `hfs`, `msdosfs`, `exfat`; `AppleFileSystemDriver`.
  * Plataforma: `AppleAPIC`, `AppleI386PCI`, `AppleIntelPIIXATA`,
    `ApplePS2Controller`, `corecrypto`, `pthread`.
* `IOKitUser` - la biblioteca de IOKit en espacio de usuario
  (`IOKit.framework`).
* `kext_tools` - `kextcache` y las herramientas que construyen el
  kernelcache.

## Libraries

Las bibliotecas de `/usr/lib`, la mayoría partes de `libSystem`:

* `Libsystem` - libSystem y sus partes (`libsystem_c`, `libsystem_kernel`,
  `libsystem_malloc`, `libsystem_platform`, `libsystem_pthread`,
  `libsystem_info`, `libsystem_notify`, `libsystem_sandbox`, `liblaunch`,
  `libdispatch`, `CommonCrypto`, ...).
* `dyld` - el enlazador dinámico.
* `objc4` - el runtime de Objective-C.
* `swift` - el runtime de Swift 6.4.2 y sus overlays, en `/usr/lib/swift`.
* `ICU`, `libiconv`, `zlib`, `mbedtls`, `openbsm`, `OpenPAM`, `OpenSSH`,
  `MiscLibs`, `CrashReporterClient`, `libfirehose_kernel`, `bootstrap`.

## Frameworks

La API de Cocoa y los frameworks que tiene debajo, en
`/System/Library/Frameworks`. Algunos vienen de Cocotron; Minkar los
completa hasta la API del sistema de referencia
([MissingFrameworks.es.md](MissingFrameworks.es.md) dice qué falta).

* Base: `CoreFoundation` (CF-Lite), `Foundation`, `CoreServices`
  (LaunchServices, FSEvents, AE), `CFNetwork`, `SystemConfiguration`,
  `DiskArbitration`, `IOSurface`, `Security`, `OpenDirectory`,
  `ServiceManagement`, `UniformTypeIdentifiers`.
* Dibujo e interfaz: `CoreGraphics` sobre `Onyx2D` (el motor 2D),
  `CoreText`, `ImageIO`, `QuartzCore`, `AppKit`, `Cocoa`, `Quartz`,
  `QuickLook`, `PreferencePanes`, `ApplicationServices` (con `PrintCore`),
  `Carbon`, `OSAKit`.
* 3D: `OpenGL` (CGL, con Mesa debajo), `Metal`.
* Medios: `CoreAudio`, `AudioToolbox`, `AudioUnit`, `CoreMIDI`, `CoreMedia`,
  `CoreVideo`, `VideoToolbox`, `AVFoundation`, `AVKit`, `MediaPlayer`.
* Otros: `WebKit` (la API de Cocoa sobre WPE WebKit), `UserNotifications`,
  `AddressBook`, `AuthenticationServices`, `LocalAuthentication`,
  `CoreLocation`, `IOBluetooth`, `Vision`, `Combine` (OpenCombine),
  `ExceptionHandling`, `XCTest`.

`PrivateFrameworks` contiene `SkyLight` (la biblioteca cliente del servidor
de ventanas, y el propio servidor en `SkyLight/Server`), `CoreUtils` y
`ktrace`.

## CoreServices

Los servicios del sistema y su escritorio, Atlas ([Atlas.es.md](Atlas.es.md)):

* Servicios: `WindowServer` (con SkyLight), `launchservicesd`, `pboard` (el
  portapapeles), `iosurfaced`, `notificationd` (el centro de
  notificaciones), `distnoted` (notificaciones distribuidas), `audiod`,
  `mediadecoder`, `fseventsd`, `logd`, `directoryd`, `dirhelper`,
  `fontcache`.
* El escritorio y sus apps: `loginwindow`, `Dock`, `SystemUIServer` (los
  extras de la barra de menús, las notificaciones), `Files` (el gestor de
  archivos), `SystemPreferences`, `Terminal`, `TextEditor`,
  `SystemMonitor`, `Logs`.

## BSD

Las órdenes Unix y sus bibliotecas ([Userland.es.md](Userland.es.md),
[Commands.es.md](Commands.es.md)): `bin`, `sbin`, `usr.bin`, `usr.sbin`,
`libexec`, `lib` (bibliotecas, entre ellas las externas que usan los medios
y WebKit: FFmpeg, dav1d, Mesa, WPE WebKit y sus dependencias), `etc`,
`share` (también los archivos `mk` con los que compila bmake), `hier` (la
disposición del sistema de archivos) y `syslog`; `launchd` está en `sbin`.

## Developer

El SDK y el toolchain con el que se compilan las apps, compilados primero
para el anfitrión:

* `Default.xctoolchain` - LLVM 21.1.6 (el fork de Swift), Swift 6.4.2,
  swift-build, `cctools` (ld64, lipo, otool), `codesign`, `mig`, `iig`,
  `ibtool`, `plktool`, `dtrace_ctf`, `gmake`, `rpcgen`, `unifdef`, `xar`;
  [tools/toolchain/MANIFEST](../tools/toolchain/MANIFEST) registra de dónde
  viene cada uno.
* `Minkar.platform`, `Minkar.sdk` e `InternalSDK` - el SDK.
* `Templates` - plantillas de proyecto (la plantilla de app de Atlas).

## SystemLibrary

Lo que va a `/System/Library` y `/Library` y no se compila en otro sitio:
`LaunchDaemons`, `Fonts` (fijadas por sus manifiestos), `Sounds`, `Icons`,
`Colors`, `Desktop_Pictures`, la plantilla de usuario y
`SystemVersion.plist`, que rellena el `Makefile` principal.

## Imagen, pruebas y herramientas

* `Image` - la imagen de disco (`mkdiskimage.py`), los parches del cargador
  EFI y la prueba de arranque (`boottest.py`).
* `Tests` - gates compilados como parte del árbol: `M3` (paquetes Swift de
  línea de órdenes), `W0`, `W1`, `W2` (WebKit) y `Gallery`.
* `tools` - herramientas de compilación y comprobación: `toolchain` (el
  manifiesto del toolchain), `check` (informes de la sysroot y de las
  órdenes), `ci`, `symbols` (las importaciones de las apps de referencia,
  la cobertura medida frente a ellas), `progress` (las páginas de
  progreso), `drm-import` (tablas de registros de amdgpu),
  `mktrustcache.py`, simbolización e informes de fallos y de pánicos.
