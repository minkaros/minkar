# Minkar — Lo que tiene API 19 y le falta a Minkar

[English](MissingFrameworks.md)

Los frameworks públicos y servicios del sistema de API 19 que Minkar todavía
no ofrece, para elegir qué implementar después de los bloques ya planificados
(WebKit W5, audio AU7–AU10, la pista de GPU). Estado al 16 nov 2026.

Cómo leerlo:

* **Estado:** *Plan* — ya está en [ROADMAP.es.md](ROADMAP.es.md);
  *Parcial* — el framework existe con parte de su API (las notas dicen cuál);
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

AddressBook, AppKit, ApplicationServices (con PrintCore), AudioToolbox,
AudioUnit (la unidad de salida), AVFoundation, AVKit, Carbon, CFNetwork,
Cocoa, Combine, CoreAudio, CoreData, CoreFoundation, CoreGraphics, CoreMedia,
CoreServices, CoreText, CoreVideo, DiskArbitration, ExceptionHandling,
Foundation, ImageIO, IOKit, IOSurface, MediaPlayer (lo que suena y los
mandos remotos), OpenDirectory, OpenGL (Mesa, en vistas y capas), OSAKit,
PreferencePanes, Quartz, QuartzCore, QuickLook, Security (un subconjunto),
ServiceManagement, System, SystemConfiguration, UniformTypeIdentifiers,
UserNotifications, VideoToolbox (decodificación: H.264, HEVC, VP9, AV1; sin
codificador), WebKit (sobre el motor de WPE WebKit), XCTest y JavaScriptCore
(privado, en `/usr/lib/webkit`).

Presentes con lo que les dan el hardware o un bloque posterior: CoreMIDI
(clientes y puertos, sin extremos hasta AU8), Metal (sin dispositivo hasta la
pista de GPU), Vision (sin reconocimiento de texto), CoreLocation (servicios
de ubicación apagados), LocalAuthentication (aún nada con qué autenticar),
AuthenticationServices (sin proveedores de inicio de sesión), IOBluetooth
(sin driver de Bluetooth).

## Por capas

El sistema en cuatro capas, del kernel hacia arriba, y en qué punto está cada
una.

| Capa | Lo que Minkar tiene | Lo que falta |
| --- | --- | --- |
| **Kernel y núcleo del sistema** | XNU (Darwin 19.6.0: Mach y BSD), I/O Kit, HFS+, FAT y exFAT, la pila de red BSD con Ethernet (I219, I211, Broadcom), sonido HDA y audio por HDMI, la política de sandbox, pantallas AMD (R1, R2 en la RX 6600) | Audio USB (AU7), drivers de Wi-Fi y Bluetooth, los motores de la GPU (G1–G5, en curso), firma de código obligatoria (decidida: trust cache estática y kext MAC propio), APFS (después de 19.6.0) |
| **Servicios básicos** | CoreFoundation, CoreGraphics, CoreText, ImageIO, QuartzCore, CoreServices (LaunchServices, `AppleEvent`, FSEvents), CFNetwork, un subconjunto de Security, CoreAudio, CoreMedia, AVFoundation, VideoToolbox (decodificación), OpenGL | Metal con dispositivo (pista GPU), Accelerate, CoreImage (la mayoría de filtros y los kernels), Network, CryptoKit, ColorSync, un indexador de búsqueda, las fuentes APSL de Security |
| **Frameworks de aplicación** | Cocoa: AppKit y Foundation, partes de Carbon, WebKit (W2–W4), AVKit, PrintCore e impresión | Las vistas web de las apps de referencia (W5), PDFKit y el resto de la lista de abajo; SwiftUI está fuera del alcance para API 19–21 |
| **Experiencia de usuario** | Atlas, con su propio lenguaje visual: ventana de inicio de sesión, barra de menús, Dock, Files, Preferencias, el centro de notificaciones (N1, N2) | El escritorio completo para el uso diario (MB2–MB3, D2–D3, K2–K3, apps A0–A10), búsqueda, accesibilidad (lector de pantalla) |

## Frameworks que faltan

### Audio y vídeo

| Framework | Estado | Apps | Notas |
| --- | --- | --- | --- |
| AudioUnit (alojar unidades de terceros), CoreAudioKit | Plan (AU9) | | La unidad de salida existe; alojarlas y sus vistas en AU9 |
| CoreMIDI | Plan (AU8) | | El framework existe; extremos y servicio en AU8 |
| VideoToolbox (codificación), MediaToolbox | Decidir | | La decodificación existe (FFmpeg en el proceso de la app) |
| CoreMediaIO | Decidir | | Cámaras y capturadoras |
| OpenAL | Decidir | | Puede ir sobre CoreAudio |
| MediaLibrary | Decidir | | MediaPlayer existe |
| AudioVideoBridging | Decidir | | Audio por red (AVB) |
| SoundAnalysis, Speech | Decidir | | |
| DVDPlayback | Fuera | | En desuso |

### Gráficos, 3D y cálculo

