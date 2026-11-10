# Minkar — Plan de acción arquitectónico

31 oct 2026

## Resumen ejecutivo

La meta de Minkar es un sistema sobre Darwin 19.6.0 que compile y ejecute apps Cocoa. La primera versión, **Minkar 19.6.0**, entrega la API 19 con el escritorio Atlas y su propio lenguaje visual; Minkar 20.0 y 21.0 completan las API 20 y 21 sin cambiar el kernel. La ruta crítica es estrictamente ascendente: arranque Darwin → launchd → userland → frameworks → WindowServer → Atlas → apps. Objective-C y Swift completo, DMG, compatibilidad binaria y arm64 se paralelizan o se difieren para no bloquear esa ruta.

El plan tiene 8 fases (0–7), cada una con entregables y un criterio de salida verificable en CI. Ninguna fase superior se da por cerrada si la inferior no pasa su gate. Desde el 29 oct 2026 rige además un requisito de rendimiento: igual o mejor que el sistema de referencia en el mismo equipo físico, exigido desde Minkar 21.0 (ver Requisitos de rendimiento).

**Fuente de verdad del estado:** [`Docs/ROADMAP.md`](https://github.com/minkaros/minkar/blob/devel/Docs/ROADMAP.md) en el repositorio (en inglés). Este documento lo refleja en español, y [Progress.es.md](Progress.es.md) resume su estado: una tabla, un mapa de metro de los hitos por plan y un tablero de trabajo, generados desde `progress/status.toml`; verificado contra la rama `devel` en `82c5f3e8f8` (31 oct 2026), 140 commits por delante de `main`.

### Principios arquitectónicos

1. **Gates verificables, de abajo arriba.** Cada fase termina con una prueba automatizada que arranca la ISO en QEMU. Sin prueba verde no hay fase cerrada.
2. **El contrato es la API pública Cocoa, por números de API.** Primero la API 19, luego la 20 y la 21. Cada símbolo se clasifica como implementado, stub o ausente, y se mide.
3. **Stubs ruidosos.** Todo stub registra su primera llamada en el log del sistema. Así las apps reales dicen qué falta, en orden de uso.
4. **Reutilizar antes que escribir.** Primero código abierto APSL (xnu, dyld, launchd, hfs), luego swift-corelibs, luego userland FreeBSD. Código nuevo solo para los huecos.
5. **Licencias limpias.** El código escrito desde cero para Minkar va bajo BSD-2-Clause o MIT; las bibliotecas y frameworks de terceros que modificamos conservan su licencia original. Regla de sala limpia: nadie que haya visto código cerrado de la plataforma de referencia aporta código. Las marcas de terceros no se usan en nombres, iconos ni textos públicos; un único TRADEMARKS.md lo declara. Una excepción, aceptada por el mantenedor porque no existe un juego de iconos del mismo alcance con licencia BSD o MIT: los iconos de Atlas son los temas Faience y Faenza (GPL-3), ficheros de datos separados en `SystemLibrary/Icons` que conservan su licencia GPL-3; ningún código se enlaza con ellos y el resto de esta regla sigue igual.
6. **Apps de referencia como pruebas de aceptación.** Un conjunto fijo de apps de código abierto escritas para la API Cocoa se compila en cada hito; su lista de símbolos importados prioriza el trabajo en frameworks.

## Decisión de versiones

Minkar separa tres capas que en la plataforma de referencia van juntas. El kernel se queda en Darwin 19.6.0; la API y el aspecto avanzan por su cuenta.

| Capa | Decisión | Nota |
| --- | --- | --- |
| Kernel y base BSD | Darwin 19.6.0 | Se toca solo cuando una API nueva necesite una interfaz del kernel |
| API | API 19 primero; luego API 20 y 21 | Cada API guiada por los imports de las apps de referencia |
| Aspecto | Lenguaje visual Atlas desde la Fase 5 | Diseño propio con su guía; sin iconos, fuentes ni recursos gráficos de terceros |

**Números de API (decidido el 30 oct 2026).** «API N» nombra la API pública Cocoa de Darwin N; no es el kernel, que sigue en Darwin 19.6.0. En código, plists y macros de disponibilidad se usa el número de versión de la tabla; en el texto, «API N».

| API | Versión en código (`API_AVAILABLE`, `ProductVersion`) | Año |
| --- | --- | --- |
| API 19 | 10.15 | 2019 |
| API 20 | 11.0 | 2020 |
| API 21 | 12.0 | 2021 |
| API 22–26 | 13.0 a 27.0 | 2022–2026, fuera del alcance |

**Versiones de Minkar (decidido el 30 oct 2026).** La versión principal de Minkar es la API que entrega; el kernel no cambia con ella.

| Minkar | Entrega | Kernel (`uname -r`) | `ProductVersion` |
| --- | --- | --- | --- |
| 19.6.0 | API 19, escritorio Atlas completo para el uso diario (MB2–MB3, D2–D3, K2–K3, apps A0–A10), WebKit (W0–W5), audio y medios (AU1–AU6) y DMG; gates funcionales M0–M6 en equipo físico | 19.6.0 | 10.15 |
| 19.6.x | Correcciones, y MB4, D4 y K4, y audio profesional (AU7–AU10); pueden traer partes de API 20 | 19.6.0 | 10.15 |
| 20.0 | API 20 completa y probada | 19.6.0 | 11.0 |
| 21.0 | API 21 completa y probada, más los gates de rendimiento | 19.6.0 | 12.0 |

- El salto de versión lo decide un gate, no la fecha. Si algún día se actualiza el kernel, la versión sube a la que corresponda.
- Equivalencias: la antigua «1.0» es Minkar 19.6.0 y la «2.0» pasa a ser el requisito de rendimiento de Minkar 21.0. La numeración 0.x (`PROD_VERSION = 0.7.1`) pasa a `19.6.0`, con `ProductFamily` Pre Alpha → Alpha → Beta.

* **APIs que dependen del kernel.** Algunas APIs nuevas esperan interfaces que XNU 19.x no tiene; por ejemplo, libpthread y libdispatch modernos usan las interfaces de workqueue y kevent de un XNU más reciente. Cada caso se resuelve con un parche acotado al kernel o con una implementación en userland, y la decisión se documenta.
* **Versión reportada (decidido el 25 oct; ampliado el 30 oct 2026).** `ProductVersion` en `SystemVersion.plist` reporta el valor de compatibilidad de la última API cerrada (10.15, 11.0, 12.0), nunca 19.6.0; `ProductUserVisibleVersion` muestra la versión de Minkar (19.6.0); `uname -r` sigue diciendo 19.6.0. `@available`, `sw_vers -productVersion`, `NSProcessInfo` y LaunchServices leen `ProductVersion`, y libSystem (`_availability_version_check`) toma el valor de ahí.
  - Se arranca en 10.15 (API 19) y se sube a 11.0 y 12.0 solo al cerrar la API 20 y la API 21.
  - Gate de cada API: un script verifica que ningún símbolo de su lista sea un stub, usando el inventario de símbolos de la Fase 0 y el log de stubs ruidosos.
  - Compatibilidad: mecanismo tipo `SystemVersionCompat.plist` y `SYSTEM_VERSION_COMPAT=1`. Los binarios enlazados con un SDK anterior reciben la versión antigua.
  - Descartado: subir `kern.osrelease` a 21.x, porque mentiría sobre la ABI del kernel; reportar 10.15 para siempre, porque bloquea las apps que exigen una API mayor en `LSMinimumSystemVersion`; y poner 19.6.0 en `ProductVersion`, porque `@available` daría por presentes APIs que faltan.
* **Software que deduce la versión de la plataforma desde Darwin** (19 → API 19) se registra en la matriz de compatibilidad de la Fase 7. Ejemplo: un clang nativo con triple `darwin19.6.0` asume el deployment target de la API 19 (10.15); se corrige con `MACOSX_DEPLOYMENT_TARGET`.

## Requisitos de rendimiento y hardware de referencia

El rendimiento de Minkar debe ser igual o mejor que el del sistema de referencia en el mismo equipo físico (decidido el 29 oct 2026). Una solución que funciona pero es lenta no cierra un requisito: queda como parcial. El uso final es en equipos físicos; las máquinas virtuales son solo entorno de desarrollo.

**Decidido (30 oct 2026):** Minkar 19.6.0 cierra los gates funcionales M0–M6; Minkar 21.0 exige además los gates de rendimiento de esta tabla. El frente de GPU empieza ya, en paralelo, porque es la pieza más larga y de ella depende el requisito.

| Métrica | Requisito, frente al sistema de referencia en el mismo equipo |
| --- | --- |
| Arranque hasta la ventana de login | ≤ referencia |
| Latencia de entrada a pixel en pantalla | ≤ referencia |
| Composición con varias ventanas animadas | Refresco de la pantalla sin cuadros perdidos; CPU de WindowServer ≤ referencia |
| Arranque de apps, en frío y en caliente | ≤ referencia |
| Memoria en reposo | ≤ referencia |
| Dibujo 2D y texto (Core Graphics, Core Text) | Benchmark de dibujo ≥ referencia |
| 3D (Metal, Vulkan, OpenGL) | ≥ referencia con la misma GPU |

| Entorno | Uso | Gates que aplica |
| --- | --- | --- |
| VM QEMU (TCG y KVM) | Desarrollo y CI | Funcionales; se permiten vías por software (pixman, lavapipe, llvmpipe) como andamio |
| PC 1: Gigabyte H270N-WIFI, i7-7700 | Primer equipo físico; arranca a login desde SATA | Funcionales y de rendimiento frente al sistema de referencia |
| PC 2 | Intel Xeon E5-2697 v2 (12 núcleos, 24 hilos), 40 GB, NVMe Samsung, xHCI Fresco Logic FL1100, Broadcom BCM57762, dos AMD FirePro D500 (Tahiti); hardware del propio fabricante (1 nov 2026): segundo PC de Minkar (gate M1) y hardware original del fabricante para medir la referencia | Ídem |

- **Cómo se mide:** Minkar y el sistema de referencia en el mismo equipo (arranque dual), con los mismos benchmarks ejecutados por SSH. El sistema de referencia solo se mide en hardware original de su fabricante (candidato: PC 2), porque su licencia no permite otro, y sus valores se guardan fuera del repo público.
- **Consecuencia:** si la referencia solo se mide en el PC 2, Minkar también debe medirse ahí, y sus GPU (dos AMD FirePro D500: Tahiti, GCN 1.0, PCI 1002:679e; la familia más antigua que tenemos y con soporte menos maduro) pasan a ser las del primer driver de alto rendimiento. La otra configuración del modelo, la FirePro D300, es Pitcairn: otro chip con su propio firmware, nunca probado (el driver conoce sus tablas y toma su microcódigo, `pitcairn_*.bin`; pendiente hasta tener una D300).
- **Familias de GPU (decidido el 2026-11-01, [ADR 0002](ADR/0002-GPU-First-Family.es.md)):** primero GCN 1.0, la FirePro D500 (Tahiti) del PC 2, donde se mide la referencia; luego RDNA2, la tarjeta RX 6600 del PC 1 (Navi 23, que hoy lleva su pantalla con el framebuffer del firmware; sus pasos R0–R5 en [GPU-RDNA2.es.md](GPU-RDNA2.es.md), R0 hecho); luego Intel Gen9, la HD 630 integrada en el i7-7700 del PC 1.

## Línea base

Minkar arranca sobre Darwin con su propio cargador EFI, todo el sistema se compila como Mach-O y ya corre en un PC físico con sus ocho hilos de CPU y desde su SSD NVMe. La Fase 2 está cerrada; el hueco mayor está entre el kernel y el escritorio. Estado al 31 oct 2026:

| Componente | Estado | Qué falta | Fase |
| --- | --- | --- | --- |
| Cargador EFI + kernel Darwin | Arranca en QEMU (TCG y KVM), en el PC 1 desde SATA o NVMe y en el PC 2 desde NVMe; SMP con todas las CPU (24 hilos en el PC 2); el botón de encendido apaga de forma ordenada | Procesadores AMD, MSI-X | 1 |
| Build del sistema | `world` completo; la CI compila y arranca en `main` y a mano | — (M0 cumplido) | 0 |
| libSystem | Las apps de referencia encuentran todos sus imports de libSystem (308 de 308); `os_log` escribe en logd (`log show`); `@available` compara con `ProductVersion`; dyld con @rpath, @loader\_path y re-exports probado | Stubs ruidosos, `logd`, shared cache de dyld | 3 |
| launchd / launchctl | launchd-842 como PID 1; launchctl con kickstart; `KeepAlive PathState`, `WatchPaths` y `QueueDirectories` | Nada | 2 (cerrada) |
| Userland y herramientas del sistema | Todos los comandos de la Fase 2 (337 de 354 de la lista de referencia) | `open` y los comandos de fases posteriores | 2 (cerrada) |
| DiskArbitration | Framework y `diskarbitrationd` originales (APSL); `diskutil` y `hdiutil` propios; monta HFS+, FAT y exFAT | — | 2 (cerrada) |
| CoreFoundation | CF-Lite (APSL) | APIs que faltan hasta la API 21 | 3 |
| Foundation | Tres lotes de correcciones (26 oct); `NSProcessInfo` con `operatingSystemVersion`; reexporta CoreFoundation; `NSURLSession` sobre libcurl, con cookies; `NSOperationQueue` sobre dispatch; el run loop principal ejecuta la cola principal de dispatch; `NSDateFormatter` sobre ICU (31 oct); `NSXPCConnection` sobre libxpc, con los servicios XPC dentro de las apps (31 oct) | Caché de URL y métricas de tareas en `NSURLSession`; auditoría de la API 19 | 3 |
| Swift 6 | El runtime y la biblioteca estándar de Swift 6.0 en `/usr/lib/swift`, compilados desde `swift-6.0-RELEASE` con el compilador de Swift del propio toolchain; los overlays (Foundation, Dispatch, ObjectiveC...), OpenCombine como Combine y XCTest; gate M3 cumplido (5 nov 2026: siete herramientas de línea de comandos de código abierto pasan sus propios tests) | Swift Concurrency en el actor principal de las apps, SwiftUI (fuera de alcance hasta API 21) | 3 (M3 cumplido) |
| WindowServer | El núcleo nuevo es el servidor de ventanas (WS4b hecho el 2 nov 2026, gate `M4`): su interfaz en `SkyLight.framework`, su servidor arranca con el sistema como `_windowserver` junto al `loginwindow` mínimo; compone ventanas de AppKit con pixman, reparte la entrada de IOHIDSystem con event taps, y cada app dibuja sus propios menús (MB0). El servidor anterior ya no existe. WS5 hecho (2 nov 2026): IOSurface con su registro `iosurfaced`, ventanas con IOSurface, un driver virtio-gpu 2D que vuelca por fotograma y cambia de modo, configuraciones de pantalla permanentes, `CGDisplayStream` | La GPU (WS6): G1 hecho en el PC 2 (driver de pantalla); G2 en marcha (su gate cumplido) | 4 |
| Aceleración por GPU | En la AMD FirePro D500 del PC 2 (Tahiti, GCN 1.0, PCI 1002:679e): la pantalla en su modo nativo, DMA y el procesador de comandos, RADV de Mesa (Vulkan 1.3, un subconjunto del CTS sin fallos), y el servidor de ventanas compone en la GPU (G1 a G4) | Los relojes de la GPU, Pitcairn (FirePro D300) y otras familias, OpenGL, las mediciones frente a la referencia | 4 y Minkar 21.0 |
| loginwindow | parcial: la ventana gráfica de inicio de sesión y el fondo de escritorio, una sesión por usuario | Autenticación con Directory Services, los LaunchAgents del usuario, el LoginWindow completo de la fase 5 | 5 |
| SystemUIServer (barra de menús) | parcial: cada app dibuja su barra de menús (MB0); SystemUIServer se compila y muestra el reloj | Menu Extras y la API completa de status items (MB1), menús estándar (MB2), el menú del sistema completo (MB3), Servicios (MB4, 19.6.x) | 5 |
| Dock | parcial: el núcleo nuevo con las claves de preferencias de la referencia, lanzar apps y las apps abiertas (D0, D1) | Interacción: reordenar, añadir, quitar, menús contextuales (D2); servicio de tiles, ventanas minimizadas, selector de apps (D3); pilas y Papelera (D4, 19.6.x) | 5 |
| Files | parcial: navegar con barra lateral y vistas de iconos, lista y columnas, abrir archivos y apps, Obtener información básico (K1) | El resto de K0 (comprobar las vistas de AppKit), operaciones con archivos (K2), iconos del escritorio y volúmenes (K3); búsqueda, etiquetas y previsualizaciones (K4, 19.6.x) | 5 |
| Text Editor | parcial: documentos de texto plano, abrir y guardar (con el panel de guardar), `open -e` | RTF, buscar y reemplazar, codificaciones, finales de línea, impresión (A2) | 5 y 6 |
| System Preferences | parcial: la app, `PreferencePanes.framework` y cinco paneles (Pantallas, Dock, Fecha y hora, Teclado, Red) | El panel Usuarios (espera a Directory Services), el resto de paneles | 5 |
| A0 Bases de las apps | hecho (8 nov: Acerca de, libros de ayuda, documentos con versiones, la plantilla; Atlas.es.md 5.3) | La plantilla de app: ventanas Acerca de y Preferencias, menú Ayuda, español e inglés, `NSDocument` | 6 |
| Terminal (A1) | hecho (8 nov: libvterm, pestañas, Preferencias; `vttest` aún sin automatizar) | Toda la app: un emulador VT100/xterm sobre un pty, pestañas, historial, Agave | 6 |
| Monitor del sistema (A3) | hecho salvo energía (8 nov; la energía por proceso espera los datos del driver de energía) | Toda la app: procesos, CPU, memoria, energía, disco, red, salir y forzar salida | 6 |
| Registros (A4) | hecho salvo escribir informes de fallos (8 nov; la app los muestra, falta el generador de informes) | Toda la app, sobre `logd` y `os_log`; informes de fallos y de pánicos | 6 |
| Captura de pantalla (A5) | falta | Toda la app, sobre `screencapture` y la lista de ventanas; los atajos del sistema | 6 |
| Visor de imágenes (A6a) | falta | Toda la app, e ImageIO | 6 |
| Visor de documentos (A6b) | falta | Toda la app, y PDFKit | 6 |
| Discos (A7) | falta | Toda la app, sobre DiskArbitration, `diskutil` y `hdiutil` | 6 |
| Información del sistema (A8) | falta | Toda la app, como `system_profiler` | 6 |
| Calculadora (A9) | falta | Toda la app: modos básico, científico y de programador | 6 |
| Gestor de archivadores (A10) | falta | Toda la app, sobre libarchive y `ditto` | 6 |
| Tipos de letra (19.6.x) | falta | Toda la app | 19.6.x |
| Contraseñas y llaves (19.6.x) | falta | Toda la app, y el llavero que muestra | 19.6.x |
| Selector de colores (19.6.x) | falta | Toda la app, y los selectores de `NSColorPanel` | 19.6.x |
| Editor de scripts (19.6.x) | falta | Toda la app, y un entorno de scripts | 19.6.x |
| Ayuda (19.6.x) | falta | Toda la app, y la búsqueda del menú Ayuda | 19.6.x |
| Caracteres (19.6.x) | falta | Toda la app, y la paleta de caracteres | 19.6.x |
| WebKit (W0–W5) | parcial: las bibliotecas de W0 compiladas y sus propios tests pasando en el PC 2 (21 de 23); JavaScriptCore con su JIT, gate W1 en marcha (es6 605 de 605); WebCore y los procesos sobre WPE, gate W2 superado; `WKWebView` (W3) y el `WebView` antiguo (W4) sobre WPE | Los tests propios de FreeType y WOFF2; las otras suites de W1; las vistas web de las apps de referencia (W5) | 3 y Minkar 19.6.0 |
| AppKit | Parcial (Cocotron) | Piezas recientes de la API y lenguaje visual Atlas | 5 |
| DMG, apps base | `hdiutil attach` ya monta por DiskArbitration | `DiskImages.framework` (UDIF), apps | 6 |
| Compatibilidad binaria, arm64 | Futuro; install names ya idénticos a los originales | Resto | 7 |

## Fase 0 — Build, toolchain y CI

Objetivo: cualquier contribuidor obtiene una ISO arrancable desde cero con un solo comando, y cada PR se prueba arrancando en QEMU. Sin esto, todas las demás fases avanzan a ciegas.

**Actividades**

- [x] Fijar el toolchain: `tools/toolchain/MANIFEST` fija la versión upstream, el árbol git y la línea de versión de cada componente (clang 17.0.6 de `swift-6.0-RELEASE`, ld64-609, y desde el 29 oct cctools con `ctf_insert` y las herramientas CTF de dtrace); `manifest.sh check` lo verifica en la CI y comprueba que una entrada compila, enlaza y archiva a los mismos bytes dos veces.
- [x] Pasar todo el toolchain a `swift-6.4.2-RELEASE` (LLVM 21.1.6; decidido el 9 nov 2026): WPE WebKit necesita clang 18 o posterior, y el clang 17.0.6 fijado (`swift-6.0-RELEASE`) lo impide. Compilador, runtime y librería estándar de Swift, libc++, libunwind y compiler-rt suben juntos; los cambios de Minkar en LLVM se trasladan (`__MINKAR__`, el triple `minkar`, libSystem sin inicializadores estáticos en compiler-rt y libc++), y tapi-1600 se adapta a LLVM 21 (no hay un tapi posterior). Hecho cuando todo el sistema compile con él y pasen la suite completa y los gates M2 y M3. *Hecho (10 nov 2026, `38d3af4398`..`3a3052ab07`):* compilación desde cero sin pasos a mano, `manifest.sh check` (los mismos bytes dos veces), lote 1966/0 con los gates M2 y M3, suite completa sin fallos propios (tres fallan igual con clang 17: `display-reconfigure` dos veces y el ajuste de líneas de la consola). Por el camino: las cabeceras de libc++ 21 en el SDK, los stubs de arranque con `llvm-readtapi`, la capa de Darwin y `dispatch_async_swift_job` con importación débil para Swift 6.4, la versión del sistema leída sin CoreFoundation mientras se inicializan las librerías (todo programa con Combine fallaba), `isNSString__` y las demás comprobaciones de clase que envía el runtime de Swift 6.4 a Foundation, el cotejo Unicode de CoreServices (GLib lo usa en cuanto hay Carbon), y los archivos generados de un árbol autotools fechados después de sus fuentes.
- [x] Targets de bmake por capa: `bmake <capa>` y `bmake <capa>-check`, `layers-check` e `image-check`. Los frameworks de terceros pasaron a la Fase 3.
- [x] CI: `.github/workflows/build-boottest.yml` compila el toolchain (en caché), el sistema y la imagen, y pasa `boottest GATE=M2`. Desde el 29 oct corre en `main` y a mano, para no agotar los minutos de Actions de una cuenta gratuita.
- [x] Inventario de símbolos: `tools/symbols/`; Minkar cubre el 72,0% de los imports de sistema de las cuatro apps de referencia (68,3% la mañana del 31 oct).
- [x] Documentar el build: `BUILDING.md` con host, paquetes, tiempos medidos y 17 errores frecuentes.

* [x] **Criterio de salida (M0):** cumplido en la CI (`05fa75cb65`, y de nuevo en `9191fe8458`, 29 oct 2026).

## Fase 1 — Arranque Darwin

Objetivo: XNU arranca en PC x86-64 genérico hasta lanzar PID 1 con el disco raíz montado. Es la prioridad oficial número dos y bloquea todo lo demás.

**Actividades**

- [x] Cargador EFI: kernel prelinkado, framebuffer GOP, mapa de memoria y tablas ACPI; desde el 30 oct pasa también las tablas de configuración (SMBIOS: modelo y UUID de la máquina).
- [x] Plataforma XNU para PC genéricos: ACPI, APIC, TSC/HPET, sin SMC.
- [x] Kexts mínimos:
  - [x] AHCI con MSI (`MinkarAHCIPort`).
  - [x] virtio-blk (`MinkarVirtio`).
  - [x] NVMe (`MinkarNVMe`, escrito desde la especificación): el PC 1 arranca desde su SSD (unos 1,4 GB/s de lectura y 1,1 GB/s de escritura). *Pendiente:* en QEMU las lecturas por el dispositivo de bloques van a 1 MB/s.
  - [x] USB xHCI + HID (`MinkarXHCIPort`, `IOUSBFamily`, `MinkarUSB`), en QEMU y en el PC 1.
  - [x] e1000 y e1000e (`MinkarE1000`), con el código compartido de Intel para I219-V e I211.
  - [x] virtio-net (`MinkarVirtio`).
  - [x] Framebuffer GOP como consola, a resolución nativa por EDID (3440×1440 en el PC 1).
- [x] Raíz HFS+ (APFS y ZFS, más adelante); FAT (`msdosfs`); exFAT con driver propio en lectura y escritura, `mount_exfat`, `fsck_exfat` y `newfs_exfat` (31 oct).
- [x] Consola serie para los gates.
- [x] Panic logs legibles: `tools/panic-report.py` simboliza cada frame; probado con un panic real.
- [x] Arranque en QEMU con KVM y TCG. *Parcial:* en los runners de la CI todavía solo TCG.
- [x] SMP (30 oct 2026): corren todas las CPU que la MADT habilita. En el PC 1, ocho hilos (`hw.ncpu` 8, 4 físicos); en QEMU probado con 2 a 64 procesadores y varias topologías.
- [x] Botón de encendido (31 oct 2026): IOACPIFamily toma el evento fijo de ACPI o el dispositivo PNP0C0C y `powerd` pide un apagado ordenado; en el PC 1 el equipo se apaga. `shutdown` y `reboot` tardan unos 2 s (antes 20 s, por diskarbitrationd), y las imágenes de prueba arrancan con `msgbuf=1048576`.
- [x] Lista de hardware de referencia (`Docs/Hardware.md`): PC 1 y PC 2 (1 nov 2026) documentados; AMD más abajo.
- [ ] Más de 64 procesadores y x2APIC (después de Minkar 19.6.0); hoy una máquina mayor corre con sus primeras 64.
- [ ] Procesadores AMD: el kernel los reconoce, pero Minkar nunca ha corrido en uno. Primero en QEMU con `-cpu EPYC`, luego en un PC AMD.
- [ ] Arrancar sin `pci_keep_console=1`; IOKit tarda más de 30 s en quedar en reposo.
- [ ] IPv6 en la red del PC 1: RDNSS y respuestas DNS sobre IPv6, que la red de usuario de QEMU no da (movido de la Fase 2 el 31 oct).
- [ ] Wi-Fi del PC 1 (Intel, M.2): sin driver.

* [x] **Criterio de salida (M1):** cumplido en QEMU, en el PC 1 (desde SATA y desde NVMe, con SMP) y en el PC 2 (1 nov 2026: desde NVMe, 24 hilos con carga, 40 GB, disco y reinicio pasan en `minkar-hwcheck`; `9c3dbe9873`, `c129adf018`, `e9a4446222`).

## Fase 2 — Userland base

Objetivo: un sistema Unix multiusuario completo sobre Darwin, gestionado por launchd y con los servicios que luego necesitan Atlas y las apps.

**libSystem y dyld**

- [x] dyld real con `@rpath`, `@loader_path` y re-exports: es el dyld original (APSL); una prueba compila un programa, sus bibliotecas y una que reexporta, y los ejecuta (31 oct). El resto de libSystem, los stubs ruidosos y la shared cache pasaron a la Fase 3.

**launchd**

- [x] Portar a Darwin la versión antigua con XPC usada en la era FreeBSD.
- [x] `launchctl` con `load`, `bootstrap`, `list` y `kickstart` (`kickstart [-k] [-p]` desde el 31 oct, probado reiniciando notifyd).
- [x] Plists XML además de JSON, para usar `LaunchDaemons` y `LaunchAgents` tal como los escriben las apps (sshd arranca desde `com.openssh.sshd.plist`).

**Userland BSD, por lotes**

- [x] Arranque: `sh`, `ls`, `cp`, `mv`, `rm`, `mount`, `ifconfig`, `ps`, `kill`.
- [x] Comandos genéricos de Unix desde FreeBSD `main` con una capa propia de compatibilidad: todos los BSD de la lista de referencia (27 oct). A `grep` le faltan `-Z` y `-J`.
- [x] Herramientas que faltaban en la imagen: 337 de 354 comandos de la lista de referencia (`Docs/Commands.md`), entre ellos `dtrace` con tipos del kernel (CTF), `system_profiler`, `pmset` con `powerd`, `networksetup`, `kextload`/`kextstat`, `emond`, `fs_usage`, `leaks`, el servidor NFS con `rpcbind` y, desde el 31 oct, el driver exFAT propio (lectura y escritura), `mount_exfat` y el bundle `exfat.fs`: diskarbitrationd monta discos exFAT en lectura y escritura. Están todos los comandos de la fase.
- [x] Movidos a fases posteriores (30 oct): `pbcopy`, `pbpaste` y `screencapture` a la Fase 4; `open`, `lsappinfo`, `say` y `mdfind`/`mdls`/`mdutil` a la Fase 5; `osascript`, `opendiff` y `tmutil` a la Fase 6.
- [x] Fuera hasta después de Minkar 19.6.0 (decidido el 30 oct): APFS (no hay implementación abierta), SMB (Samba es GPL) y `fdesctl`.
- [x] Usuario avanzado: `zsh`, `tar`, `ssh`, `curl`, `make` y `git` 2.56.0. Los comandos de git en Perl, Python y Tcl pasaron a la Fase 6.
- [x] Herramientas del sistema: `plutil`, `defaults`, `sw_vers`, `ditto`, `hdiutil` y `diskutil`; `chmod` con la sintaxis de ACL de la referencia, y el kernel aplica las entradas allow además de las deny (28 oct).

**Daemons de soporte** (necesarios para las Fases 5 y 6)

- [x] `notifyd`, con el `notifyutil` original (APSL) desde el 31 oct: se prueban posts, clientes en espera y estados, también después de un reinicio.
- [x] DiskArbitration (30 oct 2026): framework y `diskarbitrationd` originales (APSL) con los bundles `hfs.fs`, `msdos.fs` y `exfat.fs`, y `diskutil` y `autodiskmount` propios; `hdiutil attach` monta a través de él. Sin Security, el daemon decide los permisos por sí mismo.
- [x] `powerd` y `pmset` propios: ajustes, aserciones, eventos y el botón de encendido; el reposo se rechaza porque aún no hay ACPI S3.
- [x] Movidos el 31 oct: `logd` pasa a la Fase 3 con `os_log` (mientras tanto, `syslogd` y `aslmanager`); la gestión de usuarios estilo Directory Services pasa a la Fase 5, con la ventana de login que la necesita.

* [x] **Criterio de salida (M2):** arranque con launchd como PID 1, red por DHCP, login en consola y acceso por SSH.

**Estado (31 oct 2026): Fase 2 cerrada.** M2 cumplido: `boottest GATE=M2` ve sus 13 marcadores en unos 75 s bajo QEMU sin KVM, e incluye la configuración DNS de configd (`scutil --dns`) y la resolución por mDNSResponder (`dscacheutil -q host`). Todos los comandos de la fase están en la imagen, y lo que no es un comando pasó a la fase que lo necesita.

- [x] launchd-842 arranca como PID 1.
- [x] `dhcpclient` obtiene 10.0.2.15 para `en0`.
- [x] root entra por la consola con PAM.
- [x] root entra por SSH desde el anfitrión con una clave.

Componentes:

- [x] **IONetworkingFamily propio** (BSD-2), con la API de IOKit.
- [x] **Driver `MinkarE1000`** para las NIC Intel e1000 y e1000e.
- [x] **`ifconfig`, `dhcpclient` e `ipconfig` propios.**
- [x] **OpenSSH 10.6p1 sin OpenSSL**, con PAM.

Cerrado después:

- [x] MSI para disco y red.
- [x] IPv6: direcciones link-local y SLAAC, `ping6` y consultas DNS sobre IPv6, probados en QEMU desde el 31 oct. RDNSS y las respuestas DNS sobre IPv6 se comprueban en el PC 1 (Fase 1), porque la red de usuario de QEMU no los da.
- [x] DNS sobre IPv4 con `libresolv` y mDNSResponder (gate M2); sobre IPv6, como en el punto anterior.
- [x] `getaddrinfo`, `id` y `dscacheutil`.
- [x] El gate M2 también verifica la resolución de nombres.
- [x] Corregida la carrera en el driver AHCI con MSI que hacía fallar launchd.

Lo que quedaba de la Fase 2 sin ser un comando se movió el 31 oct: el resto de libSystem, los stubs ruidosos, la shared cache de dyld y `logd` a la Fase 3; Directory Services a la Fase 5; los comandos de git en Perl, Python y Tcl a la Fase 6.

## Fase 3 — Frameworks base y Swift

Objetivo: una app de línea de comandos en Objective-C o Swift, escrita para la API 19, compila y corre sin cambios; después se sube a la API 20 y a la 21. La Fase 2 ya está cerrada, así que esta fase no espera a nada.

**Decisiones de arquitectura (tomar primero)**

- [x] **Runtime Objective-C: objc4 (APSL), decidido.** Ya está en el árbol: `Libraries/Makefile` compila `objc4` antes de `Libsystem`, y `Minkar.sdk` toma sus headers. Es el ABI original en todo el sistema, requisito de la Fase 7. Falta solo confirmar `__objc_classlist` y `__objc_imageinfo` en un binario compilado.
- [x] **CoreFoundation: CF-Lite (APSL), decidido el 25 oct 2026.** Es la CF actual de `Frameworks/CoreFoundation`, con toll-free bridging propio (`CFBridging.c`, `CFRuntime.c`) y puente CF ↔ XPC. No se reemplaza por la CF de Swift. La CF-Lite pública es anterior a la API 19: las APIs de CF que faltan hasta la API 21 se añaden a mano, con la CF de swift-corelibs (Apache 2.0) como referencia. `Frameworks/CoreFoundation.old` se eliminó el 26 oct 2026.
- [x] **Foundation para Swift: Foundation de Objective-C más overlay `libswiftFoundation`, decidido el 25 oct 2026.** Es el modelo de la API 21. Base: la última versión abierta del overlay de Darwin (Apache 2.0) en el historial de `swiftlang/swift`, actualizada a Swift 6.0. swift-foundation queda solo como referencia para APIs nuevas.
- [x] **Runtime de Swift: Swift 6.0 en `/usr/lib/swift`, decidido el 25 oct 2026.** Misma versión que el LLVM del árbol (fork de swiftlang, rama `swift-6.0-release`). Punto de partida: no hay compilador, stdlib ni overlays de Swift en el árbol; solo el overlay de Dispatch (`libdispatch/src/swift`) y las anotaciones Swift en los headers de CF.
- [x] **Combine y SwiftUI, decidido el 25 oct 2026.** Ambos pertenecen a la API 19. Combine entra en la API 19 sobre OpenCombine (MIT), renombrado como módulo `Combine` e instalado como `Combine.framework`; depende del gate del toolchain Swift. Las API 19, 20 y 21 se cierran sin SwiftUI de forma explícita. OpenSwiftUI (MIT, desarrollo temprano) queda en observación y la decisión se revisa midiendo cuántas apps de referencia lo necesitan.

Orden de decisión: CoreFoundation condiciona la Foundation para Swift, y esta condiciona el runtime de Swift.

**Actividades**

0. Traer los frameworks de terceros restantes (movido desde la Fase 0): 13 frameworks se compilan e instalan; CFNetwork está comentado, y Security y OpenDirectory no se compilan.
1. Completar la CF-Lite hasta la API 21, con pruebas de bridging en cada cambio.
   - [x] Las funciones de cadena de CF aceptan las cadenas propias de Foundation (`CFLocaleCreate`, `CFStringGetCString`... leían basura de las de `NSMutableString`) (`42a40ffab0`, prueba `foundation-cf-string-bridging`, suite completa 2888/0).
   - [x] `CFArray` y `CFDictionary` tienen toll-free bridging con `NSArray` y `NSDictionary` en los dos sentidos: los objetos de CF aceptan mensajes de Objective-C (`__NSCFArray`, `__NSCFDictionary`) y las funciones de CF aceptan las colecciones de Foundation. Evidencia: commit "CoreFoundation and Foundation: arrays and dictionaries bridged both ways", prueba `foundation-cf-bridging` (13/0), suite completa 2761/0, M2 PASS.
2. Auditar Foundation contra la API 19 empezando por lo que importan las apps de referencia. *Hecho (25–26 oct):* `NSException`, `NSUUID`, `NSSecureCoding`, `NSRegularExpression`, `NSJSONSerialization` (con JSON5), `NSCache`, `NSInvocation` con la convención System V, `NSHashTable` y `NSMapTable` débiles, listas de propiedades binarias byte a byte como la referencia, `NSProgress` y la API moderna de `NSString`, además de decenas de fallos encontrados al probar. *Hecho (31 oct):* `NSProcessInfo` con `operatingSystemVersion` (10.15.0), `isOperatingSystemAtLeastVersion:`, `operatingSystemVersionString`, procesadores, memoria y `systemUptime` (`fb6e25b877`). *Hecho (31 oct):* `NSURLSession` sobre libcurl: configuración, tareas de datos, subida y descarga, delegado y bloques de finalización, redirecciones y retos de autenticación a través del delegado, datos de reanudación, URL `file:` y `data:` (`00617d5f6c`); el run loop del hilo principal ejecuta la cola principal de dispatch, que nunca se ejecutaba en un programa movido por `NSRunLoop` (`c88f713757`); `NSOperation`, `NSBlockOperation` y `NSOperationQueue` reescritas sobre dispatch, con `mainQueue`, `currentQueue`, límite de concurrencia, operaciones asíncronas y barreras (`3f886f812e`); Base64 y `rangeOfData:options:range:` en `NSData`, `NSHTTPURLResponse` (tipo MIME, charset, longitud desconocida), cuatro claves de `NSError` que no enlazaban, URL relativas que perdían el puerto, `MIN`/`MAX`/`ABS` (`81d2593fb7`). Pruebas `foundation-url-session` (30/0, con HTTPS), `foundation-operation-queue`, `foundation-main-queue`, `foundation-url-support`; suite completa 2826/0, M2 PASS. Después (`b48794b0dc`, `dc214ec2ce`, `852676bdeb`, `5671610ec9`): cookies HTTP (`NSHTTPCookie` y `NSHTTPCookieStorage` estaban vacíos), que `NSURLSession` envía y guarda; `pathComponents` no conservaba `/`, y `createDirectoryAtPath:withIntermediateDirectories:` volvía relativas las rutas absolutas; `addObserverForName:object:queue:usingBlock:`; el comportamiento por defecto de `NSDateFormatter` sobre CFDateFormatter (patrones Unicode, estilos, locales; solo funcionaban los formatos `%Y`). Pruebas `foundation-cookies`, `foundation-paths-blocks`, `foundation-date-formatter`; suite completa 2871/1 (una prueba intermitente, que volvió a pasar), M2 PASS. Después la API de ficheros por URL (`8b9592659f`, `4738af30ea`, `e2ebc901c5`): todos los directorios estándar en `NSSearchPathForDirectoriesInDomains` (conocía tres), URL de fichero como las hace la API (`file:///a%20b/`, `path` decodificado), valores de recursos y marcadores de NSURL, métodos de URL de NSFileManager y NSBundle; prueba `foundation-file-urls`; suite completa 2907/0, M2 PASS. `tools/symbols/selector-inventory.py` (`093bf4113e`) lista los mensajes de Objective-C que envían las apps de referencia y que ninguna clase de Minkar implementa: el 31 oct está el 80,0 % de 3305 (77,9 % antes de las URL de fichero). Después procesos, ficheros y formateadores (`2ebf423f7b` a `15c3a8fd96`, 9 commits): NSTask sobre posix_spawn con su API moderna (cerraba la salida estándar del hijo si no se le daba y recogía a todos los hijos del proceso); métodos con error y manejadores de lectura de NSFileHandle; temporizadores con bloque; el gestor de ficheros liberado en uso (se escribía su isa a mano) y escrituras que no truncaban; NSURLComponents y NSURLQueryItem; NSDateInterval; NSByteCountFormatter, NSDateComponentsFormatter y, sobre ICU, NSRelativeDateTimeFormatter, NSDateIntervalFormatter y NSListFormatter; NSDateComponentUndefined como NSIntegerMax. Pruebas `foundation-process-io`, `foundation-components-formatters`; suite completa 2943/0, M2 PASS; selectores 81,2 %. *Queda en `NSURLSession`:* la caché de URL, las métricas de tareas (necesitan `NSDateInterval`), tareas de stream y WebSocket, retos de confianza del servidor (necesitan Security). *Fallo conocido:* `NSKeyedUnarchiver` no lee la lista de propiedades XML que escribe `NSKeyedArchiver`. Sin probar: el comportamiento dependiente del locale (los datos de locale UTF-8 están en la imagen desde el 27 oct).
3. Security (APSL) para certificados y keychain básico. TLS en `NSURLSession` funciona sin él (libcurl con mbedTLS y `/etc/ssl/cert.pem`); los retos de confianza del servidor (`SecTrust`) y los certificados de cliente lo necesitan.
4. LaunchServices: base de datos de bundles y tipos UTI; `open` y Files dependen de ella. *Hecho en WS4a:* el servicio de sesión, que guarda las apps en ejecución (`launchservicesd` y el cliente de la sesión en `LaunchServices.framework`, `996178d949`). *Hecho (4 nov 2026, `0331f57894`, `345636f2f5`):* launchservicesd guarda las apps instaladas, sus tipos de documento y los tipos que declaran, dice qué apps abren un archivo y entrega los documentos a una app (en el momento o después de su arranque); `LSOpenCFURLRef`, `LSOpenFromURLSpec`, `LSCopyDefaultApplicationURLForURL`, `LSCopyApplicationURLsForURL`, `LSRegisterURL`, `LSRevealInFiles`; AppKit entrega los documentos a `application:openURLs:` y NSWorkspace abre a través de él. Prueba local `launchservices` 24/0. Se fue el código de la base SQLite de ravynOS (`e7ae74a2e5`).
5. CoreText y CoreData: implementación mínima guiada por uso real (ver la guía técnica de la pila gráfica).
6. Swift 6: stdlib y runtime completos, con concurrencia en el sistema, y module maps para los frameworks.
7. API 19 → 20 → 21, subiendo `ProductVersion` (10.15 → 11.0 → 12.0) al cerrar cada una; cada cierre es una versión de Minkar (19.6.0, 20.0, 21.0).

Llegan de la Fase 2 (31 oct 2026):

- [ ] Completar libc, libm, pthread, libdispatch, libxpc, notify y os\_log. *Parcial:* desde el 31 oct las apps de referencia encuentran todos sus imports de libSystem (308 de 308): `os_log` escribe sus mensajes en syslogd, con `%{public}`/`%{private}` y `%@`, y en stderr en modo DT (antes no escribía nada, y el `<os/log.h>` del SDK no tenía `os_log()`); `syslog(3)` vuelve a llegar a syslogd; `os_log_error`/`os_log_fault` con los nombres de la referencia; `os_activity`; `_availability_version_check` para `@available`; `launch_activate_socket` (`0709544df7`, `c0923c1bc7`, `5051c781b4`). También están las demás bibliotecas de `/usr/lib` que importan: `libsqlite3` (SQLite 3.54.1, con el intérprete `sqlite3`, `bbe573a8a5`), `libxar` (xar-503) y nuestra `libcompression` (ZLIB, LZMA, LZFSE) (`cae9a9c4f4`); CommonCrypto tiene `CCDigest` (`88e4800b96`). libxpc tiene conexiones reales desde el 31 oct (`b2a0bee6ce`): escuchas anónimas y endpoints, servicios Mach a través de launchd con interrupción y relanzamiento, respuestas, descriptores, puertos y memoria compartida en los mensajes, las credenciales del emisor; los arrays funcionan (antes no podían contener nada). Prueba `libxpc-connection` (14 comprobaciones). Desde el 3 nov: plists XML (`xpc_create_from_plist`), los entitlements del otro extremo (`xpc_connection_copy_entitlement_value`, de la copia de su firma que guarda el núcleo) y transacciones sobre las de liblaunch (`219ac6ca38`, `4709c75447`; pruebas `libxpc-plist`, `libxpc-transactions`). `xpc_set_event_stream_handler` sigue siendo un stub: la parte de launchd está, aún no hay proveedor de eventos. `NSObject` es solo `isa`, como en la ABI de referencia (llevaba los campos de la cabecera de CF; las clases puente los declaran ellas mismas, `a3a9d005fc`); lo que el runtime de Swift necesita del SDK: `<os/signpost.h>`, `os_system_version_get_current_version`, `_dyld_is_objc_constant` (`2f0c5038af`). Foundation reexporta CoreFoundation, como la referencia (`55ac68b1f0`). `math.h` compatible con la referencia desde el 26 oct; a `vis(3)` le faltan las flags nuevas.
- [ ] Stubs ruidosos (log en la primera llamada). *Parcial (31 oct):* `<minkar/stub.h>` registra (`org.minkar.stubs`; en logd desde el 3 nov, se lee con `log show`) la primera llamada a un stub en cada proceso, con la imagen que lo llamó, y en stderr en modo DT; los 875 métodos y funciones sin implementar de Foundation, AppKit, CoreData y QuartzCore lo usan (antes escribían en stderr en cada llamada), y los 12 stubs silenciosos de libxpc ya avisan. `tools/symbols/trivial-functions.py` lista las funciones exportadas que no hacen nada (404 el 31 oct, muchas de terceros y vacías a propósito): el resto se revisa desde esa lista.
- [ ] Shared cache de dyld (opcional al principio).
- [x] `logd`, con `os_log` (3 nov 2026, `5d587b3d44`, `6e7a02196d`): `os_log` envía sus mensajes a logd, que guarda default, error y fault en disco e info (debug si se pide) en memoria; el comando `log` los muestra, sigue, configura y borra, con predicados de NSPredicate; cada usuario ve los suyos. Prueba `logd` 23/0. syslogd solo los recibe cuando no hay logd que los tome; `log show` y `log stream` muestran también los mensajes de `syslog(3)` (del almacén de syslogd).

**Toolchain Swift** (el prototipo es gate antes del resto del trabajo de Swift)

- [x] Prototipo 1: compilar `swiftc` (`swiftlang/swift`, rama `release/6.0`) en el host Ubuntu como compilador cruzado, contra el LLVM `swift-6.0-release` del árbol. *Hecho (3 nov 2026, `1d2b7e8d9d`):* `Developer/Default.xctoolchain/swift` compila `swift-frontend` (`swiftc`, `swift`) para el host contra el LLVM y el clang de la cadena de herramientas, desde el tarball de la versión de Swift fijado por SHA-256 (un parche: sin ASTGen cuando no hay swift-syntax); aún sin macros. Desde el 4 nov 2026, con swift-syntax y macros (`bf2464179d`).
- [x] Prototipo 2: compilar `libswiftCore` para `x86_64-apple-macosx10.15` contra `Minkar.sdk` y ejecutar un "hello world" de Swift (no Embedded) en QEMU. *Hecho (3 nov 2026, `1d2b7e8d9d`):* `Libraries/swift` compila el runtime y la biblioteca estándar desde Linux en `/usr/lib/swift`; hizo falta NSObject con el tamaño de referencia (`a3a9d005fc`) y algunas piezas del SDK (`2f0c5038af`). Prueba `swift` 10/0: cadenas, protocolos, clases, genéricos, errores y colecciones.
- [x] `libswift_Concurrency` y overlays de sistema: `Darwin`, `ObjectiveC`, `CoreFoundation` y `Dispatch` (desde `libdispatch/src/swift`). *Parcial (4 nov 2026, `283d9da3e1`):* Concurrency sobre libdispatch y el overlay `Darwin`; prueba `swift` 19/0 (main asíncrono, grupos de tareas, actores, cancelación, AsyncStream). *Hecho (4 nov 2026, `30f8226a8d`):* los overlays `ObjectiveC`, `Dispatch` y `CoreFoundation` de Swift 5.4, compilados con Swift 6.0 (no desde `libdispatch/src/swift`, decidido el 4 nov: el de Darwin es la ABI de las apps compiladas para la plataforma de referencia); CoreFoundation hace de los bloques y sus objetos objetos de Objective-C sin Foundation (`bc807ce599`). Prueba `swift` 26/0.
- [x] Overlay `CoreGraphics`. *Hecho (4 nov 2026, `dc69c6b38f`):* de Swift 5.4 como los demás, con `CGFloat` generado por su plantilla gyb. Antes CoreGraphics pasó a los tipos de referencia: `CGFloat` es `double` en 64 bits y `CGContextRef` un tipo CF (`29f392e725`), los contadores son `size_t`, las enumeraciones tienen tipo, y se añadió la API que usa el overlay con sus nombres de Swift en las cabeceras (`f61dfd5b34`). Prueba `swift` 34/0 (geometría, transformaciones, colores, rutas, dibujo en un mapa de bits). Falta: nombres de Swift para el resto de CG y la nulabilidad de las cabeceras.
- [x] Overlay `libswiftFoundation` a partir de la última versión abierta, actualizado a Swift 6.0. *Hecho (5 nov 2026, `7093882f38`):* el overlay de Swift 5.4 compilado con Swift 6.0 contra la Foundation de Minkar, con sus planificadores y publicadores de Combine (run loop, cola de operaciones, temporizador, notificaciones, KVO, sesión de URL; el de DispatchQueue en el overlay de Dispatch). Antes Foundation recibió lo que Swift lee de ella y lo que le faltaba (`a3b073f23b`): los tipos de referencia (`NSStringEncoding`, `NSComparisonResult`, `NSRectEdge`), enumeraciones de cadenas con tipo, propiedades, genéricos y nulabilidad en las cabeceras, y la API 19 que llama el overlay: NSCalendar y NSLocale sobre CF (`16ab71b9a3`), la aritmética de NSDecimal y NSDecimalNumber (`e25e40ac29`), NSOrderedSet, las diferencias de colecciones, unidades y medidas, NSPersonNameComponents, NSLinguisticTagger, NSISO8601DateFormatter y NSItemProvider (`df5cbb9610`). KVO entraba en recursión infinita en un setter observado y no tenía notificaciones iniciales (`7233b00fcd`). Pruebas `swift` 71/0 (Foundation y Combine desde Swift), `foundation-api19` 49/0; suite completa 3783/1 (una prueba de cursores intermitente, pasa sola), M2 PASS. *Falta:* la confianza del servidor en URLError y en las sesiones (necesitan Security); `URLSessionWebSocketTask` es un stub que falla como no soportado.
- [x] Combine (API 19) desde OpenCombine. *Hecho (5 nov 2026, `7b305aa383`):* OpenCombine 0.14.0 (MIT) compilado como `Combine.framework`, un módulo con evolución de biblioteca que los programas enlazan como framework; `Optional.Publisher` y `Result.Publisher` anidados como en Combine. Probado con sujetos, operadores, `@Published`/`ObservableObject`, `Future`, `Zip`, decodificación, y los planificadores y publicadores de Foundation y Dispatch.
- [x] XCTest, para las suites de tests del criterio. *Hecho (5 nov 2026, `78caf199df`):* swift-corelibs-xctest (Apache-2.0) en la versión de la toolchain, compilado sobre la Foundation de Minkar como `XCTest.framework` (en `Developer/Library/Frameworks` de la plataforma para compilar, en `/Library/Developer/Frameworks` de la imagen). Sin gestor de paquetes, `xctest-discover` escribe el main de un programa de tests a partir de sus fuentes (subclases, tests heredados, extensiones, tests asíncronos). Hizo falta `NSProcessInfo`, `NSFileHandle` y `NSDateFormatter` con la forma de la referencia, y `getCFRunLoop`, que devolvía el propio NSRunLoop (`bf327957e7`). Prueba local `xctest` 15/0.
- [x] Instalar todo en `/usr/lib/swift` dentro de la imagen y añadir un gate de `boottest` que ejecute un binario Swift. *Hecho (`ea8f2964f1`):* el gate M3-swift ejecuta `swift-hello` sobre el runtime de `/usr/lib/swift`, y el gate M3 ejecuta los programas de test en Swift de las herramientas.

**Criterio de salida (M3):** un conjunto fijo de herramientas CLI de código abierto escritas para la API Cocoa (ObjC y Swift) compila y pasa sus tests en Minkar. El conjunto (fijado el 5 nov 2026), compilado en el host y con sus tests ejecutados en la imagen (gate M3 de `boottest`):

| Herramienta | Lenguaje | Licencia | Qué ejercita |
|---|---|---|---|
| swift-argument-parser, con sus ejemplos | Swift | Apache-2.0 | La base de casi todas las herramientas CLI en Swift |
| SwiftFormat | Swift | MIT | Foundation a fondo; una suite de tests grande |
| xcbeautify | Swift | MIT | Expresiones regulares, cadenas, Foundation |
| Yams | Swift | MIT | Codable, y una biblioteca en C enlazada |
| Rainbow y Files | Swift | MIT | Ficheros, rutas y la terminal |
| mogenerator | Objective-C | MIT | CoreData y Foundation |
| tag | Objective-C | MIT | Valores de recursos de NSURL y etiquetas de ficheros |

Fuera: herramientas sin licencia o con una dudosa, y las que usan API privada.

*Avance (5 nov 2026):* el criterio se ejecuta (`bmake m3-tools` y después `bmake m3-check`: `Tests/M3` compila cada herramienta y sus tests sin el gestor de paquetes, `/usr/local/tests/m3/run.sh` los ejecuta, `96f5f65249`). swift-argument-parser pasa sus cinco suites, 493 tests. Lo que necesitó: `NSTask`, `NSPipe` y `NSFileManager` tal como los lee Swift, el bundle de una herramienta de línea de comandos (apuntaba dos directorios más arriba), plantillas de expresiones regulares, valores de la línea de comandos que no son listas de propiedades sin error de análisis (`f21472a91d`); conjuntos de caracteres con puente a CF en ambos sentidos (`13a6cebd3d`); el calendario ISO 8601 (`5694846076`); la API con errores de FileHandle, y de la API 21 `FileHandle.bytes`, `lines` y las cargas asíncronas de URLSession (`89b9375fa7`). Pruebas locales `swift` 81/0. Después pasan Rainbow (139 tests) y Files (66 tests) (`7d07cc60c5`): AppKit no se podía importar desde Swift, porque CoreVideo, que importan las cabeceras de QuartzCore, nunca se compilaba (`d3ee485fac`); NSFileManager no se podía subclasificar (sus operaciones estaban en una subclase privada que devolvía toda reserva), a su cabecera le faltaba `NSFileAttributeKey`, una URL de fichero relativa conservaba sus segmentos `.`, y se ignoraba `CFFIXED_USER_HOME` (`987a49fad0`). Pruebas locales `swift` 86/0. Después pasa Yams, 192 tests (`11604bfcb9`; destinos en C en `Tests/M3`): la línea de un rango vacío era la anterior, así que recorrer líneas no avanzaba; los nombres de las codificaciones eran números; los estilos científico, de moneda y deletreado de NSNumberFormatter no estaban implementados y ahora pasan por CFNumberFormatter (`288eb57cd4`); la codificación con configuración, API 21 (`6b0d862c24`); la API de métricas de XCTest, y las clases de test disponibles desde sistemas posteriores solo se ejecutan en ellos (`7dbfad469a`). Pruebas locales `swift` y `xctest` 105/0. Después pasa xcbeautify, 643 tests (`d2eda99167`; paquetes que dependen de otros en `Tests/M3`): los recursos de un bundle plano se buscaban en una carpeta Resources que no tiene, `\` partía los nombres de fichero como `/`, los ficheros de texto sin marca de orden de bytes se leían como NEXTSTEP y no como UTF-8, y los métodos del delegado de NSXMLParser eran obligatorios (`d1c81abeff`); ListFormatter y RelativeDateTimeFormatter no tenían nombre en Swift (`de216ae5c0`). Pruebas locales `swift` y `xctest` 110/0. Después mogenerator, la primera herramienta en Objective-C, pasa sus tests en Objective-C y en Swift (`0c036fd389`; 11 comprobaciones: las clases que genera son las esperadas y sus programas guardan objetos): las clases de modelo de CoreData no se podían construir desde código ni archivar, las propiedades @dynamic no tenían accesores primitivos, el almacén XML perdía las fechas y sus cabeceras no tenían la forma que lee Swift (`484067beb4`); los predicados no tenían los operadores CONTAINS ni BETWEEN, IN buscaba subcadenas, LIKE y MATCHES no estaban implementados y no se podían archivar (`9bf58d789b`); NSKeyedArchiver perdía los objetos condicionales codificados más tarde, no se leía el Info.plist dentro del ejecutable de una herramienta, se ignoraba TMPDIR y KVC no tenía proxies de conjunto ni de conjunto ordenado (`3b7e4f8c30`). Pruebas locales `predicates`, `coredata` y `foundation-tools` 92/0; suite completa 3933/0. Después tag pasa sus 30 comprobaciones y SwiftFormat sus 4900 tests (`22a0e67f0d`; tag no trae tests propios, así que `tagTests` ejecuta cada una de sus órdenes): NSMetadataQuery y sus clases eran ficheros vacíos (`036fdb2fcf`); una ruta relativa daba una URL de fichero absoluta, `~` daba una URL nula, los nombres conocidos de NSTimeZone eran nulos y los errores de Swift tumbaban NSError (`a4875a6be4`); los nombres kMDItem (`708e61e7a7`). La suite de SwiftFormat mostró que el núcleo de release se compilaba con `-O0`: con `-O2` una llamada al sistema o un cambio de hilo cuesta la sexta parte, y la suite tarda 28 minutos en vez de 53 (`9db0205544`). *Cumplido (5 nov 2026):* el gate M3 pasa con las siete herramientas (22 marcadores); suite completa 3931/5 con el núcleo `-O2` (los cinco, corregidos o intermitentes conocidos).

- [ ] La prueba del generador de cadenas de Rainbow pide 50.000 cadenas por segundo; el 7 nov 2026 la VM del gate hizo unas 44.500 en el mismo Xeon E5-2697 v2 donde pasaba el 5 nov, así que el gate la deja fuera (`SKIP_RainbowTests` en `Tests/M3/rainbow`). Averiguar qué la hizo más lenta (libSystem aún a `-O0`, el modelo de CPU Ivy Bridge de la VM desde el 7 nov, el kernel y QEMU del host Ubuntu 24.04), medirlo y volver a incluirla.

**Siguiente: cobertura Cocoa.** Las herramientas del M3 son de línea de comandos y casi no usan AppKit. Medido contra las cuatro apps de referencia (5 nov 2026): selectores 83,5 %, símbolos importados 74,3 %, AppKit 70,2 %, CoreFoundation 60,3 %. El bloque trabaja desde las listas de lo que falta del inventario, con objetivos de selectores ≥ 90 %, AppKit ≥ 85 % y CoreFoundation ≥ 80 %, y añade más apps Cocoa de código abierto con las que medir. Primero: el layout por restricciones (`NSLayoutConstraint`, anclas, guías de layout, el lenguaje de formato visual, sobre el resolvedor kiwi) y `NSStackView`, que usan las cuatro apps; después WebKit, requisito de Minkar 19.6.0 (decidido el 5 nov 2026): WPE WebKit (LGPL y BSD, el port pensado para incrustar) bajo un `WebKit.framework` propio de Minkar con la API Cocoa (`WKWebView`, scripts y controladores de contenido, delegados de navegación y de interfaz, preferencias, y el antiguo `WebView`), que dibuja en vistas de AppKit; el navegador sigue siendo Ladybird. Después tablas basadas en vistas, `NSVisualEffectView`, los símbolos de clases puente de CF y Security.

*Avance (6 nov 2026):* layout por restricciones y `NSStackView` en AppKit (`3d987fdf45`): restricciones con elementos, anclas y el lenguaje de formato visual, guías de layout, tamaños intrínsecos con sus prioridades, `fittingSize`, resueltas con kiwi 1.4.8 (BSD modificada, fijada); `NSEdgeInsets` en Foundation (`187850779e`); ejecutar un selector en un hilo principal que aún no había arrancado su bucle lanzaba una excepción (`c692e35c64`). Prueba local `autolayout` 28/0; los gates M2 y M3 pasan.

*Avance (9 nov 2026):* medido de nuevo contra las cuatro apps de referencia: símbolos importados 85,5 % (74,3 %), AppKit 89,6 % (70,2 %), CoreFoundation 97,8 % (60,3 %), CoreGraphics 100 %, Foundation 89,7 %, selectores 88,5 % (83,5 %). Los métodos de AppKit reciben y devuelven los tipos de la API: 531 declaraciones tenían los `float`, `int` y `unsigned` de Cocotron donde la API tiene `CGFloat`, `NSInteger`, `NSUInteger` y tipos de opciones con nombre, de modo que Swift veía `Float` e `Int32` y los binarios de la referencia pasaban doubles donde AppKit leía floats; con ellos se corrigieron las llamadas que pasaban la dirección de una variable más estrecha (`4748eba480`, `dd3915777d`). `NSEvent` con la numeración y los nombres de la API, deltas `CGFloat`, monitores locales y eventos CG (`88ef154c9d`); etiquetas y estados `NSInteger` y controles creados en una llamada (`78f0e89df9`); texto medido en líneas y los tamaños de la fuente del sistema, 13, 11 y 10 (`7929fdd9b4`); `NSColor` y `NSColorSpace` de la API 19, la geometría de CoreGraphics exportada con las reglas de la referencia y espacios de color por nombre (`f2e2c89459`, `1bf753c030`); tablas basadas en vistas (`2164fd35bb`, `9e64c1274d`); controladores de vista con hijos y presentación, `NSSplitViewController` y `NSTabViewController` (`0be9785692`); `NSAppearance`, `NSVisualEffectView`, vistas capturadas en mapas de bits (`6c251265bb`); pesos de fuente y las fuentes monoespaciadas del sistema (`5afc0696f0`); los nombres de los bindings (`fc45d9647f`); `NSWorkspace` abre aplicaciones con una configuración y avisa de los volúmenes (`30acc8f2ef`). Los símbolos de clases puente de CF, como se decidió el 9 nov 2026 (opción A): dyld busca en Foundation lo que los binarios importan de CoreFoundation y Minkar guarda en Foundation (`8c06f071ed`); CoreFoundation gana `CFAttributedString`, `CFStringTransform`, marcadores de URL y archivos alias (`b828d839c7`, `059e7bc40e`). Foundation: valores por defecto de suites y preferencias escritas como listas de propiedades XML (el formato de texto antiguo perdía números y datos), actividades del proceso, `NSBackgroundActivityScheduler`, completado de rutas y los volúmenes montados (`f232ca582f`, `0e6eeaecc9`, `dd161c6ec8`). Pruebas locales `appkit-graphics-api`, `appkit-controls-api`, `appkit-view-tables`, `appkit-view-controllers`, `appkit-appearance-api`, `cf-api19-additions` y `foundation-api19-more`; las puertas M2 y M3 pasan.

*Avance (9 nov 2026, segunda ronda):* símbolos importados 95,2 % (85,5 %), selectores 93,9 % (88,5 %); AppKit, CoreFoundation, CoreGraphics, Foundation, CoreServices, CoreText, QuartzCore y ApplicationServices no importan nada que falte (100 %). Siguen ausentes: Security (54 símbolos, el siguiente), WebKit (10, con su plan arriba) y las clases de API 20 y API 22 `UTType` y `SMAppService`. dyld busca en AppKit lo que los binarios importan de Foundation y Minkar guarda en AppKit (`b57ccbd06e`). Foundation: las clases de literales constantes que el compilador de la plataforma de referencia dispone en los binarios (`NSConstantArray`, `NSConstantDictionary`, los números constantes y las colecciones vacías), descriptores y manejadores `AppleEvent` (`NSAppleEventDescriptor`, `NSAppleEventManager`; `NSAppleScript` indica que no hay componente de scripting), directorios personales por usuario, tipos HFS como texto y la enumeración de tramos de atributos (`e03ced3524`, `852e608474`). CoreServices: un framework AE (descriptores, coerciones, manejadores en el mismo proceso), handles, recursos, `Gestalt`, `FSRef`, referencias de iconos, `MDItem`, información de elementos y manejadores por defecto (`5a5488ac29`, `65bb14584c`). CoreGraphics: el orden de bytes por defecto de los mapas de bits es el de los componentes en memoria, como en la plataforma de referencia (RGBA se guardaba A, B, G, R, y los programas que leían los píxeles de un contexto de mapa de bits veían los canales cambiados), formatos opacos e identificadores de tipo CF para sus tipos (`4a567351b9`, `8a4e9afeda`). CoreText: líneas, tramos, estilos de párrafo, fuentes por nombre y rasgos, `CTFontRef` como el tipo CF de la API (`c64e917b51`). QuartzCore: las capas dibujan (`display` no hacía nada), `renderInContext:`, `CAShapeLayer` (`46d7ca1a20`). ApplicationServices: números de serie de procesos y `UAZoom` (`dbada9a0a8`). AppKit: `NSAnimationContext` (no estaba implementado), `NSTextInputContext` y texto marcado en `NSTextView`, imágenes dibujadas por un bloque y como CGImage, rectángulos de ventana en la pantalla y pantalla completa, y unas 45 llamadas más de tablas, menús, paneles y vistas con desplazamiento (`1c8186c1c0`, `4d18786847`). Frameworks nuevos: UserNotifications, ServiceManagement, AddressBook, AudioToolbox, OSAKit, Carbon y Quartz (Quick Look) (`3d2be6b070`); donde Minkar aún no tiene el servicio (avisos en pantalla, salida de audio, una base de contactos, un componente de scripting), lo dicen en sus cabeceras y se comportan como si estuviera desactivado. Pruebas locales `coreservices-api19`, `foundation-literals-moved`, `coregraphics-byte-order`, `coretext-api19`, `quartzcore-layers-api`, `appkit-api19-more`, `frameworks-api19-small`, `imageio-api`, `appkit-more-api`; las puertas M2 y M3 pasan. Security (decidido el 9 nov 2026: subconjunto propio de Minkar ahora, las fuentes APSL de Security en `Frameworks/Security` después): autorización para administradores según regla (sin diálogo de autenticación aún), contraseñas del llavero por `SecItem` y las llamadas antiguas (en un archivo que solo lee el usuario, aún sin cifrar), claves y certificados sobre libcrypto, transformaciones de digest y verificación, y consultas de firma de código que informan que el código no está firmado (`d47c6ef6e3`, prueba `security-api19`). Los símbolos importados quedan en 99,0 %: falta WebKit (10), y `UTType` (API 20) y `SMAppService` (API 22). Después llegan las clases de la API de WebKit, con las páginas mostradas como texto hasta que WPE WebKit sea su motor: `WKWebView` con su configuración, scripts de usuario (guardados, no ejecutados: aún no hay JavaScript) y delegados, y el antiguo `WebView` (ver `webkit-utt-api`); y `UTType` de API 20 sobre las declaraciones de LaunchServices. Los objetivos del bloque se cumplen: símbolos importados 99,9 % (1414 de 1415; solo falta `SMAppService`, de API 22, fuera del alcance planeado), selectores 97,6 % (lo que queda son sobre todo los delegados del actualizador propio de las apps de referencia), y AppKit, CoreFoundation, CoreGraphics, Foundation, CoreServices, CoreText, QuartzCore, ApplicationServices y Security al 100 %.

**WebKit (cobertura Cocoa, requisito de Minkar 19.6.0; decidido el 5 nov 2026).** WPE WebKit 2.54 (LGPL y BSD, el port pensado para incrustar) compilado para Minkar como `PLATFORM(WPE)` sobre los servicios Darwin reales que WTF ya conoce (`OS(DARWIN)`: memoria virtual Mach, hilos, libdispatch), con una plataforma Minkar de su WPE Platform API, y encima el `WebKit.framework` propio de Minkar con la API Cocoa. Sus dependencias se descargan en versiones fijadas con su SHA-256 y se compilan como bibliotecas dinámicas (glib, libsoup y las partes LGPL como bibliotecas, sin enlazado estático). El navegador sigue siendo Ladybird.
- [ ] W0 — Las dependencias en `/usr/lib`, tal como las pide WPE 2.54.2 (`Source/cmake/OptionsWPE.cmake`; dibuja con el Skia que trae, así que sin cairo): glib (con pcre2 y libffi), libsoup 3 (con nghttp2, libpsl y brotli), glib-networking sobre OpenSSL, HarfBuzz con ICU, FreeType y fontconfig, libpng, libjpeg, libwebp, woff2, libxslt, libgcrypt (con libgpg-error), libtasn1, libxkbcommon y libepoxy (sobre Mesa); ICU y libxml2 son las de Minkar. Criterio: los tests propios de cada biblioteca que no necesitan red. *Parcial (6 nov 2026):* `minkar.extlib.mk` compila un proyecto fijado con Meson, CMake o autotools como bibliotecas dinámicas en el SDK y en `/usr/lib` (`c3129f586c`); ya están PCRE2 10.45, libffi 3.5.2 y GLib 2.86.1. GLib necesitó API 19 que faltaba: UTType y UTCoreTypes (`f8a4e037d9`), LaunchServices por tipo de contenido y por esquema de URL, con las URL que no son ficheros abiertas por la app de su esquema (`a25d499340`, `5767db4ce3`), ApplicationServices que incluye CoreServices (`cec5667897`), `NSUserNotification` (`cb38c88317`) y el puente toll-free CFURL/NSURL en ambos sentidos (`165dd272db`). Prueba local `glib-uti` 23/0 (bucle principal, PCRE2, GObject, GIO, procesos, UTType, LaunchServices, tipos de contenido y apps por defecto de GIO); `foundation-cf-bridging` 20/0 con URL. Después las herramientas de compilación de GLib para el host, con los subproyectos de Meson también fijados y sin descargas propias (`53ade2e20d`); nghttp2 1.66.0, Brotli 1.1.0, libpsl 0.21.5, libsoup 3.6.5 y glib-networking 2.80.1 (`2aadfd6d11`), que confía en el almacén de OpenSSL mientras Minkar no tenga framework Security. Prueba local `libsoup` 9/0: un servidor local responde a una sesión en HTTP y sobre TLS (certificado autofirmado), la lista de sufijos públicos, el decodificador de contenido. Después el resto (6 nov 2026): las cabeceras completas de ICU para las bibliotecas externas (`f394c90119`); HarfBuzz con ICU, libwebp, WOFF2 y Expat descargadas, y libpng, libjpeg-turbo, FreeType y Fontconfig como bibliotecas dinámicas a partir de las copias que Onyx2D y CoreText ya compilan en estático, con Fontconfig configurado como el de CoreText para que ambos compartan `/etc/fonts` y las cachés (`8cb321eac2`); Libgcrypt y libgpg-error, libtasn1, libxkbcommon con xkeyboard-config, libepoxy, y libxml2 y libxslt como `/usr/lib/libxml2.2.dylib` y `libxslt.1.dylib`, como las tiene la plataforma de referencia (`739fbc4e9a`). Pruebas locales `fonts-images` 9/0 y `crypto-xml` 7/0. Ya están todas las bibliotecas que pide W0; falta el criterio en sí: los tests propios de cada biblioteca, compilados para Minkar y ejecutados en la imagen. *Criterio, parcial (7 nov 2026):* `bmake w0-check W0_HOST=root@dirección` copia los tests propios de las bibliotecas (Tests/W0) a una máquina física con Minkar y los ejecuta allí como un usuario sin privilegios de root (`f9d9076530`, `be499de2d1`); en VM anidadas en otro hipervisor despertar un hilo cuesta cerca de un milisegundo y los tests de cerrojos de GLib no terminan, así que el criterio se comprueba en hardware. En el PC 2 pasan 16 bibliotecas todos los tests que tienen sin red: PCRE2, Expat, GLib (274 programas), glib-networking, libsoup (38, también los de Apache, con un Apache httpd compilado para Minkar, `6b43706af4`), nghttp2, libpsl, Brotli, HarfBuzz (374, sus ejecutores en Python pasados a sh), libxml2, libpng, Fontconfig, libxkbcommon (1.13.2, `367f638cd6`), Libgcrypt, libgpg-error y libtasn1; lo que cada una deja fuera está en su skip.txt, con la razón. Arreglado por el camino: el mutex ocupado de pthread_cond_wait leído dos veces (`109687faa2`), un descriptor cerrado en g_poll de GLib (`f20105085b`), getsockopt(SO_ACCEPTCONN) (`b717c9c263`), /etc/hosts leído después del DNS (`c99393d4b9`), una carrera en glib-networking 2.80 (ahora 2.90.1, `d416e917f9`). Después (7 nov 2026) pasan cinco más en el PC 2 (`89905161de`): libxslt (746 casos, en la ejecución de la puerta, ahora con 17 bibliotecas), libffi (726 programas de su suite de DejaGnu, compilados y comprobados sin DejaGnu), libjpeg-turbo (211), libepoxy (4; su test de CGL que dibuja necesita OpenGL, que aún no está) y libwebp (sus vectores de prueba, porque no tiene tests unitarios: cada vector decodificado a cinco formatos, con y sin hilos, contra sumas MD5). Falta: FreeType y WOFF2, que no tienen tests que funcionen sin descargas (el único de FreeType necesita una fuente de licencia dudosa); las dos las usan los tests de HarfBuzz y Fontconfig, que pasan.
- [ ] W1 — JavaScriptCore: el shell `jsc` y las suites de JavaScript que no necesitan navegador, con el JIT. *Avance (7 nov 2026):* JavaScriptCore del repositorio de WebKit en el commit de WPE 2.54.2, compilado como su port JSCOnly con el JIT, un framework privado en `/usr/lib/webkit` con `jsc` (`3eba0388e3`); hizo falta CFNotificationCenter (`18735971e8`), `KERN_NOT_FOUND` (`854fb2e231`), `pthread_self_is_exiting_np` (`26838cbada`) y codesign para bundles (`9a6968b48a`). En el PC 2: la suite de ES6 pasa, 605 de 605; la suite stress ejecutó 13.967 de sus 20.174 tests antes de llenarse el disco, y solo fallaron dos (8 ejecuciones), por el `pow` de libm, desviado una ULP entera: ahora es el de Arm (de musl, MIT), con 0,52 ULP como máximo (`600343c267`), y pasan. Falta: el resto de la suite stress y las demás suites, en el PC 1.
- [x] W2 W2 — WebCore y los procesos de WebKit (interfaz, contenido web, red) sobre una plataforma WPE de Minkar: fotogramas en búferes de memoria compartida (más adelante IOSurface) y eventos de entrada. Criterio: una página HTML dibujada en una imagen y comparada con una de referencia. *Hecho (10 nov 2026):* WPE WebKit 2.54.2 compilado con el toolchain (clang 21) en `BSD/lib/wpewebkit` (las fuentes de WebKit en el commit de JavaScriptCore, sus seis parches y once propios: el puerto WPE en Darwin sin los bundles de los puertos Cocoa, IPC por sockets de flujo con cada mensaje enmarcado, porque los sockets Unix de Darwin no tienen `SOCK_SEQPACKET`, y los tipos de contenido de GLib convertidos en tipos MIME); plataforma WPE sin pantalla, Skia sobre el EGL de Mesa, sin GStreamer hasta AU6. Criterio superado: `bmake w2-check` (`Tests/W2`) dibuja `page.html` (bloques, flexbox, grid, bordes, transparencia, una caja añadida por JavaScript y texto) a 800x600 en la VM en 2 s, con 0 píxeles distintos de la referencia hecha con la geometría de la página y 3184 píxeles de tinta en el texto. Los eventos de entrada se prueban con las vistas de W3.
- [x] W3 — `WebKit.framework`, la API moderna: `WKWebView` dibujando en su vista de AppKit, `loadHTMLString:baseURL:`, `loadRequest:`, `evaluateJavaScript:completionHandler:`, `WKWebViewConfiguration`, `WKPreferences`, `WKUserContentController` con `WKUserScript` y manejadores de mensajes de script, `WKNavigationDelegate` y `WKUIDelegate`; ratón, teclado, desplazamiento, foco, copiar y pegar. *Hecho (10 nov 2026, `e5e516dade`):* `WKWebView` es un WebKitWebView sobre la plataforma WPE de Minkar (una pantalla con un toplevel por vista, fotogramas de memoria compartida copiados a la vista a hasta 60 por segundo, el portapapeles en el pasteboard general, los cursores de AppKit); el contexto principal de GLib gira en los run loops del hilo principal; los eventos de ratón, desplazamiento y teclado de AppKit llegan a WebKit (códigos virtuales de tecla a los de entrada de Linux, símbolos a partir de los caracteres de AppKit). `WebKit.framework` se compila en una etapa propia después de BSD. Tests locales `webkit-wpe` 9/0 (una página dibujada derecha por `drawRect:`, los valores y errores de JavaScript, un script de usuario y un mensaje de script, un clic, escritura, copiar y pegar por el pasteboard, la rueda, alert, confirm, prompt y `window.open`/`close`) y `webkit-utt-api` 7/0. Encontrado por el camino: el `NSRunLoop` de Foundation no está construido sobre CFRunLoop aquí (los observadores y fuentes de CF en el run loop principal no se ejecutan bajo AppKit; WebKit tiene su propia fuente de entrada), un hueco de API 19 que queda abierto; `NSNull` se describe como `<null>` (`d1bd8f9cc9`). *Pendiente:* IOSurfaces en vez de copias, métodos de entrada y teclas muertas, arrastrar y soltar, impresión.
- [x] W4 — La API antigua sobre el mismo motor: `WebView`, `WebFrame`, `WebPreferences` y los delegados de carga de marco, de política y de interfaz. *Hecho (10 nov 2026, `86ed0b5d33`):* un `WebView` muestra un `WKWebView` propio y convierte sus llamadas en las de los delegados antiguos: carga de marco (con el objeto window limpiado en cada página), política mediante `WebPolicyDecisionListener`, los paneles síncronos del delegado de interfaz, ventanas nuevas y cierre; `WebScriptObject` pasa valores a través de JavaScript (los objetos se guardan en una tabla de la página); `stringByEvaluatingJavaScriptFromString:`, `searchFor:`, la respuesta y los bytes de la fuente de datos, `WebPreferences` como ajustes de WebKit. Las llamadas síncronas de la API ejecutan GLib anidado cuando se hacen desde dentro de las devoluciones de llamada de WebKit (un script desde `webView:didFinishLoadForFrame:` funciona). El motor de texto ya no existe. Tests locales `webkit-legacy` 7/0, `webkit-utt-api` 7/0, `webkit-wpe` 9/0. *Pendiente:* la API del DOM (`DOMDocument` y el resto), marcos distintos del principal, descargas y objetos de Objective-C expuestos a los scripts.
- [ ] W5 — Las vistas web de las apps de referencia funcionan (sus vistas HTML, scripts de usuario y ajustes). Pruebas locales para cada uno de W0–W5; los tests de API de WebKit donde funcionen.

**Audio y medios (AU1–AU10; decidido el 9 nov 2026).** Sin GTK, Qt ni GStreamer: el audio y el vídeo de WebKit pasan por los frameworks propios de Minkar. Profesional desde el principio, para no rehacer el núcleo después: un camino de tiempo real sin copias extra (los clientes escriben en memoria compartida y el servicio de audio mezcla en el mismo ciclo, en hilos de tiempo real del kernel, con memoria bloqueada y sin bloqueos ni reservas de memoria en el camino del audio); canales, frecuencias de muestreo (de 44,1 a 192 kHz) y formatos sin valores fijos, varios dispositivos a la vez; un reloj por dispositivo (tiempo en muestras, hora del sistema y relación de velocidad) para corregir la deriva y para los dispositivos agregados; búferes de hasta 32 muestras; y un registro de componentes que carga bundles externos. Decodificación con FFmpeg (LGPL 2.1, librería dinámica, sin sus partes GPL ni no libres, recortada a decodificadores y demuxers) y dav1d (BSD-2) para AV1, fijados con su SHA-256 y en un proceso aparte. AU1–AU6 son necesarios para Minkar 19.6.0; AU7–AU10 llegan en 19.6.x.
- [ ] AU1 — Kernel: IOAudioFamily (APSL) y un driver HDA (el manejo de códecs portado de FreeBSD, BSD-2) para el `intel-hda` de QEMU, el PC 1, el Mac Pro y el audio por HDMI. Criterio: una onda senoidal reproducida en QEMU, grabada a WAV y comparada muestra a muestra; después, oída en el PC 1.
- [ ] AU2 — Un servicio de audio (Mach/MIG) dueño de los dispositivos que mezcla, y `CoreAudio.framework`: `CoreAudioTypes.h`, propiedades y avisos de `AudioObject`, `AudioDeviceCreateIOProcID`/`Start`/`Stop`, hora del sistema; volumen en la barra de menús de Atlas. Criterio: dos apps sonando a la vez, pruebas de `AudioObject`, latencia de ida y vuelta y una prueba larga contando cortes.
- [ ] AU3 — AudioToolbox: AudioComponent y la unidad de salida por defecto, AudioQueue, AudioConverter (frecuencias con speexdsp, BSD), AudioFile y ExtAudioFile (WAV, AIFF, CAF), sonidos del sistema que suenan. Criterio: un reproductor de línea de comandos y las pruebas de API 19 de AudioToolbox.
- [ ] AU4 — FFmpeg y dav1d en `BSD/lib`. Criterio: un archivo de prueba de cada formato decodificado con su checksum.
- [ ] AU5 — CoreMedia (`CMTime`, `CMSampleBuffer`, descripciones de formato) y AVFoundation (`AVAsset`, `AVPlayer`, `AVPlayerItem`, `AVPlayerLayer` sobre Core Animation e IOSurface, `AVAudioPlayer`), y el `AVPlayerView` de AVKit. Criterio: un vídeo de referencia reproducido, con los fotogramas comparados y la sincronía de audio y vídeo medida.
- [ ] AU6 — Los medios de WebKit sobre ellos: un reproductor para WPE sobre el AVFoundation de Minkar (`<audio>`, `<video>`) y Web Audio con salida por CoreAudio. Criterio: una página con `<video>` muestra los fotogramas esperados y suena; una prueba de Web Audio.
- [ ] AU7 — Driver USB Audio Class 2.0 (la mayoría de interfaces de estudio). *19.6.x.*
- [ ] AU8 — CoreMIDI, framework y servicio. *19.6.x.*
- [ ] AU9 — Alojar Audio Units, y Accelerate (vDSP; BLAS y LAPACK sobre OpenBLAS, BSD-3). *19.6.x.*
- [ ] AU10 — Dispositivos agregados y plug-ins de dispositivo en espacio de usuario. *19.6.x.* Criterios medidos en la misma máquina física que el sistema de referencia: latencia de ida y vuelta con el búfer más pequeño, cortes en una prueba larga con carga, error y deriva del reloj, uso de CPU del servicio de audio.


## Fase 4 — Gráficos y WindowServer

Objetivo: WindowServer dibuja ventanas de AppKit y recibe teclado y ratón. En la VM basta el camino por software; en los equipos físicos el objetivo es la composición por GPU, porque el requisito de rendimiento lo exige.

**Arquitectura propuesta**

- Una interfaz de backend de pantalla dentro de WindowServer: `GOP` (software) → `virtio-gpu` (VMs) → acelerado. AppKit y Quartz nunca ven el backend.
- Composición con Onyx2D y QuartzCore, igual que hoy; el puerto cambia la salida, no el modelo de dibujo.
- Eventos: HID del kernel → WindowServer → cola de eventos de cada app, con el mismo modelo de `NSEvent`.
- Comunicación app ↔ WindowServer por Mach/XPC, no por sockets ad hoc.

**Decisión de backend (25 oct 2026)**

El propio WindowServer se reescribe en los hitos WS0 a WS6, según lo decidido el 31 oct 2026: ver el [plan de reescritura de WindowServer](WindowServer-Rewrite.es.md). Las decisiones de salida, compositor y entrada de abajo son las que usa ese plan.

- **Salida inicial:** `IOGOPFramebuffer.kext` con `IOFramebufferDisplay.m` y composición por software. Sirve para Minkar 19.6.0 y para la VM; en los equipos físicos es solo el punto de partida.
- **Compositor (cambio del 29 oct 2026):** se diseña para GPU desde el inicio, con Vulkan como backend principal detrás de una interfaz propia. pixman queda para el arranque inicial y para VMs sin GPU.
- **VMs:** driver virtio-gpu 2D como subclase de `IOFramebuffer`; los modos 3D de virtio-gpu (virgl, Venus) son opcionales para probar caminos acelerados.
- **Entrada:** IOHIDFamily, con `ApplePS2Controller` en QEMU y xHCI + HID (`MinkarUSB`) en hardware real. libinput, libevdev y mtdev ya no se compilan.
- **Teclado:** libxkbcommon como solución temporal; a largo plazo `UCKeyTranslate` y Text Input Sources (Fase 5).
- **GPU (cambio del 29 oct 2026):** deja de estar diferida. Es un frente paralelo desde ya: driver del kernel para una familia, Mesa Vulkan/OpenGL, compositor en GPU y, a largo plazo, Metal nativo sobre el driver. La primera familia es una decisión abierta (ver Requisitos de rendimiento).

**Actividades**

- [x] Backend de salida sobre IOFramebuffer: WindowServer abre `IOGOPFramebuffer` y dibuja en él (31 oct 2026, `f10f778c78`: la profundidad es un índice; prueba `windowserver` 6/0).
- [x] WindowServer arranca y dibuja en QEMU (WS0, 31 oct 2026): su job de launchd se instala desactivado y, activado, lo arranca y lo reinicia si muere (`f56c70bb88`, `9f46a761ac`); una ventana de AppKit de demostración se dibuja y recibe un clic y una tecla como `NSEvent` (`6888034ad2`, `70b62fe8bc`, `0e3d06ec35`, `dc931eeeb0`). El gate `M4-draw` (`cce7da92bc`) pasa con 0 de 786432 píxeles distintos de la referencia.
- [x] Esqueleto e interfaz del núcleo nuevo (WS1, 31 oct 2026): `WindowServer.defs` (MIG), el cliente en `SkyLight.framework` y el servidor nuevo en sus `Resources`, junto al actual en `CoreServices/WindowServer/Legacy` (`9e48d1f62a`, `f9028e286a`). Las consultas de pantalla, los modos y configuraciones, las tablas de transferencia, la captura, el cursor, los fundidos y la lista de ventanas se sirven por MIG, y la salida de un cliente libera lo que tenía (`0e4d7293ff`, `8dab4060cc`, `37996817e4`, `41e37b7441`, `030dba37c7`). `skylight-test` 115/0.
- [x] Ventanas, superficies y compositor por software (WS2, 1 nov 2026): superficies como entradas de memoria Mach con dos buffers y daño, un compositor pixman por pantalla guiado por el daño y la frecuencia de refresco, el cursor dibujado al final, `wsctl` (`6ca5e31aae`, `1aa140cdab`); ventanas de AppKit dibujadas a través de SkyLight (`b957c09fac`). El gate `M4-compose` (`8512fe60ba`) pasa con 0 de 786432 píxeles distintos, tres veces.
- [x] Entrada (WS3, 1 nov 2026): el servidor nuevo lee IOHIDSystem y reparte a las ventanas, con repetición de teclas, número de clics, captura al arrastrar, la ventana clave y movimientos por la barra de título hechos por el servidor (`c3763a06a8`, `03bb7ed773`); `CGEventTapCreate`, `CGEventPost` y `CGGetEventTapList` (`fa7aae5e38`); AppKit a través de él (`13f99802d5`, `d0c63753c2`). El gate `M4-compose` con un clic, una tecla y un arrastre (`98237bf14e`); `eventtap-test` 15/0.
- [x] El comportamiento del servidor del núcleo nuevo (WS4a, 1 nov 2026; `996178d949` a `e31dd0ef53`): su propio usuario (`_windowserver`), regiones de arrastre, el mapa de teclado en SkyLight, atajos simbólicos, apps en ejecución por la sesión de LaunchServices, la app del frente, el sitio de la barra de menús, la captura del puntero y los elementos de estado (lo que necesita MB0), ventanas minimizadas e imágenes de ventanas (lo que necesita el Dock), listas de ventanas, ventanas hijas y hojas, cursores y rectángulos de cursor, sombras, captura de pantallas, el estado de `CGEventSource`, tablas de transferencia y fundidos en pantalla, y la entrada en su propia cola sobre una instantánea del modelo. Gate `M4-compose` (40 marcadores); pruebas locales del servidor (117 pasadas); no queda ningún `SHORTCUT(WS4a)`.
- [x] El cambio (WS4b, 2 nov 2026; `6c3b719a32` a `9d510ec6c2`): cada app dibuja sus propios menús (MB0: su barra de menús en su propia ventana, el seguimiento completo, los atajos de teclado y Control-F2, los menús contextuales y los botones emergentes, los elementos de estado como ventanas de su app, el menú del sistema); el `loginwindow` mínimo es dueño de la sesión (Cerrar sesión, Reiniciar, Apagar); el servidor nuevo es el de por defecto y arranca con el sistema (`-textconsole` mantiene la consola de texto); CoreGraphics y AppKit solo hablan con él (Quartz Display Services a través de SkyLight); el servidor anterior, libinput, libevdev y mtdev ya no existen. Gate `M4` (54 marcadores, con M4-draw y M4-compose dentro); suite local completa: 3192 pasadas; no queda ningún `SHORTCUT(WS4b)`.
- [x] Validación de entitlements en el kernel (ADR 0001, 1 nov 2026; `57c6d5734b`, `48ee7a4288`, `24412d4182`, `c1cda1f1ae`, `f77748bbb7`, `8366b8d0f6`): la construcción de la imagen firma los ejecutables del sysroot y escribe una trust cache estática; el kext de política `MinkarCodeIntegrity` marca las firmas `CS_SIGNED`, y como binarios de plataforma las de la caché, y mata en el exec a un binario de fuera que lleve un entitlement restringido. Root ya no abre la conexión de servidor de IOHIDSystem sin el entitlement. Lo comprueba el gate M2 (17 marcadores); prueba local `codesign` 13/0; suite local completa de imagen: 2773 pasadas.
- [x] Entrega de eventos de IOHIDSystem al espacio de usuario (31 oct 2026, `e2df542919`): la conexión del servidor de eventos mapea la cola compartida `EvGlobals` y recibe avisos por su puerto Mach; los dispositivos de puntero también llegan. Prueba `hid-events` 9/0. El bucle de despacho de `NEW_HID` sigue desactivado: alimenta las colas del sistema de eventos, que no se compilan.
- [x] Lector de entrada IOHIDSystem en WindowServer (`kIOHIDServerConnectType`, `EvGlobals`, puerto Mach), 31 oct 2026, `ab71ce75a4`: los movimientos, clics y teclas inyectados por QMP le llegan (prueba `windowserver` 6/0).
  - [x] Teclado PS/2: sus teclas llegan a WindowServer y a una ventana de AppKit (el gate `M4-draw` escribe con él, 31 oct 2026).
  - [x] Driver `ApplePS2Mouse` sobre `ApplePS2MouseDevice` (31 oct 2026, `2aa9a1da6e`): movimiento, tres botones y rueda (IntelliMouse); prueba `ps2-mouse` 12/0.
  - [x] Headers `IOKit/hidsystem` en el SDK.
  - [x] xHCI + HID (`MinkarUSB`), probado en QEMU y en el PC 1.
- [x] Retirar `libinput`, `libevdev` y `mtdev` del árbol (con el servidor anterior, WS4b, `c192d0dc42`).
- [x] virtio-gpu 2D como subclase de `IOFramebuffer` (`MinkarVirtioGPU`, 2 nov 2026, `3e73fce129`, `1f58b3fb44`).
- [x] `pbcopy` y `pbpaste` sobre el servidor de pasteboard, y `screencapture` sobre la captura de WindowServer (movidos desde la Fase 2; 3 nov 2026): el servidor de pasteboard `pboard` (`dd06e82755`), el `NSPasteboard` de AppKit sobre él con la API de 10.6 en adelante (`fec8a72732`), `pbcopy` y `pbpaste` (`a5ced9833e`), `screencapture` a PNG, TIFF o el pasteboard (`c536c7dbe9`). Para más adelante: los datos prometidos entre procesos, la selección interactiva, el cursor y JPEG en `screencapture`.
- [x] Varias pantallas y cambio de resolución (3 nov 2026; probado en QEMU con dos pantallas, sin segundo monitor en los equipos de referencia): los cambios de modo llegan a las apps (WS5, `display-reconfigure`); un solo escritorio sobre las pantallas, primero la de arranque por su posición PCI (`e5720024f7`); el puntero se mueve sobre todas y nunca fuera de ellas, porque IOHIDSystem recibe las pantallas del servidor de ventanas (`e5720024f7`); cada pantalla se captura por separado (`93f2e9249a`); disposición y duplicación, y una pantalla que deja de duplicar vuelve junto a las demás (`9b6a9a3a2d`). Prueba local `multi-display` 19/0. Queda: marcar como pantalla de arranque el dispositivo que contiene el framebuffer del firmware, para que una GPU de arranque que no sea la primera del bus siga siendo la principal.
- [x] ADR de GPU: familia, licencias (solo partes MIT/BSD de drm-kmod), capa tipo LinuxKPI para XNU y prototipo KMS en el equipo de referencia (ADR 0002 y G1, 2 nov 2026, `a42a1800b1`, `050acdb1ee`).
- [ ] Gate de rendimiento de composición en el PC de referencia (requisito de Minkar 21.0).

**Criterio de salida (M4):** en QEMU y en un PC de referencia, varias apps de AppKit abren ventanas, se arrastran, reciben teclado y redibujan sin artefactos.

**Estado (3 nov 2026): M4 cumplido.** En QEMU, `boottest GATE=M4` ve sus 54 marcadores con la VGA estándar y con la virtio. En el PC 2, con el árbol de hoy (`c6d74fc756`) a 3440x1440 y 60 Hz, compuesto por el backend de Vulkan: las dos ventanas de AppKit de la demo recibieron clics y teclas del teclado y el ratón USB (a través de un KVM) y se arrastraron por su barra de título, sin parpadeos ni restos a la vista del mantenedor ni en la captura de la pantalla (frames p95 17,3 ms); una segunda app se abrió, pasó al frente y se cerró, y la primera lo vio por NSWorkspace. Queda para Minkar 21.0: la puerta de rendimiento de composición.

## Pila gráfica: componentes y secuencia

Del entorno gráfico ya existen la base de dibujo (Core Graphics con FreeType, y pixman, libpng, libjpeg y libtiff en `BSD/lib`), la consola GOP y la entrada USB. Faltan el servidor de composición, IOSurface y todo lo que depende de la GPU. Estado verificado en `devel` (`cce7da92bc`, 31 oct 2026): WS0 está hecho, el WindowServer actual dibuja ventanas de AppKit con entrada (gate `M4-draw`); WS1 también (`030dba37c7`): la interfaz y el servidor del núcleo nuevo, probados con `skylight-test`; WS2 (`8512fe60ba`): compone ventanas de AppKit (gate `M4-compose`); y WS3 (`98237bf14e`): entrada y event taps.

Componentes, por estado:

- **Existen:** Core Graphics con FreeType; pixman, libpng, libjpeg y libtiff (`BSD/lib`); la consola GOP (`IOGOPFramebuffer`); la entrada USB (xHCI + HID, `MinkarUSB`); los headers `IOKit/hidsystem`; la entrega de eventos de IOHIDSystem; `ApplePS2Mouse` y el teclado PS/2.
- **Parciales:** WindowServer (WS0 a WS5 hechos: el núcleo nuevo es el servidor de ventanas, con IOSurface y virtio-gpu, en QEMU; WS6 por hacer); QuartzCore y Onyx2D; AppKit (Cocotron; con los arreglos que necesitó una primera ventana, 31 oct 2026).
- **Faltan:** el compositor de Core Animation; Core Text sobre HarfBuzz/FreeType; giflib y libwebp; ColorSync sobre Little CMS; y el frente de GPU (driver del kernel, Mesa, Metal).

El detalle técnico de cada pieza está en la guía técnica de la pila gráfica.

Secuencia de desarrollo:

1. Entrega de eventos de IOHIDSystem y ratón PS/2.
2. WindowServer sobre el framebuffer GOP.
3. virtio-gpu 2D (driver propio, BSD-2; hecho el 2 nov 2026).
4. IOSurface (memoria compartida Mach; hecho el 2 nov 2026).
5. Compositor de Core Animation, diseñado para GPU (backend Vulkan; pixman solo para el arranque y las VMs).
6. HarfBuzz/FreeType para Core Text, giflib/libwebp, Little CMS para ColorSync.
7. AppKit moderno con vistas respaldadas por capas.

En paralelo, el frente de GPU G1–G5: desde la familia del primer driver y el prototipo KMS hasta Mesa (lavapipe/llvmpipe en VMs) y Metal (darling-metal e Indium, MPL-2.0).

El paso 1 se completó el 31 oct 2026 (`e2df542919`, `2aa9a1da6e`), y también los primeros hitos del paso 2, WS0 (gate `M4-draw`, `cce7da92bc`) WS1 (la interfaz del núcleo nuevo, `030dba37c7`) WS2 (su compositor, gate `M4-compose`, `8512fe60ba`) y WS3 (entrada y event taps, `98237bf14e`); la comprobación de entitlements del ADR 0001 está hecha (`8366b8d0f6`), también WS4a (el comportamiento del servidor, 1 nov 2026, `e31dd0ef53`), y también WS4b (el cambio, gate `M4`, 2 nov 2026, `9d510ec6c2`); y también WS5 (2 nov 2026, `9a472063c1`): los pasos 3 y 4 están hechos (virtio-gpu 2D, `3e73fce129` y `1f58b3fb44`; IOSurface, `b28eb5f413`). Sigue WS6. G1 está hecho (2 nov 2026, ADR 0002, `a42a1800b1` y `050acdb1ee`): en el PC 2, `MinkarAMDFramebuffer` toma la pantalla de la FirePro desde el firmware en la resolución nativa del monitor (3440x1440 a 50 Hz), muestra su propio framebuffer en VRAM, y `wsctl mode` cambia el modo con el escalador, con las tablas de la VBIOS ejecutadas por el intérprete AtomBIOS. Los modos con tiempos propios (reloj de píxel y enlace DisplayPort fijados por el driver) llegan con G2. G2 está en marcha y su gate se cumple en el PC 2 (2 nov 2026): el motor DMA ejecuta un búfer de comandos y su fence se señala, y copia entre superficies en VRAM (`6be0dcb84c`, `b9ac2e1f36`); un gestor de VRAM (`dfee0c87f3`), un user client con `minkar-gputest` (`d405ae35bf`), y la GART con búferes de sistema en las dos GPU, también la del monitor (`34f7974d13`, `2b11571625`). Desde entonces (2 nov 2026): las IOSurfaces llegan a la GPU por la GART (`e92fe4b7ff`), interrupciones con esperas de fence que duermen (`7e6621a639`), el servidor de ventanas presenta sus fotogramas con el motor DMA de la GPU (`38e8875af1`), IOSurfaces en VRAM compartidas entre procesos (`f13053f237`); y los modos con tiempos propios por HDMI: el EDID por DDC (`4dcbddbf47`, `de0233a02d`), el modo fijado con las tablas de la VBIOS (`8677a3bb9b`), la resolución nativa a su frecuencia más alta al arrancar, 60 Hz en el PC 2 (`59e0165082`), cada frecuencia como un modo propio (`e14a3fc4b6`, `e86385a4f3`), y la pantalla vuelve sola tras desconectar y conectar su cable (`6999c389ad`). Queda de G2: el anillo GFX (el procesador de comandos, con el microcódigo de AMD); los relojes de la GPU (aplazados por el mantenedor: se quedan los del arranque); y el entrenamiento del enlace DisplayPort, a la espera de una pantalla DisplayPort para probarlo. Desde entonces G2 también da a cada programa su propio espacio de direcciones en la GPU (contextos de memoria virtual 1 a 15, `1910d34651`, `78d6f046a3`): cualquier programa puede usar la GPU en él, y lo que va más allá queda para administradores o el servidor de ventanas (`84a8e4139b`); un búfer de comandos que nunca termina reinicia el procesador de comandos. G3 ha empezado (3 nov 2026): el inicio del bloque gráfico (tiling, backends de render, unidades de cómputo, `ee1c6210c9`) y el RLC con el estado inicial (`76e14432ee`), en las dos D500; y RADV de Mesa ya funciona sobre él (3 nov 2026): nuestra libdrm lleva sus contextos, envíos, mapeos de GPU y objetos de sincronización al user client (`3ef2fd2de9`), y Mesa 26.2.6 con cuatro parches propios (`8d911e7e1e`) encuentra las dos GPU como dispositivos Vulkan 1.3 y ejecuta en ellas escrituras de buffers, rellenos y copias por DMA y shaders de cómputo (`minkar-vktest`, 17 de 17). Desde entonces Mesa se compila en el árbol con sus herramientas fijadas (`f31988bb0b`), y RADV dibuja: un triángulo con un pipeline gráfico y shaders SPIR-V, fuera de pantalla, comprobado píxel a píxel (`minkar-vkdraw`, `7b62c1005e`). Y presenta en ventanas (3 nov 2026): `VK_EXT_metal_surface` sobre un `CAMetalLayer` propio cuyos drawables son IOSurfaces, como presentan las apps Vulkan en el sistema de referencia (`66db104abc`, `197aff60d2`, `80f271c9b8`, `1be99a691e`); el triángulo se vio en su ventana en la pantalla del PC 2. Esa presentación aún copia por la CPU y no tiene sincronía vertical: los drawables en VRAM mostrados por el compositor de GPU sin copias, al ritmo de la pantalla, son de WS6. G3 está hecho (3 nov 2026): su gate se cumple con el triángulo presentado en una ventana y las pruebas de conformidad de Khronos (CTS 1.4.7.1, a través del loader de Vulkan de Khronos, `18629db65a`, `b9eacfef5d`) pasando un subconjunto de 1.992 casos de la lista mustpass en el PC 2 sin ningún fallo (1.887 pasan, 105 no soportados); por el camino encontraron lo que les faltaba al driver y a libdrm: buffers por páginas, VRAM fuera de la ventana de la CPU, sitio para miles de buffers y mapeos, plazos de las fences y sync files (`f96d6be9cb`, `9981d713a5`). OpenGL viene después. G4 ha empezado (3 nov 2026): el compositor dibuja a través de una interfaz de backend (`4979a11f03`), y `WSVulkanBackend` compone con un compute shader en un frame en VRAM compartido con la GPU de la pantalla, presentado por su motor DMA; en el PC 2 sus frames coinciden bit a bit con los de pixman (G4.2, `c32fe22c13`). Las IOSurfaces que una app crea en VRAM se componen en su sitio, sin copias (G4.3, `7d973f778a`). Y los frames se muestran con cambios de página en el retrazado vertical de la pantalla, uno por refresco, sin cortes (G4.4, `f103270509`, `1601b4934e`). Los drawables de CAMetalLayer son IOSurfaces en VRAM que se muestran como capa de su ventana y se componen donde están, y las apps de Vulkan presentan al ritmo de la pantalla; la CPU ya no copia frames (G4.5, `9e74fdc214`, `58ab118eed`, `0822411404`). Queda una copia, de la GPU dentro de la VRAM, de la imagen del swapchain al drawable, como en la ruta de Metal de Mesa: 2,65 ms por frame a 3440x1440 medidos en G4.6 con la GPU a sus relojes de arranque (unos 0,3 ms esperados con sus relojes completos), que se mantiene por ahora; quitarla queda pendiente para la puerta de rendimiento de Minkar 21.0. G4 está hecho (3 nov 2026): G4.6 midió el servidor de ventanas con los dos backends (wsctl stats, `bfb9e26e4d`; minkar-vkcopy, `e487b84770`; las cifras en [Measurements.es.md](Measurements.es.md)): las apps de Vulkan se componen 14 veces más barato, con una décima parte de la CPU del servidor y al ritmo de la pantalla; los cambios de página cuestan un refresco de latencia (28 ms del ratón a la pantalla). La GPU aún funciona con los relojes que deja el firmware (la memoria a 150 MHz de 1.270), así que estas cifras son una línea base. El ratón y el teclado USB del PC 2 funcionan, a través de un KVM, desde que el driver xHCI codifica los intervalos de interrupción según la velocidad del dispositivo (`8f22e24b9a`). Sigue en el frente de GPU: los relojes de la GPU (si_dpm y su SMC), y después las mediciones otra vez; luego G5. Las flechas discontinuas marcan lo que el frente de GPU reutiliza de los pasos 4 y 5.

## Fase 5 — Escritorio Atlas

Objetivo: una sesión gráfica completa con el lenguaje visual Atlas, con login, barra de menús global, Dock, escritorio con iconos y preferencias. El orden sigue lo que el usuario ve al arrancar. Plan del escritorio y sus apps: [Atlas.es.md](Atlas.es.md).

1. **LoginWindow:** autenticación contra el servicio de usuarios de esta fase (Directory Services, más abajo) y arranque de la sesión (`LaunchAgents` del usuario).
2. **Barra de menús y SystemUIServer:** el AppKit de cada app dibuja su propia barra de menús y sus menús; los status items son ventanas de sus apps; SystemUIServer aloja los Menu Extras. Plan: [Atlas.es.md](Atlas.es.md), capítulo 3 (MB0 en WS4b, MB1 a MB4 aquí).
3. **Menu Extras básicos:** volumen, pantallas, red, reloj, notificaciones. Cada uno como bundle cargable, no compilado dentro de SystemUIServer. Plan: [Atlas.es.md](Atlas.es.md), capítulo 3 (MB1).
4. **Dock:** lanzador, apps abiertas, minimizado y gestión del fondo de pantalla, sobre LaunchServices. Plan: [Atlas.es.md](Atlas.es.md), capítulo 4 (D0–D4).
5. **Files:** app nueva en Cocoa puro. Navegación con LaunchServices, montaje vía DiskArbitration, iconos de escritorio, papelera y "Abrir con". Plan: [Atlas.es.md](Atlas.es.md), capítulo 4 (K0–K4).
6. **System Preferences:** app contenedora + cargador de bundles `.prefPane` (`NSPreferencePane`). Paneles iniciales: Pantallas, Teclado, Usuarios, Red, Dock, Fecha y hora. *Parcial (4 nov 2026):* la app, `PreferencePanes.framework` y cinco paneles, Pantallas, Dock, Fecha y hora, Teclado y Red (`948e15445f`, `f5f26fc193`, `7c5f6cda8b`; prueba local `system-preferences` 12/0); Usuarios espera a Directory Services.
7. **AppKit:** completar controles faltantes y dar a todo el sistema el lenguaje visual Atlas (diseño propio, sin recursos de terceros), priorizado por lo que usan Files y System Preferences. *4 nov 2026:* un panel de guardar (`1e4869b895`); la fuente del sistema, Inter, es también la del editor de texto hasta elegir una de ancho fijo (por revisar). *5 nov 2026:* la fuente de ancho fijo es Agave (SIL OFL 1.1), descargada en una versión fijada e instalada sin cambios; `userFixedPitchFont` sigue a `NSFixedPitchFont` y Text Editor muestra el texto plano con ella; las fuentes cuya familia lleva espacios se encuentran por su nombre PostScript (`642e6a1cda`), y los caracteres que cubre una fuente perdían el último de cada tramo, que el texto dibujaba entonces con otra fuente (`5e51bdc6ba`). Files tiene un icono de prueba, no el definitivo (`2194ff5695`); las imágenes dibujadas a menos de la mitad de su tamaño se promedian en lugar de muestrearse (`f474cc75a0`). Prueba local `fixed-pitch` 14/0.

Criterio transversal: todos estos componentes son apps o bundles Cocoa normales. Así funcionan como prueba continua de AppKit.

Comandos que llegan con esta fase (movidos desde la Fase 2):

- [ ] `open` y `lsappinfo`, sobre LaunchServices. *Parcial (4 nov 2026):* `open` con `-a`, `-b`, `-e`, `-t`, `-R` y `-n` (`906f12eae4`); falta `lsappinfo`. Con él llegó el editor de texto, Text Editor (`b8b8906ab9`).
- [ ] `mdfind`, `mdls` y `mdutil`, con un índice de metadatos para la búsqueda de Files.
- [ ] Gestión de usuarios estilo Directory Services, para LoginWindow y el panel Usuarios (movida de la Fase 2 el 31 oct).
- [ ] `say`, con un driver de audio y síntesis de voz.

**Criterio de salida (M5):** desde el arranque se llega a una sesión gráfica, se abre una app desde el Dock, se navega con Files y se cambia la resolución desde Preferencias, con el lenguaje visual de Atlas que necesita K1 ([Atlas.es.md](Atlas.es.md), capítulo 4).

## Fase 6 — Aplicaciones y distribución

Objetivo: instalar software arrastrando la app desde una imagen de disco a `/Applications` y tener las apps básicas de trabajo. Esta fase es el primer uso real del sistema y la mejor fuente de bugs.

**DMG**

1. `DiskImages.framework` propio (MIT/BSD): lectura del formato UDIF con sus compresiones habituales, luego escritura.
2. Montaje vía DiskArbitration en `/Volumes/<etiqueta>`, usando el driver HFS+ de la Fase 1.
3. `hdiutil` mínimo (`attach`, `detach`, `create`) encima del framework, y doble clic en Files.
4. Compresión HFS+ (decmpfs) en el kext de hfs: la plataforma de referencia guarda comprimidos muchos archivos de las apps y del sistema, y el hfs de Minkar (el hfs-556.60.1 de Darwin 20, adaptado al xnu de Darwin 19) se compila sin ella, así que esos archivos se leen vacíos. Criterio: una app copiada desde un DMG de la plataforma de referencia se ejecuta.
5. El resto de la adaptación de hfs (encontrado el 10 nov 2026, ver `Kernel/_PROVENANCE`): encontrar y corregir el bucle del que `hfs_vfsutils.c` sale detectando duplicados (un apaño, según su propio comentario), y que `panic_on_assert` vuelva a salir de la configuración del kernel en vez de estar siempre activo.

**Aplicaciones**

Las apps de uso diario se planifican en [Atlas.es.md](Atlas.es.md), capítulo 5: una base común (A0), y después Terminal, Text Editor (Editor de texto), System Monitor (Monitor del sistema), Logs (Registros), Screenshot (Captura de pantalla), Image Viewer y Document Viewer (visores de imágenes y de documentos), Disks (Discos), System Information (Información del sistema), Calculator (Calculadora) y Archive Manager (Gestor de archivadores) (A1–A10), obligatorias para Minkar 19.6.0; Fonts, Passwords and Keys, Color Picker, Script Editor, Help y Characters en 19.6.x; el navegador (Ladybird) y un IDE a largo plazo.

Riesgo a señalar: muchas apps Cocoa actuales usan SwiftUI, que no figura en el roadmap. Conviene registrarlo como decisión abierta en lugar de descubrirlo tarde.

Comandos que llegan con esta fase (movidos desde la Fase 2):

- [ ] `osascript`, con una Open Scripting Architecture para las apps.
- [ ] `opendiff`, con una app de comparación de archivos.
- [ ] `tmutil`, con copias de seguridad.
- [ ] Los comandos de git en Perl, Python y Tcl (`git send-email`, `gitk`…), con esos lenguajes (movidos de la Fase 2 el 31 oct).

Adelanto: `hdiutil attach` ya monta imágenes en `/Volumes` a través de DiskArbitration (30 oct 2026); falta leer el formato UDIF de los DMG.

**Criterio de salida (M6):** se descarga un DMG, se monta, se copia la app a `/Applications` y corre; y el escritorio Atlas está completo para el uso diario, tal como lo fija la sección 6 de [Atlas.es.md](Atlas.es.md): MB2 y MB3, D2 y D3, K2 y K3, y las apps esenciales A0–A10, usadas a diario en la propia máquina de desarrollo; WebKit hasta W5, con las vistas web de las apps de referencia funcionando; y audio y medios hasta AU6, con sonido en el sistema y `<video>` en WebKit. MB4, D4, K4, las demás apps y el audio profesional (AU7–AU10) llegan en 19.6.x; el navegador web no hace falta.

## Fase 7 — Compatibilidad binaria y arm64

Objetivo posterior a Minkar 19.6.0: ejecutar binarios x86-64 compilados para la plataforma de referencia sin recompilar, y portar el sistema a arm64. No bloquea la ruta crítica, pero hay decisiones que deben tomarse desde ya para no cerrarla.

**Decisiones que protegen esta fase desde hoy**

- Nombres de instalación de dylibs y frameworks idénticos a los originales (`/System/Library/Frameworks/...`). *Hecho el 25 oct* para todos los frameworks (AppKit en `Versions/C`), `libobjc.A.dylib` y `libbz2.1.0.dylib`, con las versiones de compatibilidad originales, comprobado con dyld en la imagen; `libicucore` exporta los nombres simples de ICU (`u_strlen`).
- Versiones de compatibilidad y actual en cada dylib alineadas con la API cerrada (API 19 → 20 → 21).
- No cambiar la disposición de ivars públicos ni tamaños de structs expuestos en headers.
- `ProductVersion` y `uname -r` se documentan juntos; la matriz de compatibilidad registra qué apps fallan por deducir la versión de la plataforma desde Darwin.

**Actividades (después de M6)**

1. Cargar en dyld binarios compilados para la plataforma de referencia: firmas de código ignoradas o verificadas sin la cadena de certificados original, hardened runtime tolerado.
2. Matriz de compatibilidad con apps x86-64 reales: arranca, funciona, falla y por qué.
3. arm64: bootloader, plataforma XNU arm64 y toolchain cruzado; empezar en QEMU `virt`.

**Criterio de salida (M7):** un conjunto de apps x86-64 descargadas de terceros corre sin recompilar; la ISO arm64 arranca hasta M2 en QEMU.

## Hitos, dependencias y ruta crítica

Ruta crítica: Fase 0 (M0) → Fase 1 (M1) → Fase 2 (M2) → Fase 3 (M3) → Fase 4 (M4) → Fase 5 (M5) → Fase 6 (M6, Minkar 19.6.0); la Fase 7 (M7) y el rendimiento de Minkar 21.0 van aparte.

M0, M1 y M2 están cumplidos (M1 el 1 nov, con el PC 2) y la Fase 2 está cerrada (31 oct). El trabajo activo está en las Fases 3 y 4, que deben cerrar antes de Atlas. El frente de GPU arranca en paralelo y alimenta el requisito de rendimiento de Minkar 21.0: igual o mejor que el sistema de referencia en el mismo equipo. La Fase 7 sigue después de Minkar 19.6.0.

- [x] M0 — Build, toolchain, CI
- [x] M1 — Arranque Darwin
- [x] M2 — Userland base
- [x] M3 — Frameworks y Swift (cumplido el 5 nov 2026: gate M3)
- [ ] M4 — Gráficos y WindowServer (WS0 a WS3 hechos)
- [ ] M5 — Escritorio Atlas
- [ ] M6 — Apps y distribución (Minkar 19.6.0)
- [ ] Minkar 21.0 — API 21 completa y rendimiento
- [ ] M7 — Compatibilidad binaria, arm64

| Gate | Fase | Estado al 31 oct 2026 |
| --- | --- | --- |
| M0 | Build, toolchain, CI | Cumplido (CI verde desde `05fa75cb65`) |
| M1 | Arranque Darwin | Cumplido en QEMU, en el PC 1 (SATA, NVMe, SMP) y en el PC 2 (NVMe, 24 hilos; 1 nov 2026) |
| M2 | Userland base | Cumplido (`boottest GATE=M2`, 17 marcadores desde el 1 nov 2026, con la comprobación de integridad de código); Fase 2 cerrada el 31 oct |
| M3 | Frameworks y Swift | Cumplido (5 nov 2026): toolchain, runtime, overlays de Swift, Combine y XCTest; el gate M3 pasa con las siete herramientas (swift-argument-parser; Rainbow y Files; Yams; xcbeautify; mogenerator; tag; SwiftFormat) |
| M4 | Gráficos y WindowServer | Cumplido en QEMU (`boottest GATE=M4`, 54 marcadores, 2 nov 2026, `9d510ec6c2`): WS0 a WS4b hechos (el núcleo nuevo es el servidor de ventanas, los menús en cada app con MB0, el `loginwindow` mínimo; el servidor anterior borrado). También WS5 (IOSurface y virtio-gpu, 2 nov 2026, `9a472063c1`; M4 pasa con virtio-vga y con la VGA estándar). La Fase 4 sigue con la GPU (WS6) y en hardware físico |
| M5 | Escritorio Atlas | Sin empezar |
| M6 (Minkar 19.6.0) | Apps y distribución, el escritorio completo para el uso diario (MB2–MB3, D2–D3, K2–K3, apps A0–A10), WebKit (W0–W5) y audio y medios (AU1–AU6) | Sin empezar; `hdiutil attach` ya monta |
| Minkar 21.0 | API 21 completa y rendimiento ≥ sistema de referencia en equipo físico | Sin empezar; depende del frente de GPU |
| M7 | Compatibilidad binaria, arm64 | Después de Minkar 19.6.0 |

## Riesgos y mitigaciones

El riesgo dominante es de personas, no técnico: un equipo pequeño con muchas capas abiertas a la vez. La mitigación general es cerrar gates en orden y limitar el trabajo en curso.

| Riesgo | Impacto | Mitigación |
| --- | --- | --- |
| Pocos desarrolladores con experiencia en XNU y kexts | Fase 1 se alarga y bloquea todo | Documentar el arranque paso a paso; priorizar solo hardware de QEMU hasta M2 |
| CF-Lite pública anterior a la API 19 | Faltan APIs de CF para las API 20 y 21 | Añadirlas a mano con la CF de swift-corelibs como referencia; pruebas de bridging en cada cambio |
| Hardware real heterogéneo | Bugs imposibles de reproducir | Lista corta de hardware de referencia; lo demás, "best effort" |
| Drivers de GPU: el bloque más largo, y ahora requisito | Minkar 21.0 no iguala al sistema de referencia | Frente paralelo desde ya; una sola familia de GPU; prototipo KMS antes de comprometerse; compositor diseñado para GPU |
| SwiftUI fuera del roadmap | Muchas apps modernas no compilan | Decidido: las API 19–21 cierran sin SwiftUI; OpenSwiftUI en observación; medir cuántas apps de referencia lo usan |
| Licencias y sala limpia | Código que hay que retirar | Revisión de licencia en cada importación de terceros; declaración de sala limpia en cada PR |
| Trabajo disperso en muchas capas | Nada llega a usable | Tablero público por fase; máximo dos fases activas al mismo tiempo |
| Documentación desfasada (wiki archivado) | Contribuidores trabajan sobre información vieja | Mover roadmap y estado a `Docs/` dentro del repo, actualizados en cada hito |

## Próximos pasos

En el mismo orden que los próximos pasos de `Docs/ROADMAP.md` (ruta crítica):

- [x] **PC 1 (Fase 1):** NVMe (arranca desde el SSD) y SMP (corren los ocho hilos), el 29 y 30 oct; botón de encendido y apagado ACPI S5, el 31 oct.
- [ ] **PC 1 (Fase 1):** arranque sin `pci_keep_console`, IPv6 en la red (RDNSS, DNS sobre IPv6) y reposo ACPI S3; el equipo con la RX 6600 como otro equipo de pruebas. (El segundo PC del gate M1 es el PC 2, 1 nov 2026.)
- [x] **Userland (Fase 2):** cerrada el 31 oct (exFAT en lectura y escritura, `launchctl kickstart`, rutas de dyld, notifyd e IPv6 probados; el resto pasó a las Fases 3, 5 y 6).
- [x] **Fase 4:** entrega de eventos de IOHIDSystem en el kernel (`e2df542919`), el driver `ApplePS2Mouse` (`2aa9a1da6e`) y el lector de entrada de WindowServer (`ab71ce75a4`), 31 oct 2026; pruebas `hid-events` 9/0, `ps2-mouse` 12/0, `windowserver` 6/0; pasan las pruebas de consola y M2 (suite completa con 2991 pruebas superadas después de (a)).
- [x] **Fase 4:** instalar el job de WindowServer y lograr el primer arranque que dibuje en `IOGOPFramebuffer` (WS0, 31 oct 2026): el job se instala desactivado (`f56c70bb88`); activado, WindowServer dibuja una ventana de AppKit de demostración que recibe un clic y una tecla; gate `M4-draw` (`cce7da92bc`).
- [ ] **Fase 3:** las clases de Foundation que más faltan según el informe de símbolos (`NSURLSession` hecha el 31 oct, `00617d5f6c`, con cookies; quedan su caché y métricas; `NSXPCConnection`, `NSXPCListener` y `NSXPCInterface` hechas el 31 oct, `82c5f3e8f8`, prueba `foundation-xpc-connection` 20/0, suite completa 2977/0, M2 PASS; servicios XPC dentro de las apps: la app los lanza bajo demanda como hijos suyos, sin launchd, prueba `foundation-xpc-service` 9/0), y lo que llegó de la Fase 2 (libSystem, stubs ruidosos, `logd`).
- [ ] **Fase 3:** los prototipos del toolchain Swift.

Además, lo que este plan añade y aún no está en el repo:

- [x] **Versiones (31 oct):** `PROD_VERSION = 19.6.0` para `ProductUserVisibleVersion`, `PROD_COMPAT_VERSION = 10.15` para `ProductVersion`, y `sw_vers` mostrando «Minkar 19.6.0 (kernel Darwin 19.6.0)», con su manual (`d2b559759f`). Probado en QEMU: `@available(10.15)` verdadero y `(11.0)` falso, `operatingSystemVersion` 10.15.0, `uname -r` 19.6.0. `libsystem_info` y el kext de pthread conservan sus propias versiones. En el mismo trabajo: `launch_activate_socket` (`5051c781b4`), `os_log` y `os_activity` (`0709544df7`), `_availability_version_check` (`c0923c1bc7`) y `NSProcessInfo` (`fb6e25b877`).
- [x] **`Docs/ROADMAP.md` al día (31 oct):** reescrito desde este plan, con Atlas y Files, el esquema API 19/20/21 → Minkar 19.6.0/20.0/21.0, el frente de GPU y el requisito de rendimiento.
- [ ] **Compatibilidad de versión:** mecanismo tipo `SystemVersionCompat.plist` y `SYSTEM_VERSION_COMPAT=1` para binarios enlazados con un SDK anterior.
- [x] **Decidir la familia del primer driver de GPU** y escribir el ADR: GCN 1.0, luego RDNA2, luego Gen9 ([ADR 0002](ADR/0002-GPU-First-Family.es.md), `69fed7cb81`). Un driver de pantalla es también lo que apaga la pantalla, que el framebuffer GOP no permite: hecho para GCN 1.0 con G1; la RX 6600 del PC 1 es la siguiente.
- [ ] **Medición:** medir el sistema de referencia en hardware original (PC 2) y guardar los valores fuera del repo público, para los gates de rendimiento de Minkar 21.0.
- [ ] **Rendimiento NVMe en QEMU:** las lecturas por el dispositivo de bloques van a 1 MB/s.

## Fuentes

- [El repositorio minkaros/minkar](https://github.com/minkaros/minkar), rama `devel`, verificada en `c7a066b196` (1 nov 2026).
- [Hardware.md](Hardware.md): equipos de referencia.
- [Commands.md](Commands.md): comandos de Minkar frente a la lista de referencia.
- [Userland.md](Userland.md): historial de comandos portados (exFAT, `notifyutil`, `launchctl kickstart`).

El estado sale del repositorio; las decisiones de arquitectura, los requisitos de rendimiento y la secuencia de la pila gráfica son propuestas de este plan, y las que se confirman se registran aquí.
