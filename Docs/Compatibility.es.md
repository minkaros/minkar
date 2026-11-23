# Compatibilidad

Hasta dónde llega en Minkar el binario publicado de cada app designada, y qué frameworks
piden esas apps que Minkar todavía no tiene. Para quien elige en qué trabajar. Última
revisión: 21 nov 2026. Es una copia de [Compatibility.md](Compatibility.md) y se actualiza
en el mismo commit.

El trabajo en frameworks se ordena por esta página y por el registro de stubs ruidosos,
sea cual sea la API a la que pertenece un símbolo. Las partes generadas (entre los
marcadores `compat:`) salen de `bmake progress`; no se editan a mano.

## La matriz

El gate BC (`bmake bc-check`; carril BC en [ROADMAP.es.md](ROADMAP.es.md)) instala en la
imagen el binario publicado de cada app designada como lo haría un usuario, lo arranca con
el usuario de la sesión y anota hasta dónde llegó en
[progress/compatibility.tsv](progress/compatibility.tsv) (`BC_UPDATE=yes` lo escribe).

<!-- compat:matrix:begin -->
_Comprobado: 2026-11-21; ProductVersion 10.15; the gate took 17 minutes under KVM, with the apps disk._

7 de 17 apps muestran una ventana.

### El conjunto 19 (BC4)

| App | Versión | Mínima | Arquitecturas | Resultado | Menús | Motivo |
| --- | --- | --- | --- | --- | --- | --- |
| Firefox | 160.0 | 10.15 | arm64, x86_64 | `window` | sí | as minkar; a window at 0,22, 1280x702 |
| HexFiend | 2.18.1 | 10.14.6 | arm64, x86_64 | `window` | sí | as minkar; a window at 167,275, 367x442 |
| KeepingYouAwake | 1.6.8 | 10.13 | arm64, x86_64 | `no-window` | no | as minkar; running after 120s without a document window (only a panel, 416x169, at level 8); 2026-11-21 17:00:27.580 KeepingYouAwake[371:10da6f7c0] unexpected character 76 'v' at 7 |
| Platypus | 5.4.1 | 10.11 | arm64, x86_64 | `window` | sí | as minkar; a window at 308,127, 664x546 |
| Sloth | 3.4 | 10.13 | arm64, x86_64 | `window` | sí | as minkar; a window at 322,91, 892x633 |
| TextMate | 2.0.23 | 10.12 | arm64, x86_64 | `window` | sí | as minkar; a window at 426,162, 427x422 |
| Thunderbird | 160.0 | 10.15 | arm64, x86_64 | `window` | sí | as minkar; a window at 0,22, 1280x702 |

### Las apps designadas siguientes (BC5 en adelante)

