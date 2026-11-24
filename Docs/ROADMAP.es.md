# Hoja de ruta

Qué busca Minkar, cómo se mide, qué está hecho y qué queda abierto. Última revisión:
21 nov 2026. Es una copia de [ROADMAP.md](ROADMAP.md) y se actualiza en el mismo commit.

El estado de un vistazo está en [Progress.md](Progress.md), generada desde
`progress/status.toml`; hasta dónde llega cada app designada, en
[Compatibility.es.md](Compatibility.es.md); por qué las cosas son como son, en
[Decisions.md](Decisions.md).

## Objetivo

Minkar ejecuta los binarios publicados de las apps hechas para la plataforma de
referencia, sin modificarlos ni recompilarlos, un nivel de API por detrás del actual de la
plataforma: hoy el nivel actual es la API 26 (solo arm64), así que el objetivo es la
API 25. El avance se mide por las apps designadas, no por «API N completa»: una versión se
cierra cuando su conjunto de apps designadas arranca y funciona desde los binarios
publicados (gate BC).

«API N» nombra la API Cocoa pública de Darwin N; no es el kernel. El código, los plists y
las macros de disponibilidad usan el número de versión.

| API | Versión en el código | Año |
| --- | --- | --- |
| API 19 | 10.15 | 2019 |
| API 20 | 11.0 | 2020 |
| API 21 | 12.0 | 2021 |
| API 22–24 | 13.0 a 15.0 | 2022–2024 |
| API 25 | 26.0 | 2025, el objetivo |
| API 26 | 27.0 | 2026, el nivel actual de la plataforma de referencia (solo arm64) |

## Versiones

La versión de Minkar sigue el número de Darwin de su kernel. `ProductVersion` va aparte:
indica el nivel de compatibilidad (hoy 10.15), nunca la versión de Minkar, y sube cuando
las apps designadas que necesitan uno mayor pasan el gate BC.

| Minkar | Kernel | Qué la cierra |
| --- | --- | --- |
| 19.6.0 | Darwin 19.6.0 | Los gates M5 y M6 (con el audio AU1–AU10) y el conjunto de apps 19 en verde (BC4) |
| 19.6.x | Darwin 19.6.0 | Correcciones, MB4, D4, K4, las apps A5–A10; `ProductVersion` puede subir por conjuntos de apps |
| 25.x | Darwin 25 | KX3: todos los gates pasan con el kernel nuevo |
| Después | Su Darwin | Apps designadas, arm64, SwiftUI, rendimiento |

- `ProductUserVisibleVersion` muestra la versión (19.6.0); `sw_vers` imprime
  `Minkar 19.6.0 (kernel Darwin 19.6.0)`; `uname -r` sigue en 19.6.0 hasta KX3.
- `@available`, `NSProcessInfo` y `LSMinimumSystemVersion` leen `ProductVersion`; cada
  símbolo que falta es un stub ruidoso, así que `@available` no oculta ningún hueco
  silencioso. `SYSTEM_VERSION_COMPAT` para binarios enlazados con un SDK anterior está por
  hacer.
- Un gate decide una versión, no una fecha. El requisito de rendimiento (igual o mejor que
  el sistema de referencia en la misma máquina física) es el gate de una versión posterior
  a 25.x, medido con el kernel nuevo.

## Principios

1. **Gates comprobables, de abajo arriba.** Sin prueba de arranque en verde, no se cierra una fase.
2. **Los binarios publicados funcionan.** [Compatibility.es.md](Compatibility.es.md) y el registro de stubs ordenan el trabajo en frameworks, sea cual sea la API de un símbolo.
3. **Stubs ruidosos.** Cada stub registra su primera llamada.
4. **Reutilizar antes de escribir.** Primero código APSL, después swift-corelibs, después FreeBSD.
5. **Licencias limpias y sala limpia.** El código propio de Minkar es BSD-2-Clause o MIT; nadie que haya visto el código cerrado de la plataforma de referencia contribuye código.
6. **Alcance congelado por versión.** Un framework o una app entra solo si lo pide un binario designado o el registro de stubs; el audio y la GPU quedan fuera de la regla.
7. **Trabajo en curso limitado.** Como mucho dos carriles activos, más el audio y la GPU.

