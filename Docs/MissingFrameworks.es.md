# Minkar — Lo que tiene API 19 y le falta a Minkar

[English](MissingFrameworks.md)

Los frameworks públicos y servicios del sistema de API 19 que Minkar todavía
no ofrece, para elegir qué implementar después de los bloques ya planificados
(WebKit W2–W5, audio y medios AU1–AU10). Estado al 10 nov 2026.

Cómo leerlo:

* **Estado:** *Plan* — ya está en [ROADMAP.es.md](ROADMAP.es.md);
  *Decidir* — sin plan todavía; *Fuera* — fuera del alcance decidido
  (servicios en la nube del fabricante de la plataforma de referencia,
  SwiftUI) o en desuso.
* **Apps:** cuántas de las apps de referencia importan el framework
  (`tools/symbols/symbol-inventory.py`), y cuántos símbolos. Un framework que
  ninguna app de referencia importa puede importar igual: las apps medidas son
  pocas.

La lista de frameworks de API 19 sale de la documentación pública, no del SDK
del fabricante (sala limpia). Puede faltar o sobrar algún nombre; la medición
es lo que decide.

## Lo que Minkar ya tiene

AddressBook, AppKit, ApplicationServices, AudioToolbox (todavía sin salida de
sonido), Carbon, CFNetwork, Cocoa, Combine, CoreData, CoreFoundation,
CoreGraphics, CoreServices, CoreText, CoreVideo, DiskArbitration, Foundation,
ImageIO, IOKit, IOSurface, OpenDirectory, OpenGL, OSAKit, PreferencePanes,
Quartz, QuartzCore, Security (un subconjunto), ServiceManagement, System,
SystemConfiguration, UniformTypeIdentifiers, UserNotifications, WebKit (la
API; su motor es W2), XCTest y JavaScriptCore (privado, en
`/usr/lib/webkit`).

## Por capas

El sistema en cuatro capas, del kernel hacia arriba, y en qué punto está cada
una.

| Capa | Lo que Minkar tiene | Lo que falta |
| --- | --- | --- |
| **Kernel y núcleo del sistema** | XNU (Darwin 19.6.0: Mach y BSD), I/O Kit, HFS+, FAT y exFAT, la pila de red BSD con Ethernet (I219, I211) | Drivers de audio (AU1, AU7), drivers de Wi-Fi y Bluetooth, los drivers de GPU (G1–G5, en curso), firma de código obligatoria (decidida: trust cache estática y kext MAC propio), APFS (después de 19.6.0) |
| **Servicios básicos** | CoreFoundation, CoreGraphics, CoreText, ImageIO, QuartzCore, CoreServices (LaunchServices, `AppleEvent`, FSEvents), CFNetwork, un subconjunto de Security | CoreAudio y CoreMedia/AVFoundation (AU1–AU6), Metal (pista GPU), Accelerate, CoreImage, Network, CryptoKit, ColorSync, un indexador de búsqueda, las fuentes APSL de Security |
| **Frameworks de aplicación** | Cocoa: AppKit y Foundation (símbolos importados al 99,9 % frente a las apps de referencia), partes de Carbon, la API de WebKit | El motor de WebKit (W2–W5), PDFKit, QuickLook y el resto de la lista de abajo; SwiftUI está fuera del alcance para API 19–21 |
| **Experiencia de usuario** | Atlas, con su propio lenguaje visual: ventana de inicio de sesión, barra de menús, Dock, Files, Preferencias | El escritorio completo para el uso diario (MB2–MB3, D2–D3, K2–K3, apps A0–A10), búsqueda, avisos visibles de notificaciones, accesibilidad (lector de pantalla) |

## Frameworks que faltan

### Audio y vídeo

| Framework | Estado | Apps | Notas |
| --- | --- | --- | --- |
| CoreAudio | Plan (AU2) | | Servicio de audio y la API `AudioObject` |
| AudioUnit | Plan (AU3, AU9) | | Unidad de salida; alojar unidades de terceros en AU9 |
| CoreMIDI | Plan (AU8) | | Framework y servicio |
| CoreAudioKit | Plan (AU9) | | Vistas de unidades de audio |
| AVFoundation, AVKit, CoreMedia | Plan (AU5) | | Sobre FFmpeg y dav1d |
| VideoToolbox, MediaToolbox | Decidir | | Encajan en AU5 |
| CoreMediaIO | Decidir | | Cámaras y capturadoras |
| OpenAL | Decidir | | Puede ir sobre CoreAudio |
| MediaPlayer, MediaLibrary | Decidir | | |
| AudioVideoBridging | Decidir | | Audio por red (AVB) |
| SoundAnalysis, Speech | Decidir | | |
| DVDPlayback | Fuera | | En desuso |

### Gráficos, 3D y cálculo