| App | Versión | Mínima | Arquitecturas | Resultado | Menús | Motivo |
| --- | --- | --- | --- | --- | --- | --- |
| Cyberduck | 9.6.0 | 10.13 | arm64, x86_64 | `at-start` | no | as minkar; exit status 1, after its run loop started; Couldn't spawn command: su |
| IINA | 1.5.1 | 11 | arm64, x86_64 | `min-version` | no | requires 11, Minkar reports 10.15; run anyway: no-load: as minkar; dyld: Library not loaded: /System/Library/PrivateFrameworks/PIP.framework/Versions/A/PIP Referenced from: /Applications/IINA.app/Contents/MacOS/IINA Reason: image not found  |
| iTerm2 | 3.7.8 | 13.0 | arm64, x86_64 | `min-version` | no | requires 13.0, Minkar reports 10.15; run anyway: signature: as minkar; MinkarCodeIntegrity: refused /Applications/iTerm.app/Contents/MacOS/iTerm2: restricted entitlement keychain-access-groups outside the trust cache |
| KeePassXC | 2.8.0 | 12.0 | x86_64 | `min-version` | no | requires 12.0, Minkar reports 10.15; run anyway: signature: as minkar; MinkarCodeIntegrity: refused /Applications/KeePassXC.app/Contents/MacOS/KeePassXC: restricted entitlement keychain-access-groups outside the trust cache |
| LibreOffice | 26.8.3 | 11.0.0 | x86_64 | `min-version` | no | requires 11.0.0, Minkar reports 10.15; run anyway: no-load: as minkar; dyld: Library not loaded: /System/Library/Frameworks/Kerberos.framework/Versions/A/Kerberos Referenced from: /Applications/LibreOffice.app/Contents/Frameworks/libmergedlo.dylib Reason: image not found  |
| Skim | 1.7.19 | 10.13 | arm64, x86_64 | `window` | sí | as minkar; a window at 334,104, 480x382 |
| Transmission | 4.1.4 | 12.0 | arm64, x86_64 | `min-version` | no | requires 12.0, Minkar reports 10.15; run anyway: no-load: as minkar; dyld: Library not loaded: /System/Library/Frameworks/QuickLookUI.framework/Versions/A/QuickLookUI Referenced from: /Applications/Transmission.app/Contents/MacOS/Transmission Reason: image not found  |
| UTM | 5.0.9 | 12.0 | arm64, x86_64 | `min-version` | no | requires 12.0, Minkar reports 10.15; run anyway: signature: as minkar; MinkarCodeIntegrity: refused /Applications/UTM.app/Contents/MacOS/UTM: restricted entitlement com.apple.vm.device-access outside the trust cache |
| VLC | 3.0.24 | 10.7.5 | arm64, x86_64 | `at-start` | no | as minkar; exit status 0, after its run loop started; [00007f903d4a3c40] [cli] lua interface error: Error loading script /Applications/VLC.app/Contents/MacOS/share/lua/intf/cli.luac: ...kage/macosx/../../../share/lua/intf/modules/host.lua:286: Interrupted. |
| VSCodium | 1.143.17232 | 12.0 | x86_64 | `min-version` | no | requires 12.0, Minkar reports 10.15; run anyway: install: hdiutil attach: hdiutil: attach failed - no mountable file systems |

Resultados, del que menos llega al que más:

- `install`: no se pudo instalar el archivo descargado
- `signature`: el kernel rechazó su firma
- `no-load`: dyld no pudo cargarla (falta una biblioteca o un símbolo)
- `before-main`: terminó antes de que corrieran los inicializadores de sus bibliotecas
- `at-start`: terminó después, antes de mostrar una ventana
- `no-window`: corre, pero no mostró ninguna ventana
- `min-version`: su versión mínima es mayor que la que reporta Minkar y no mostró ventana (el motivo dice hasta dónde llegó)
- `not-run`: el gate no llegó a ella
- `window`: muestra una ventana
<!-- compat:matrix:end -->

## Frameworks que piden las apps designadas

Cada framework que importa una app designada y que Minkar no tiene, y los que tiene solo
en parte, desde [progress/frameworks.tsv](progress/frameworks.tsv): cuántas de las 17 apps lo
importan y cuántos de los símbolos distintos que importan exporta Minkar (un stub ruidoso
cuenta como exportado). Para refrescar los datos, se pasa
`tools/symbols/symbol-inventory.py` por las apps y después
`tools/compat/matrix.py --frameworks <su informe> Docs/progress/frameworks.tsv`.

Estado: **Plan**, en el roadmap; **Partial**, el framework existe con parte de su API;
**Decide**, sin plan todavía; **Out**, fuera del alcance decidido (los servicios en la
nube del fabricante) u obsoleto.