## Arquitectura

- Darwin 19.6.0 hasta KX3, después Darwin 25; objc4, CF-Lite, Foundation con el overlay `libswiftFoundation`, Swift 6.4.2, Combine desde OpenCombine.
- Un AppKit basado en Cocotron, el [servidor de ventanas](WindowServer.md) y el escritorio propios, [Atlas](Atlas.md); WPE WebKit bajo un `WebKit.framework` propio.
- Un compositor pensado para la GPU (Vulkan), Mesa, Metal sobre darling-metal e Indium ([GPU.md](GPU.md)); [SwiftUI](SwiftUI.md) sobre un motor adaptado de SwiftCrossUI.

## Fases cerradas

| Fase | Gate | Cerrada en |
| --- | --- | --- |
| 0 — Compilación, toolchain y CI | M0: la CI compila el sistema y lo arranca | `05fa75cb65` |
| 1 — Arranque de Darwin | M1: QEMU, PC 1 (SATA, NVMe, SMP) y PC 2 (NVMe, 24 hilos) | `9c3dbe9873` |
| 2 — Userland base | M2: launchd como PID 1, DHCP, acceso por consola y por SSH, resolución de nombres | `71d99e9cdb` |
| 3 — Frameworks base y Swift | M3: siete herramientas libres de línea de órdenes pasan sus pruebas | `22a0e67f0d` |
| 4 — Gráficos y WindowServer | M4: ventanas de AppKit dibujadas, arrastradas y con texto, en QEMU y en el PC 2 | `9d510ec6c2` |

Lo que quedó de estas fases:

- Procesadores AMD, x2APIC, arrancar sin `pci_keep_console=1`; en el PC 1, DNS por IPv6, ACPI S3 y Wi-Fi.
- Stubs ruidosos en todas partes, la caché compartida de dyld, libSystem optimizada; la prueba de velocidad de Rainbow en el gate M3.
- Perfiles de aprovisionamiento comprobados; la fase 3 del sandbox (IOKit, mapeos, información de procesos, preferencias).
- Lecturas NVMe a 1 MB/s en QEMU.

## Trabajo abierto

### Fase 5 — Escritorio Atlas

Gate M5: desde el arranque empieza una sesión gráfica, se abre una app desde el Dock, Files
navega y la resolución cambia desde Preferencias del sistema, con el lenguaje visual de
Atlas. Detalle: [Atlas.md](Atlas.md).

- [ ] LoginWindow: autenticación con Directory Services, los `LaunchAgents` del usuario, la pantalla bloqueada.
  Directory Services hecho: OpenDirectory (APSL) y su servicio en `directoryd` (usuarios,
  grupos, contraseñas; prueba `opendirectory`). Hechas la ventana de inicio de sesión y la sesión del usuario
  (autoLoginUser, la lista de usuarios, las apps de la sesión como el usuario, Log Out de vuelta a la ventana;
  prueba `login-window`); los `LaunchAgents` del usuario en su propio launchd (la sesión Background;
  los limitados a Aqua, después); la pantalla bloqueada (prueba `lock-screen`).
- [x] Menús: MB0–MB3 hechos (MB4 en 19.6.x).
- [x] Dock: D0–D3 hechos (D4 en 19.6.x).
- [x] Files: K0–K3 hechos (K4 en 19.6.x).
- [x] Preferencias del sistema: la app y sus paneles, con Notificaciones y Usuarios y grupos (usuarios
  creados, eliminados, administradores, contraseñas, opciones de inicio de sesión; prueba `users-pane`).
  Revisados frente a la referencia: General (aspecto, colores de acento y de resaltado, barras de
  desplazamiento, el navegador por omisión; prueba `general-pane`), Notificaciones, Red con su hoja
  Avanzado (prueba `network-pane`), Pantallas, Fecha y hora (prueba `timezone-change`), Ratón, Sonido,
  Teclado (Texto, Fuentes de entrada, Atajos: los atajos de todo el sistema con los números de la
  referencia en com.apple.symbolichotkeys; pruebas `hotkeys`, `symbolic-hotkeys`), Dock (prueba `dock-pane`).