| Framework | Estado | Apps | Notas |
| --- | --- | --- | --- |
| Metal, MetalKit | Plan (pista GPU) | | darling-metal e Indium |
| Accelerate | Plan (AU9) | | vDSP, BLAS y LAPACK (OpenBLAS, BSD-3); muy usado, conviene adelantarlo |
| CoreImage | Decidir | | Muy usado por las apps de imagen |
| MetalPerformanceShaders | Decidir | | |
| SceneKit, SpriteKit, ModelIO, GameplayKit, RealityKit | Decidir | | |
| ColorSync | Decidir | | Little CMS ya está previsto |
| OpenCL, GLKit, GLUT, AGL | Decidir | | En desuso, pero algunas apps los usan |
| CoreML, Vision, NaturalLanguage | Decidir | | |

### Interfaz e integración de apps

| Framework | Estado | Apps | Notas |
| --- | --- | --- | --- |
| SwiftUI | Fuera | | Decidido para API 19–21; se sigue OpenSwiftUI |
| PDFKit | Decidir | | Lo necesita el visor de documentos de Atlas |
| QuickLook, QuickLookThumbnailing | Decidir | | Quartz ya tiene una parte |
| ScreenSaver, InputMethodKit, LinkPresentation, PencilKit | Decidir | | |
| FinderSync, NotificationCenter, SafariServices | Decidir | | Integración con Files y Atlas |
| ScriptingBridge, AppleScriptKit, AppleScriptObjC, Automator | Decidir | | Necesitan el componente de scripts (Fase 6) |
| ImageCaptureCore, ICADevices, TWAIN | Decidir | | Escáneres y cámaras |
| SecurityInterface, SecurityFoundation | Decidir | | |
| Collaboration, ExceptionHandling, InstallerPlugins | Decidir | | |

### Datos personales y servicios en red

| Framework | Estado | Apps | Notas |
| --- | --- | --- | --- |
| Contacts, ContactsUI, EventKit | Decidir | | AddressBook ya existe |
| CoreSpotlight | Decidir | | Va con un indexador de búsqueda; `MDItem` ya existe |
| Photos, PhotosUI | Decidir | | |
| MapKit, CoreLocation | Decidir | | Necesitan un proveedor de mapas o de ubicación |
| FileProvider, FileProviderUI | Decidir | | |
| MultipeerConnectivity | Decidir | | |
| CloudKit, GameKit, StoreKit, Accounts, Social, PushKit, DeviceCheck, BusinessChat, VideoSubscriberAccount | Fuera | | Dependen de los servicios en la nube del fabricante |
| SyncServices, CalendarStore | Fuera | | En desuso |

### Seguridad, red y hardware

| Framework | Estado | Apps | Notas |
| --- | --- | --- | --- |
| CryptoKit | Decidir | | Muy usado desde Swift |
| LocalAuthentication, AuthenticationServices, CryptoTokenKit, PCSC | Decidir | | |
| GSS, Kerberos, LDAP | Decidir | | |
| Network | Decidir | | Muy usado por las apps en Swift |
| NetworkExtension, NetFS | Decidir | | |
| CoreWLAN, CoreBluetooth, IOBluetooth, IOBluetoothUI | Decidir | | Necesitan drivers de Wi-Fi y Bluetooth |
| IOUSBHost, ExternalAccessory, GameController, ForceFeedback, CoreHaptics | Decidir | | |
| SystemExtensions, DriverKit, EndpointSecurity, ExecutionPolicy | Decidir | | |
| Hypervisor | Decidir | | |
| DirectoryService, JavaVM; Python, Ruby, Tcl y Tk como frameworks | Fuera | | En desuso |

## Servicios del sistema que faltan

| Servicio | Estado | Notas |
| --- | --- | --- |
| Servicio de audio | Plan (AU2) | |
| Indexado de búsqueda | Decidir | `MDItem` existe, sin indexador |
| Impresión (CUPS y PrintCore) | Decidir | Todavía nada |
| Wi-Fi y Bluetooth (drivers y servicios) | Decidir | Todavía nada |
| Cámara y captura de vídeo | Decidir | |
| Servicio de llavero con cifrado | Plan | El subconjunto actual se sustituye por las fuentes APSL de Security |
| Avisos visibles de notificaciones | Decidir | La API existe; no se muestra nada |
| Copias de seguridad (`tmutil`) | Plan (Fase 6) | |
| Firma de código obligatoria | Plan | Trust cache estática y kext MAC propio |
| Accesibilidad (lector de pantalla, servidor de accesibilidad) | Decidir | |
| Asistente de voz, dictado, almacenamiento en la nube, tienda de apps, continuidad entre dispositivos | Fuera | Servicios del fabricante |

## Orden propuesto

Tras W2 y AU1–AU6, según cuánto software necesita cada uno y cuánto hardware
especial pide:

1. Accelerate y CoreImage.
2. Network y CryptoKit.
3. PDFKit y QuickLook (apps de Atlas).
4. Impresión (CUPS).
5. Wi-Fi y Bluetooth (mucho trabajo de drivers; necesarios en portátiles).

La medición (columna *Apps*) y más apps de referencia pueden cambiarlo.