<!-- compat:frameworks:begin -->
| Framework | Apps | Símbolos en Minkar | Estado |
| --- | ---: | ---: | --- |
| Metal | 9 | 17 de 60 | Plan (pista GPU) |
| VideoToolbox | 6 | 38 de 78 | Partial |
| CoreImage | 5 | 41 de 49 | Partial |
| LocalAuthentication | 5 | 1 de 1 | Partial |
| MediaPlayer | 5 | 17 de 25 | Partial |
| Accelerate | 4 | 0 de 47 | Plan (AU9) |
| CoreLocation | 4 | 7 de 7 | Partial |
| CoreMIDI | 4 | 24 de 25 | Plan (AU8) |
| QuickLookThumbnailing | 4 | 0 de 5 | Decide |
| QuickLookUI | 4 | 0 de 5 | Decide |
| ScreenCaptureKit | 4 | 0 de 14 | Decide |
| Accessibility | 3 | 0 de 3 | Decide |
| AuthenticationServices | 3 | 8 de 38 | Partial |
| ColorSync | 3 | 0 de 10 | Plan |
| CryptoKit | 3 | 0 de 45 | Decide |
| IOBluetooth | 3 | 1 de 8 | Partial |
| MetalKit | 3 | 2 de 7 | Plan (pista GPU) |
| Network | 3 | 0 de 14 | Decide |
| SwiftUI | 3 | 0 de 1022 | Plan (carril SU) |
| Vision | 3 | 2 de 21 | Partial |
| AudioUnit | 2 | 14 de 15 | Plan (AU9) |
| Kerberos | 2 | 0 de 27 | Decide |
| OpenDirectory | 2 | 0 de 8 | Partial |
| ScriptingBridge | 2 | 0 de 2 | Decide |
| AccessoryAccess | 1 | 0 de 3 | Decide |
| AppIntents | 1 | 0 de 216 | Decide |
| AVFAudio | 1 | 0 de 5 | Decide |
| Charts | 1 | 0 de 61 | Decide |
| Contacts | 1 | 0 de 10 | Decide |
| CoreBluetooth | 1 | 0 de 10 | Decide |
| CoreDisplay | 1 | 0 de 1 | Decide |
| CoreHaptics | 1 | 0 de 9 | Decide |
| CoreMediaIO | 1 | 0 de 1 | Decide |
| CoreML | 1 | 0 de 9 | Decide |
| CoreWLAN | 1 | 0 de 1 | Decide |
| CryptoTokenKit | 1 | 0 de 1 | Decide |
| DeviceCheck | 1 | 0 de 1 | Out |
| DiskImageKit | 1 | 0 de 33 | Decide |
| ForceFeedback | 1 | 0 de 9 | Decide |
| FoundationModels | 1 | 0 de 70 | Decide |
| GameController | 1 | 0 de 12 | Decide |
| GSS | 1 | 0 de 7 | Decide |
| Hypervisor | 1 | 0 de 19 | Decide |
| JavaRuntimeSupport | 1 | 0 de 23 | Decide |
| MediaAccessibility | 1 | 0 de 1 | Decide |
| MetalFX | 1 | 0 de 4 | Decide |
| ParavirtualizedGraphics | 1 | 0 de 5 | Decide |
| PCSC | 1 | 0 de 13 | Decide |
| PIP (private) | 1 | 0 de 2 | Decide |
| SafariServices | 1 | 0 de 3 | Decide |
| SecurityInterface | 1 | 0 de 3 | Decide |
| StoreKit | 1 | 0 de 3 | Out |
| Symbols | 1 | 0 de 6 | Decide |
| TipKit | 1 | 0 de 28 | Decide |
| Virtualization | 1 | 0 de 63 | Decide |
| VisionKit | 1 | 0 de 40 | Decide |
| vmnet | 1 | 0 de 20 | Decide |
<!-- compat:frameworks:end -->

Estado y notas, escritos a mano: la tabla toma el estado de aquí, y solo lista un
framework que Minkar tiene si tiene línea aquí. Un framework que falta y no tiene línea es
Decide. Los símbolos que faltan en los frameworks que Minkar tiene están en el registro de
stubs ruidosos.

- **Accelerate**: Plan (AU9) — vDSP, BLAS y LAPACK sobre OpenBLAS (BSD-3).
- **AudioUnit**: Plan (AU9) — la unidad de salida existe; alojar unidades de terceros y CoreAudioKit en AU9.
- **AuthenticationServices**: Partial — sin proveedores de inicio de sesión.
- **ColorSync**: Plan — sobre Little CMS.
- **CoreImage**: Partial — el registro de CIFilter, las unidades de imagen y un contexto sobre CoreGraphics; faltan los kernels y la mayoría de los filtros.
- **CoreLocation**: Partial — los servicios de ubicación, apagados.
- **CoreMIDI**: Plan (AU8) — clientes y puertos; los endpoints y el servicio MIDI en AU8.
- **DeviceCheck**: Out — servicio en la nube del fabricante.
- **IOBluetooth**: Partial — sin driver de Bluetooth.
- **LocalAuthentication**: Partial — todavía no hay con qué autenticar.
- **MediaPlayer**: Partial — lo que se reproduce y los mandos remotos.
- **Metal**: Plan (pista GPU) — la API existe sin dispositivo; darling-metal e Indium.
- **MetalKit**: Plan (pista GPU) — `MTKView` dibuja para su delegado sin drawable.
- **OpenDirectory**: Partial — la interfaz en C existe; las clases de Objective-C que importan las apps no.
- **ScriptingBridge**: Decide — necesita el componente de scripting (Fase 6).
- **StoreKit**: Out — servicio en la nube del fabricante.
- **SwiftUI**: Plan (carril SU) — capa pública propia de Minkar sobre un motor basado en SwiftCrossUI; ver [SwiftUI.md](SwiftUI.md).
- **VideoToolbox**: Partial — decodificación (H.264, HEVC, VP9, AV1) con FFmpeg; sin codificador.
- **Vision**: Partial — sin reconocimiento de texto.