- [ ] AppKit y el lenguaje visual de Atlas en todo el sistema. Hecho: los aspectos claro y oscuro
  (Automático según la hora) y el color de acento, que todas las apps siguen al momento, la barra de
  menús, el Dock y los Menu Extras (resaltados con el acento, iconos plantilla teñidos; iconos de
  Lucide); prueba `general-pane`. Falta: los grises fijos que quedan en controles de AppKit y en las apps.
- [x] Órdenes: `lsappinfo` (prueba `lsappinfo`); `mdls`, `mdfind`, `mdutil` sin índice, `mdfind` busca en el disco (prueba `md-commands`); `say` con eSpeak NG, en un proceso aparte (prueba `say`).

### Fase 6 — Aplicaciones y distribución

Gate M6, Minkar 19.6.0: instalado en una máquina física desde su propio medio USB; un DMG
descargado, montado y su app ejecutada; Atlas completo para el uso diario (MB2–MB3,
D2–D3, K2–K3, A0–A4, N0–N1, W0–W3); WebKit hasta W5; audio AU1–AU10; el conjunto de
apps 19 en verde (BC4).

- [x] DMG: UDIF con bloques ADC, zlib, bzip2, LZFSE y LZMA conectados por el `MinkarDiskImage` del kernel; `hdiutil convert` y `create`.
- [x] Compresión de HFS+ (decmpfs): lectura con `MinkarFSCompression` (zlib, LZVN, LZFSE), escritura con `ditto --hfsCompression` (LZFSE; prueba `hfs-compression`).
- [ ] El resto de la adaptación de hfs (`Kernel/_PROVENANCE`).
- [ ] Instalador del sistema: una imagen USB arrancable, la app instaladora, el primer arranque (idioma, teclado, zona horaria, el primer administrador); sin cuentas de prueba en la imagen publicada. Hecho en QEMU: `bmake installer-image` (I3), el Instalador (I4: comprobaciones, borrado del disco, copia comprimida del sistema, `bless --setBoot`; menos de dos minutos, 405 MB), la Configuración inicial (I5); la prueba de instalación `tools/install-gate.sh` en el repositorio de pruebas (I6). Hecho en PC 1 (24 nov 2026): el medio arranca desde un lector de tarjetas USB e instala en el disco SATA en unos cuatro minutos; el sistema instalado arranca en el modo propio de la pantalla, con la Configuración inicial en español y sonido por el códec de la placa (`/var/log/install.log` en PC 1; commits 1b6817027b, 4bee1bce14). Falta: instalar en PC 2, ajustar el tamaño del medio a su contenido, y el reinicio desde el medio (75 s, frente a 53 s del sistema instalado).
- [x] Apps: A0–A4 hechas ([Atlas.md](Atlas.md#5-applications)); A5–A10 en 19.6.x.
- [ ] Órdenes: las órdenes Tcl de git. Hecho: `osascript`, intérprete propio de la parte más usada de AppleScript (prueba `osascript`); `opendiff`, las diferencias en Text Editor (prueba `opendiff`). `tmutil` llega con las copias de seguridad en 19.6.x (decisión del mantenedor, 23 nov 2026).

#### WebKit

- [ ] W0 — Sus dependencias en `/usr/lib`: parcial (21 de 23 pasan sus propias pruebas).
- [ ] W1 — JavaScriptCore con su JIT: la batería ES6 pasa; faltan las demás.
- [x] W2 — WebCore y los procesos sobre una plataforma WPE de Minkar. `5abd93f984`
- [x] W3 — `WKWebView`. `a5d929ebdd`
- [x] W4 — El `WebView` antiguo. `86ed0b5d33`
- [ ] W5 — Las vistas web de las apps designadas: funciona lo que usa su código (`ac148eb478`); faltan las propias apps.

#### Audio y medios

Frameworks propios de Minkar sobre FFmpeg (LGPL) y dav1d; sin GTK, Qt ni GStreamer.

- [x] AU1–AU6 — IOAudioFamily y HDA, el servicio de audio y CoreAudio, AudioToolbox, FFmpeg y dav1d, CoreMedia y AVFoundation, los medios de WebKit. `f39b73ee62`
- [ ] AU7 — USB Audio Class 2.0.
- [ ] AU8 — CoreMIDI, framework y servicio.
- [ ] AU9 — Alojar Audio Units, y Accelerate.
- [ ] AU10 — Dispositivos agregados y plug-ins de dispositivo; latencia y deriva medidas frente a la referencia.

### Carril BC — Compatibilidad binaria

Los binarios x86-64 publicados de las apps designadas arrancan y funcionan sin
recompilarlos; las apps y hasta dónde llega cada una están en
[Compatibility.es.md](Compatibility.es.md). Los nombres de instalación, la disposición de
las ivars públicas y los tamaños de las structs se mantienen como en la plataforma de
referencia.

- [ ] BC0 — El gate BC (`bmake bc-check`) en la CI con todas las apps designadas; espera su primera ejecución en la CI.
- [x] BC1 — La matriz generada, [Compatibility.es.md](Compatibility.es.md).
- [ ] BC2 — Casos del cargador: chained fixups, `LC_BUILD_VERSION`, binarios universales, frameworks incrustados, firmas de terceros y hardened runtime, `SYSTEM_VERSION_COMPAT`; parcial.
- [ ] BC3 — Auditoría de ABI a partir de las codificaciones de tipos de los binarios de las apps, sin los headers del fabricante.
- [ ] BC4 — El conjunto de apps 19 en verde; parte de M6.
- [ ] BC5… — Un grupo de apps designadas por cada subida de `ProductVersion`.

### Carril ARM — arm64

Con el kernel nuevo, y por ahora solo en una máquina virtual.

- [ ] ARM0 — El userland y los frameworks compilan para arm64 en la CI (cerca del 96 % de los objetos); no se enlaza ni se arranca nada.
- [ ] ARM1 — Darwin 25 arranca en arm64 en una VM hasta el gate M2; el destino de arranque no está decidido.
- [ ] ARM2 — El escritorio en arm64 en QEMU.
- [ ] ARM3 — Arrancan los binarios arm64 de las apps designadas.

### Carril KX — Kernel nuevo: Darwin 25

`xnu-12377.121.6`, primero x86-64; detalle en [ADR 0003](ADR/0003-Kernel-Upgrade.md).
Hasta KX3, `devel` sigue en Darwin 19.6.0 y el código nuevo de frameworks y escritorio
compila con los dos kernels.

- [x] KX0 — ADR 0003 aceptado para x86-64, con condiciones; una comprobación de que Darwin 25 compila.
- [ ] KX1 — Darwin 25 arranca en QEMU hasta el gate M2 en una rama; después de publicar 19.6.0 y después de R1.
- [ ] KX2 — Todos los gates en QEMU, en el PC 1 y en el PC 2.
- [ ] KX3 — `devel` pasa a Darwin 25; Darwin 19 queda como la rama de mantenimiento de 19.6.x.

### Carril SU — SwiftUI

Detalle en [SwiftUI.md](SwiftUI.md).

- [ ] SU0 — El inventario de símbolos informa de lo que importan de SwiftUI las apps designadas; en la CI.
- [x] SU0b — La revisión de sala limpia y de licencias de SwiftCrossUI, aprobada el 21 nov 2026.
- [ ] SU1 — El diseño, [SwiftUI.md](SwiftUI.md); propuesto.
- [ ] SU2 — SwiftCrossUI importado; las apps de prueba de SwiftUI de Minkar muestran sus ventanas. Después de 19.6.0.
- [ ] SU3 — Cada símbolo de SwiftUI que importan los binarios designados existe con el mismo nombre y la misma disposición.
- [ ] SU4 — Arranca un binario publicado que enlaza SwiftUI (candidata: iTerm2).

### Carril R — Estructura

El núcleo se queda en un repositorio; el resto sale por pasos. Detalle en
[Structure.md](Structure.md).

- [ ] R0 — El inventario y Structure.md; no se mueve nada.
- [ ] R1 — El toolchain a su propio repositorio, como artefacto fijado, con AArch64; justo después de 19.6.0, antes de KX1.
- [ ] R2 — Las fuentes de terceros de BSD como descargas fijadas más parches; 19.6.x.
- [ ] R3 — La CI publica cada capa compilada; 19.6.x, después de R1.
- [ ] R4 — Las apps de Atlas a su propio repositorio, compiladas con el SDK publicado.

### Pista de la GPU

En paralelo con todo, y base del requisito de rendimiento. GCN 1.0 (PC 2): G1–G4 hechos,
falta G5 (Metal); lo siguiente, los relojes de la GPU. RDNA2 (PC 1): R0–R2 hechos; lo
siguiente, R2b. Después, Intel Gen9. Detalle en [GPU.md](GPU.md).

## Próximos pasos

El orden del mantenedor (23 nov 2026). Carril A, uno tras otro:

1. Imprimir: hecho (cupsd y sus comandos, el panel Impresoras); falta probarlo con una impresora real en el PC 1.
2. W0–W3: pestañas de ventana (hecho), mosaico (hecho), restauración de ventanas (hecho), Emoji y símbolos (hecho; Noto Emoji, monocromo).
3. Los comandos de las fases 5 y 6: `lsappinfo`, `mdfind`, `mdls`, `mdutil`, `say`, `osascript`,
   `opendiff` (hecho); `tmutil` pasa a 19.6.x con las copias de seguridad.
4. La autorización de Security completa (decisión del mantenedor, 23 nov 2026): authd (el código
   APSL del árbol) con su base de derechos, el cliente APSL en Security.framework, el Agente de
   autorización de Atlas (la ventana de contraseña), `AuthorizationExecuteWithPrivileges` con un
   trampolín setuid (hecho; prueba `image/authorization`); el candado de los paneles
   (SecurityInterface) en Impresoras, Fecha y hora, Usuarios y grupos y Red (SCHelper, APSL)
   (hecho; prueba `image/pane-lock`); CUPS con los derechos de autorización, admin anidado
   en `_lpadmin`. Hecho.
5. El instalador del sistema y la compresión de HFS+.
6. WebKit: W0, W1, W5.
7. LoginWindow: la sesión Aqua del usuario (agentes limitados a Aqua); la espera hasta la ventana
   de inicio de sesión en el PC 1.
8. El lenguaje visual (el aspecto claro y los controles), cuando el mantenedor decida.

En paralelo, el carril BC: KeepingYouAwake (el del conjunto 19 sin ventana), BC2, BC0 en la CI.
Audio AU7–AU10 y la GPU (relojes en el PC 2, R2b en el PC 1) en paralelo, como siempre.

## Riesgos

| Riesgo | Mitigación |
| --- | --- |
| Un equipo pequeño con muchas capas abiertas | Gates en orden; dos carriles más el audio y la GPU; alcance congelado por versión |
| Poca gente con experiencia en XNU | El arranque documentado paso a paso; primero QEMU |
| CF-Lite es anterior a las API que usan las apps | swift-corelibs CF como referencia; pruebas de puente en cada cambio |
| Los drivers de GPU, el bloque más largo | Una pista en paralelo, una familia cada vez |
| `ProductVersion` subida con la API incompleta | Stubs ruidosos; el gate BC antes de cada subida |
| Apps designadas poco representativas | La lista se revisa en cada subida |
| Piezas no publicadas del kernel nuevo | Listadas en el ADR 0003; un prototipo en una rama |
| Licencias y sala limpia | Una revisión de licencia en cada importación |