| Framework | Estado | Apps | Notas |
| --- | --- | --- | --- |
| Metal (un dispositivo), MetalKit | Plan (pista GPU) | | darling-metal e Indium; la API de Metal existe sin dispositivo, y MTKView de MetalKit (desde el 19 nov 2026, la necesita Skim) dibuja para su delegado sin drawable, como en la referencia sin dispositivo |
| Accelerate | Plan (AU9) | | vDSP, BLAS y LAPACK (OpenBLAS, BSD-3); muy usado, conviene adelantarlo |
| CoreImage | Parcial | | Lo necesita Skim. Framework propio desde el 19 nov 2026 (QuartzCore lo reexporta): CIFilter con su registro, atributos y unidades de imagen (CIPlugIn), las constantes públicas y un contexto que dibuja en luz lineal sobre CoreGraphics; filtros integrados solo los que dibuja (CIBoxBlur, CILinearGradient). Faltan los kernels (CIKernel) y el resto de filtros |
| MetalPerformanceShaders | Decidir | | |
| SceneKit, SpriteKit, ModelIO, GameplayKit, RealityKit | Decidir | | |
| ColorSync | Decidir | | Little CMS ya está previsto |
| OpenCL, GLKit, GLUT, AGL | Decidir | | En desuso, pero algunas apps los usan |
| CoreML, NaturalLanguage; el reconocimiento de Vision | Decidir | | Vision existe sin reconocimiento |

### Interfaz e integración de apps

| Framework | Estado | Apps | Notas |
| --- | --- | --- | --- |
| SwiftUI | Fuera | | Decidido para API 19–21; se sigue OpenSwiftUI |
| PDFKit | Decidir | | Lo necesita el visor de documentos de Atlas |
| QuickLookThumbnailing | Decidir | | QuickLook existe |
| ScreenSaver, InputMethodKit, LinkPresentation, PencilKit | Decidir | | |
| FinderSync, NotificationCenter, SafariServices | Decidir | | Integración con Files y Atlas |
| ScriptingBridge, AppleScriptKit, AppleScriptObjC, Automator | Decidir | | Necesitan el componente de scripts (Fase 6) |
| ImageCaptureCore, ICADevices, TWAIN | Decidir | | Escáneres y cámaras |
| SecurityInterface, SecurityFoundation | Decidir | | |
| Collaboration, InstallerPlugins | Decidir | | ExceptionHandling existe |

### Datos personales y servicios en red

| Framework | Estado | Apps | Notas |
| --- | --- | --- | --- |
| Contacts, ContactsUI, EventKit | Decidir | | AddressBook ya existe |
| CoreSpotlight | Decidir | | Va con un indexador de búsqueda; `MDItem` ya existe |
| Photos, PhotosUI | Decidir | | |
| MapKit; el proveedor de CoreLocation | Decidir | | Necesitan un proveedor de mapas o de ubicación; CoreLocation existe con sus servicios apagados |
| FileProvider, FileProviderUI | Decidir | | |
| MultipeerConnectivity | Decidir | | |
| CloudKit, GameKit, StoreKit, Accounts, Social, PushKit, DeviceCheck, BusinessChat, VideoSubscriberAccount | Fuera | | Dependen de los servicios en la nube del fabricante |
| SyncServices, CalendarStore | Fuera | | En desuso |

### Seguridad, red y hardware

| Framework | Estado | Apps | Notas |
| --- | --- | --- | --- |
| CryptoKit | Decidir | | Muy usado desde Swift |
| CryptoTokenKit, PCSC; los proveedores de LocalAuthentication y AuthenticationServices | Decidir | | Los dos frameworks existen, sin nada con qué autenticar |
| GSS, Kerberos, LDAP | Decidir | | |
| Network | Decidir | | Muy usado por las apps en Swift |
| NetworkExtension, NetFS | Decidir | | |
| CoreWLAN, CoreBluetooth, IOBluetoothUI | Decidir | | Necesitan drivers de Wi-Fi y Bluetooth; IOBluetooth existe sin dispositivos |
| IOUSBHost, ExternalAccessory, GameController, ForceFeedback, CoreHaptics | Decidir | | |
| SystemExtensions, DriverKit, EndpointSecurity, ExecutionPolicy | Decidir | | |
| Hypervisor | Decidir | | |
| DirectoryService, JavaVM; Python, Ruby, Tcl y Tk como frameworks | Fuera | | En desuso |

## Servicios del sistema que faltan

| Servicio | Estado | Notas |
| --- | --- | --- |
| Indexado de búsqueda | Decidir | `MDItem` existe, sin indexador |
| Impresión en impresoras (un servidor CUPS) | Decidir | PrintCore, NSPrinter e imprimir a PDF existen (16 nov 2026); libcups está, `cupsd` no, así que no hay impresoras |
| Wi-Fi y Bluetooth (drivers y servicios) | Decidir | Todavía nada |
| Cámara y captura de vídeo | Decidir | |
| Servicio de llavero con cifrado | Plan | El subconjunto actual se sustituye por las fuentes APSL de Security |
| Copias de seguridad (`tmutil`) | Plan (Fase 6) | |
| Firma de código obligatoria | Plan | Trust cache estática y kext MAC propio |
| Accesibilidad (lector de pantalla, servidor de accesibilidad) | Decidir | |
| Asistente de voz, dictado, almacenamiento en la nube, tienda de apps, continuidad entre dispositivos | Fuera | Servicios del fabricante |

## Orden propuesto

Hechos ya el motor de WebKit (W2–W4) y el audio y los medios (AU1–AU6),
según cuánto software necesita cada uno y cuánto hardware especial pide:

1. Accelerate y CoreImage.
2. Network y CryptoKit.
3. PDFKit y QuickLook (apps de Atlas).
4. Impresoras (`cupsd`; PrintCore e imprimir a PDF están hechos).
5. Wi-Fi y Bluetooth (mucho trabajo de drivers; necesarios en portátiles).

La medición (columna *Apps*) y más apps de referencia pueden cambiarlo.