**Todavía no los importa ninguna app designada** (salen de esta lista cuando una lo haga):
Accounts, AGL, AppleScriptKit, AppleScriptObjC, AudioVideoBridging, Automator,
BusinessChat, CalendarStore, CloudKit, Collaboration, ContactsUI, CoreAudioKit,
CoreSpotlight, DirectoryService, DriverKit, DVDPlayback, EndpointSecurity, EventKit,
ExecutionPolicy, ExternalAccessory, FileProvider, FileProviderUI, FinderSync, GameKit,
GameplayKit, GLKit, GLUT, ICADevices, ImageCaptureCore, InputMethodKit, InstallerPlugins,
IOBluetoothUI, IOUSBHost, JavaVM, LDAP, LinkPresentation, MapKit, MediaLibrary,
MediaToolbox, MetalPerformanceShaders, ModelIO, MultipeerConnectivity, NaturalLanguage,
NetFS, NetworkExtension, NotificationCenter, OpenAL, OpenCL, PencilKit, Photos, PhotosUI,
PushKit, Python, RealityKit, Ruby, SceneKit, ScreenSaver, SecurityFoundation, Social,
SoundAnalysis, Speech, SpriteKit, SyncServices, SystemExtensions, Tk, TWAIN,
VideoSubscriberAccount.

## Lo que Minkar tiene

AddressBook, AppKit, ApplicationServices (con PrintCore), AudioToolbox, AudioUnit (la
unidad de salida), AVFoundation, AVKit, Carbon, CFNetwork, Cocoa, Combine, CoreAudio,
CoreData, CoreFoundation, CoreGraphics, CoreImage (en parte), CoreMedia, CoreServices,
CoreText, CoreVideo, DiskArbitration, ExceptionHandling, Foundation, ImageIO, IOKit,
IOSurface, MediaPlayer, OpenDirectory (en parte), OpenGL (Mesa, en vistas y capas),
OSAKit, PDFKit, PreferencePanes, Quartz, QuartzCore, QuickLook, SecurityInterface (el candado), Security (un
subconjunto), ServiceManagement, System, SystemConfiguration, UniformTypeIdentifiers,
UserNotifications, VideoToolbox (decodificación), WebKit (sobre el motor de WPE WebKit),
XCTest y JavaScriptCore (privado). Presentes, a la espera del hardware o de un bloque
posterior: CoreMIDI, Metal, MetalKit, Vision, CoreLocation, LocalAuthentication,
AuthenticationServices, IOBluetooth.

## Servicios del sistema que faltan

| Servicio | Estado | Notas |
| --- | --- | --- |
| Indexado para búsquedas | Plan (19.6.x, K4) | `MDItem` y `mdls`, `mdfind` (busca en el disco), `mdutil`; sin indexador todavía (decidido el 23 nov 2026) |
| Imprimir en impresoras | Hecho | `cupsd` y sus comandos, el panel Impresoras, imprimir por PrintCore en impresoras IPP Everywhere que aceptan PDF |
| Wi-Fi y Bluetooth | Decide | Todavía sin drivers |
| Cámara y captura de vídeo | Decide | |
| Autorización (authd, derechos, contraseña de administrador) | Hecho | authd y cliente APSL, el Agente de autorización de Atlas, `AuthorizationExecuteWithPrivileges`, el candado de los paneles (SecurityInterface) |
| Llavero con cifrado | Plan | El subconjunto actual de Security se sustituye por las fuentes APSL de Security |
| Copias de seguridad (`tmutil`) | Plan (19.6.x) | Se mueve con las copias de seguridad (decidido el 23 nov 2026) |
| Accesibilidad (lector de pantalla, servidor de accesibilidad) | Decide | |
| Asistente de voz, dictado, almacenamiento en la nube, tienda de apps, continuidad entre dispositivos | Out | Servicios del fabricante |
