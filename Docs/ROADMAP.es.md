# Minkar — Plan de acción arquitectónico

31 oct 2026

## Resumen ejecutivo

La meta de Minkar es un sistema sobre Darwin 19.6.0 que compile y ejecute apps Cocoa. La primera versión, **Minkar 19.6.0**, entrega la API 19 con el escritorio Gienah y su propio lenguaje visual; Minkar 20.0 y 21.0 completan las API 20 y 21 sin cambiar el kernel. La ruta crítica es estrictamente ascendente: arranque Darwin → launchd → userland → frameworks → WindowServer → Gienah → apps. Objective-C y Swift completo, DMG, compatibilidad binaria y arm64 se paralelizan o se difieren para no bloquear esa ruta.

El plan tiene 8 fases (0–7), cada una con entregables y un criterio de salida verificable en CI. Ninguna fase superior se da por cerrada si la inferior no pasa su gate. Desde el 29 oct 2026 rige además un requisito de rendimiento: igual o mejor que el sistema de referencia en el mismo equipo físico, exigido desde Minkar 21.0 (ver Requisitos de rendimiento).

**Fuente de verdad del estado:** [`Docs/ROADMAP.md`](https://github.com/minkaros/minkar/blob/devel/Docs/ROADMAP.md) en el repositorio (en inglés). Este documento lo refleja en español; verificado contra la rama `devel` en `fb6e25b877` (31 oct 2026), 95 commits por delante de `main`.

### Principios arquitectónicos

1. **Gates verificables, de abajo arriba.** Cada fase termina con una prueba automatizada que arranca la ISO en QEMU. Sin prueba verde no hay fase cerrada.
2. **El contrato es la API pública Cocoa, por números de API.** Primero la API 19, luego la 20 y la 21. Cada símbolo se clasifica como implementado, stub o ausente, y se mide.
3. **Stubs ruidosos.** Todo stub registra su primera llamada en el log del sistema. Así las apps reales dicen qué falta, en orden de uso.
4. **Reutilizar antes que escribir.** Primero código abierto APSL (xnu, dyld, launchd, hfs), luego swift-corelibs, luego userland FreeBSD. Código nuevo solo para los huecos.
5. **Licencias limpias.** Código nuevo en MIT o BSD-2, nada GPL en componentes base (por ejemplo DMG) y regla de sala limpia: nadie que haya visto código cerrado de la plataforma de referencia aporta código. Las marcas de terceros no se usan en nombres, iconos ni textos públicos; un único TRADEMARKS.md lo declara.
6. **Apps de referencia como pruebas de aceptación.** Un conjunto fijo de apps de código abierto escritas para la API Cocoa se compila en cada hito; su lista de símbolos importados prioriza el trabajo en frameworks.

## Decisión de versiones

Minkar separa tres capas que en la plataforma de referencia van juntas. El kernel se queda en Darwin 19.6.0; la API y el aspecto avanzan por su cuenta.

| Capa | Decisión | Nota |
| --- | --- | --- |
| Kernel y base BSD | Darwin 19.6.0 | Se toca solo cuando una API nueva necesite una interfaz del kernel |
| API | API 19 primero; luego API 20 y 21 | Cada API guiada por los imports de las apps de referencia |
| Aspecto | Lenguaje visual Gienah desde la Fase 5 | Diseño propio con su guía; sin iconos, fuentes ni recursos gráficos de terceros |

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
| 19.6.0 | API 19, escritorio Gienah y DMG; gates funcionales M0–M6 en equipo físico | 19.6.0 | 10.15 |
| 19.6.x | Correcciones; pueden traer partes de API 20 | 19.6.0 | 10.15 |
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
| PC 2 | Por elegir; para medir la referencia, hardware original del fabricante (candidato: Mac Pro 6,1) | Ídem |

- **Cómo se mide:** Minkar y el sistema de referencia en el mismo equipo (arranque dual), con los mismos benchmarks ejecutados por SSH. El sistema de referencia solo se mide en hardware original de su fabricante (candidato: Mac Pro 6,1), porque su licencia no permite otro, y sus valores se guardan fuera del repo público.
- **Consecuencia:** si la referencia solo se mide en el Mac Pro 6,1, Minkar también debe medirse ahí, y su GPU (FirePro D300, GCN 1.0, la más antigua y con soporte menos maduro) pasa a ser la del primer driver de alto rendimiento.
- **Decisión abierta — familia del primer driver de GPU:** PC 1 usa la gráfica integrada Intel HD 630 (Gen9) salvo que tenga una tarjeta dedicada; otro equipo disponible tiene AMD RX 6600 (RDNA2), útil como equipo de pruebas de Minkar sin publicar mediciones del sistema de referencia en él. Hay que elegir entre empezar por la GPU del PC 1 (desarrollo más rápido) o por la del Mac Pro 6,1 (donde se puede comparar).

## Línea base

Minkar arranca sobre Darwin con su propio cargador EFI, todo el sistema se compila como Mach-O y ya corre en un PC físico con sus ocho hilos de CPU y desde su SSD NVMe. La Fase 2 está cerrada; el hueco mayor está entre el kernel y el escritorio. Estado al 31 oct 2026:

| Componente | Estado | Qué falta | Fase |
| --- | --- | --- | --- |
| Cargador EFI + kernel Darwin | Arranca en QEMU (TCG y KVM) y en el PC 1, desde SATA o NVMe; SMP con todas las CPU; el botón de encendido apaga de forma ordenada | PC 2, procesadores AMD, MSI-X | 1 |
| Build del sistema | `world` completo; la CI compila y arranca en `main` y a mano | — (M0 cumplido) | 0 |
| libSystem | Las apps de referencia encuentran todos sus imports de libSystem (308 de 308); `os_log` escribe en syslogd; `@available` compara con `ProductVersion`; dyld con @rpath, @loader\_path y re-exports probado | Stubs ruidosos, `logd`, shared cache de dyld | 3 |
| launchd / launchctl | launchd-842 como PID 1; launchctl con kickstart; `KeepAlive PathState`, `WatchPaths` y `QueueDirectories` | Nada | 2 (cerrada) |
| Userland y herramientas del sistema | Todos los comandos de la Fase 2 (337 de 354 de la lista de referencia) | `open` y los comandos de fases posteriores | 2 (cerrada) |
| DiskArbitration | Framework y `diskarbitrationd` originales (APSL); `diskutil` y `hdiutil` propios; monta HFS+, FAT y exFAT | — | 2 (cerrada) |
| CoreFoundation | CF-Lite (APSL) | APIs que faltan hasta la API 21; puente `CFArray`/`CFDictionary` | 3 |
| Foundation | Tres lotes de correcciones (26 oct); `NSProcessInfo` con `operatingSystemVersion` (31 oct) | `NSURLSession`; auditoría de la API 19 | 3 |
| Swift 6 | Nada en el árbol | Compilador, stdlib, overlays | 3 |
| WindowServer | Compila sobre XNU con backend IOFramebuffer; nunca ejecutado | Ejecutarlo; entrada por IOHIDSystem | 4 |
| Aceleración por GPU | No existe; sin ella tampoco se puede apagar la pantalla | Frente completo (requisito de rendimiento) | 4 y Minkar 21.0 |
| SystemUIServer, Dock | Parciales, no se compilan | Casi todo | 5 |
| Kraz, System Preferences | No existen | Apps nuevas en Cocoa | 5 |
| AppKit | Parcial (Cocotron) | Piezas recientes de la API y lenguaje visual Gienah | 5 |
| DMG, apps base | `hdiutil attach` ya monta por DiskArbitration | `DiskImages.framework` (UDIF), apps | 6 |
| Compatibilidad binaria, arm64 | Futuro; install names ya idénticos a los originales | Resto | 7 |

## Fase 0 — Build, toolchain y CI

Objetivo: cualquier contribuidor obtiene una ISO arrancable desde cero con un solo comando, y cada PR se prueba arrancando en QEMU. Sin esto, todas las demás fases avanzan a ciegas.

**Actividades**

- [x] Fijar el toolchain: `tools/toolchain/MANIFEST` fija la versión upstream, el árbol git y la línea de versión de cada componente (clang 17.0.6 de `swift-6.0-RELEASE`, ld64-609, y desde el 29 oct cctools con `ctf_insert` y las herramientas CTF de dtrace); `manifest.sh check` lo verifica en la CI y comprueba que una entrada compila, enlaza y archiva a los mismos bytes dos veces.
- [x] Targets de bmake por capa: `bmake <capa>` y `bmake <capa>-check`, `layers-check` e `image-check`. Los frameworks de terceros pasaron a la Fase 3.
- [x] CI: `.github/workflows/build-boottest.yml` compila el toolchain (en caché), el sistema y la imagen, y pasa `boottest GATE=M2`. Desde el 29 oct corre en `main` y a mano, para no agotar los minutos de Actions de una cuenta gratuita.
- [x] Inventario de símbolos: `tools/symbols/`; Minkar cubre el 68,3% de los imports de sistema de las cuatro apps de referencia (31 oct).
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
- [ ] Lista de hardware de referencia (`Docs/Hardware.md`). *En curso:* PC 1 documentado; PC 2 sin elegir.
- [ ] Más de 64 procesadores y x2APIC (después de Minkar 19.6.0); hoy una máquina mayor corre con sus primeras 64.
- [ ] Procesadores AMD: el kernel los reconoce, pero Minkar nunca ha corrido en uno. Primero en QEMU con `-cpu EPYC`, luego en un PC AMD.
- [ ] Arrancar sin `pci_keep_console=1`; IOKit tarda más de 30 s en quedar en reposo.
- [ ] IPv6 en la red del PC 1: RDNSS y respuestas DNS sobre IPv6, que la red de usuario de QEMU no da (movido de la Fase 2 el 31 oct).
- [ ] Wi-Fi del PC 1 (Intel, M.2): sin driver.

* [ ] **Criterio de salida (M1):** cumplido en QEMU y en el PC 1 (desde SATA y desde NVMe, con SMP). Falta un segundo PC.

## Fase 2 — Userland base

Objetivo: un sistema Unix multiusuario completo sobre Darwin, gestionado por launchd y con los servicios que luego necesitan Gienah y las apps.

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
1. Completar la CF-Lite hasta la API 21, con pruebas de bridging en cada cambio. Pendiente: `CFArray` y `CFDictionary` aún no tienen puente a `NSArray` y `NSDictionary`.
2. Auditar Foundation contra la API 19 empezando por lo que importan las apps de referencia. *Hecho (25–26 oct):* `NSException`, `NSUUID`, `NSSecureCoding`, `NSRegularExpression`, `NSJSONSerialization` (con JSON5), `NSCache`, `NSInvocation` con la convención System V, `NSHashTable` y `NSMapTable` débiles, listas de propiedades binarias byte a byte como la referencia, `NSProgress` y la API moderna de `NSString`, además de decenas de fallos encontrados al probar. *Hecho (31 oct):* `NSProcessInfo` con `operatingSystemVersion` (10.15.0), `isOperatingSystemAtLeastVersion:`, `operatingSystemVersionString`, procesadores, memoria y `systemUptime` (`fb6e25b877`). *Queda del informe de símbolos:* `NSURLSession`. *Fallo conocido:* `NSKeyedUnarchiver` no lee la lista de propiedades XML que escribe `NSKeyedArchiver`. Sin probar: el comportamiento dependiente del locale (los datos de locale UTF-8 están en la imagen desde el 27 oct).
3. Security (APSL) para certificados y keychain básico, necesario para TLS en `NSURLSession`.
4. LaunchServices: base de datos de bundles y tipos UTI; `open` y Kraz dependen de ella.
5. CoreText y CoreData: implementación mínima guiada por uso real (ver la guía técnica de la pila gráfica).
6. Swift 6: stdlib y runtime completos, con concurrencia en el sistema, y module maps para los frameworks.
7. API 19 → 20 → 21, subiendo `ProductVersion` (10.15 → 11.0 → 12.0) al cerrar cada una; cada cierre es una versión de Minkar (19.6.0, 20.0, 21.0).

Llegan de la Fase 2 (31 oct 2026):

- [ ] Completar libc, libm, pthread, libdispatch, libxpc, notify y os\_log. *Parcial:* desde el 31 oct las apps de referencia encuentran todos sus imports de libSystem (308 de 308): `os_log` escribe sus mensajes en syslogd, con `%{public}`/`%{private}` y `%@`, y en stderr en modo DT (antes no escribía nada, y el `<os/log.h>` del SDK no tenía `os_log()`); `syslog(3)` vuelve a llegar a syslogd; `os_log_error`/`os_log_fault` con los nombres de la referencia; `os_activity`; `_availability_version_check` para `@available`; `launch_activate_socket` (`0709544df7`, `c0923c1bc7`, `5051c781b4`). `math.h` compatible con la referencia desde el 26 oct; a `vis(3)` le faltan las flags nuevas.
- [ ] Stubs ruidosos (log en la primera llamada).
- [ ] Shared cache de dyld (opcional al principio).
- [ ] `logd`, con `os_log` (mientras tanto, `syslogd` y `aslmanager`: los mensajes de `os_log` van a syslogd hasta que `logd` los guarde).

**Toolchain Swift** (el prototipo es gate antes del resto del trabajo de Swift)

- [ ] Prototipo 1: compilar `swiftc` (`swiftlang/swift`, rama `release/6.0`) en el host Ubuntu como compilador cruzado, contra el LLVM `swift-6.0-release` del árbol.
- [ ] Prototipo 2: compilar `libswiftCore` para `x86_64-apple-macosx10.15` contra `Minkar.sdk` y ejecutar un "hello world" de Swift (no Embedded) en QEMU. Si falla, reevaluar compilar la stdlib en un host x86-64 de la plataforma de referencia.
- [ ] `libswift_Concurrency` y overlays de sistema: `Darwin`, `ObjectiveC`, `CoreFoundation` y `Dispatch` (desde `libdispatch/src/swift`).
- [ ] Overlay `libswiftFoundation` a partir de la última versión abierta, actualizado a Swift 6.0.
- [ ] Instalar todo en `/usr/lib/swift` dentro de la imagen y añadir un gate de `boottest` que ejecute un binario Swift.

**Criterio de salida (M3):** un conjunto fijo de herramientas CLI de código abierto escritas para la API Cocoa (ObjC y Swift) compila y pasa sus tests en Minkar.

## Fase 4 — Gráficos y WindowServer

Objetivo: WindowServer dibuja ventanas de AppKit y recibe teclado y ratón. En la VM basta el camino por software; en los equipos físicos el objetivo es la composición por GPU, porque el requisito de rendimiento lo exige.

**Arquitectura propuesta**

- Una interfaz de backend de pantalla dentro de WindowServer: `GOP` (software) → `virtio-gpu` (VMs) → acelerado. AppKit y Quartz nunca ven el backend.
- Composición con Onyx2D y QuartzCore, igual que hoy; el puerto cambia la salida, no el modelo de dibujo.
- Eventos: HID del kernel → WindowServer → cola de eventos de cada app, con el mismo modelo de `NSEvent`.
- Comunicación app ↔ WindowServer por Mach/XPC, no por sockets ad hoc.

**Decisión de backend (25 oct 2026)**

- **Salida inicial:** `IOGOPFramebuffer.kext` con `IOFramebufferDisplay.m` y composición por software. Sirve para Minkar 19.6.0 y para la VM; en los equipos físicos es solo el punto de partida.
- **Compositor (cambio del 29 oct 2026):** se diseña para GPU desde el inicio, con Vulkan como backend principal detrás de una interfaz propia. pixman queda para el arranque inicial y para VMs sin GPU.
- **VMs:** driver virtio-gpu 2D como subclase de `IOFramebuffer`; los modos 3D de virtio-gpu (virgl, Venus) son opcionales para probar caminos acelerados.
- **Entrada:** IOHIDFamily, con `ApplePS2Controller` en QEMU y xHCI + HID (`MinkarUSB`) en hardware real. libinput, libevdev y mtdev ya no se compilan.
- **Teclado:** libxkbcommon como solución temporal; a largo plazo `UCKeyTranslate` y Text Input Sources (Fase 5).
- **GPU (cambio del 29 oct 2026):** deja de estar diferida. Es un frente paralelo desde ya: driver del kernel para una familia, Mesa Vulkan/OpenGL, compositor en GPU y, a largo plazo, Metal nativo sobre el driver. La primera familia es una decisión abierta (ver Requisitos de rendimiento).

**Actividades**

- [ ] Backend de salida sobre IOFramebuffer. *Parcial:* el kext carga y WindowServer compila con el backend, pero nunca se ha ejecutado.
- [ ] WindowServer arranca y dibuja en QEMU. Su job (`org.minkar.WindowServer.plist`) está en el árbol pero no se instala.
- [ ] Entrega de eventos de IOHIDSystem al espacio de usuario: el bucle de despacho está bajo un `NEW_HID` sin definir y las colas de eventos no se crean.
- [ ] Lector de entrada IOHIDSystem en WindowServer (`kIOHIDServerConnectType`, `EvGlobals`, puerto Mach).
  - [ ] Teclado PS/2. *Parcial:* sus teclas llegan a la consola, no a WindowServer.
  - [ ] Driver `ApplePS2Mouse` sobre `ApplePS2MouseDevice`; `IOHIPointing` se compila desde `90c96e8862`.
  - [x] Headers `IOKit/hidsystem` en el SDK.
  - [x] xHCI + HID (`MinkarUSB`), probado en QEMU y en el PC 1.
- [ ] Retirar `libinput`, `libevdev` y `mtdev` del árbol.
- [ ] virtio-gpu 2D como subclase de `IOFramebuffer`.
- [ ] `pbcopy` y `pbpaste` sobre el servidor de pasteboard, y `screencapture` sobre la captura de WindowServer (movidos desde la Fase 2).
- [ ] Multi-monitor y cambio de resolución, después de M4.
- [ ] ADR de GPU: familia, licencias (solo partes MIT/BSD de drm-kmod), capa tipo LinuxKPI para XNU y prototipo KMS en el equipo de referencia.
- [ ] Gate de rendimiento de composición en el PC de referencia (requisito de Minkar 21.0).

**Criterio de salida (M4):** en QEMU y en un PC de referencia, varias apps de AppKit abren ventanas, se arrastran, reciben teclado y redibujan sin artefactos.

## Pila gráfica: componentes y secuencia

Del entorno gráfico ya existen la base de dibujo (Core Graphics con FreeType, y pixman, libpng, libjpeg y libtiff en `BSD/lib`), la consola GOP y la entrada USB. Faltan el servidor de composición, IOSurface y todo lo que depende de la GPU. Estado verificado en `devel` (`fb6e25b877`, 31 oct 2026): desde el 29 oct no hubo cambios en la pila gráfica.

Componentes, por estado:

- **Existen:** Core Graphics con FreeType; pixman, libpng, libjpeg y libtiff (`BSD/lib`); la consola GOP (`IOGOPFramebuffer`); la entrada USB (xHCI + HID, `MinkarUSB`); los headers `IOKit/hidsystem`.
- **Parciales:** WindowServer (compila, nunca se ha ejecutado); el teclado PS/2 (llega a la consola, no a WindowServer); QuartzCore y Onyx2D; AppKit (Cocotron).
- **Faltan:** la entrega de eventos de IOHIDSystem; `ApplePS2Mouse`; virtio-gpu 2D; IOSurface; el compositor de Core Animation; Core Text sobre HarfBuzz/FreeType; giflib y libwebp; ColorSync sobre Little CMS; y el frente de GPU (driver del kernel, Mesa, Metal).

El detalle técnico de cada pieza está en la guía técnica de la pila gráfica.

Secuencia de desarrollo:

1. Entrega de eventos de IOHIDSystem y ratón PS/2.
2. WindowServer sobre el framebuffer GOP.
3. virtio-gpu 2D (puerto desde FreeBSD, BSD-2).
4. IOSurface (memoria compartida Mach).
5. Compositor de Core Animation, diseñado para GPU (backend Vulkan; pixman solo para el arranque y las VMs).
6. HarfBuzz/FreeType para Core Text, giflib/libwebp, Little CMS para ColorSync.
7. AppKit moderno con vistas respaldadas por capas.

En paralelo, el frente de GPU G1–G5: desde la familia del primer driver y el prototipo KMS hasta Mesa (lavapipe/llvmpipe en VMs) y Metal (darling-metal e Indium, MPL-2.0).

El paso 1 es el siguiente: sin entrada, WindowServer no puede cerrar M4. G1 empieza en cuanto se decida la familia de GPU, y las flechas discontinuas marcan lo que el frente de GPU reutiliza de los pasos 4 y 5.

## Fase 5 — Escritorio Gienah

Objetivo: una sesión gráfica completa con el lenguaje visual Gienah, con login, barra de menús global, Dock, escritorio con iconos y preferencias. El orden sigue lo que el usuario ve al arrancar.

1. **LoginWindow:** autenticación contra el servicio de usuarios de esta fase (Directory Services, más abajo) y arranque de la sesión (`LaunchAgents` del usuario).
2. **SystemUIServer y barra de menús:** menú de la app activa vía WindowServer, más una API pública de Menu Extras y `NSStatusItem` (prioridad oficial).
3. **Menu Extras básicos:** volumen, pantallas, red, reloj, notificaciones. Cada uno como bundle cargable, no compilado dentro de SystemUIServer.
4. **Dock:** lanzador, apps abiertas, minimizado y gestión del fondo de pantalla, sobre LaunchServices.
5. **Kraz:** app nueva en Cocoa puro. Navegación con LaunchServices, montaje vía DiskArbitration, iconos de escritorio, papelera y "Abrir con".
6. **System Preferences:** app contenedora + cargador de bundles `.prefPane` (`NSPreferencePane`). Paneles iniciales: Pantallas, Teclado, Usuarios, Red, Fecha y hora.
7. **AppKit:** completar controles faltantes y dar a todo el sistema el lenguaje visual Gienah (diseño propio, sin recursos de terceros), priorizado por lo que usan Kraz y System Preferences.

Criterio transversal: todos estos componentes son apps o bundles Cocoa normales. Así funcionan como prueba continua de AppKit.

Comandos que llegan con esta fase (movidos desde la Fase 2):

- [ ] `open` y `lsappinfo`, sobre LaunchServices.
- [ ] `mdfind`, `mdls` y `mdutil`, con un índice de metadatos para la búsqueda de Kraz.
- [ ] Gestión de usuarios estilo Directory Services, para LoginWindow y el panel Usuarios (movida de la Fase 2 el 31 oct).
- [ ] `say`, con un driver de audio y síntesis de voz.

**Criterio de salida (M5):** desde el arranque se llega a una sesión gráfica, se abre una app desde el Dock, se navega con Kraz y se cambia la resolución desde Preferencias.

## Fase 6 — Aplicaciones y distribución

Objetivo: instalar software arrastrando la app desde una imagen de disco a `/Applications` y tener las apps básicas de trabajo. Esta fase es el primer uso real del sistema y la mejor fuente de bugs.

**DMG**

1. `DiskImages.framework` propio (MIT/BSD): lectura del formato UDIF con sus compresiones habituales, luego escritura.
2. Montaje vía DiskArbitration en `/Volumes/<etiqueta>`, usando el driver HFS+ de la Fase 1.
3. `hdiutil` mínimo (`attach`, `detach`, `create`) encima del framework, y doble clic en Kraz.

**Aplicaciones**

| App | Opción recomendada | Por qué | Dependencia crítica |
| --- | --- | --- | --- |
| Terminal | Escribir en Cocoa | Ejercita `NSTextView`, pty y fuentes | AppKit texto, CoreText |
| TextEdit | Escribir en Cocoa | Ejercita documentos, RTF y Cocoa Text System | `NSDocument`, CoreText |
| Navegador | Ladybird (BSD-2) | Ya tiene interfaz AppKit propia | Foundation de red, AppKit |
| IDE | CodeEdit, a largo plazo | Depende de SwiftUI, excluido de las API 19–21 | SwiftUI |

Riesgo a señalar: muchas apps Cocoa actuales usan SwiftUI, que no figura en el roadmap. Conviene registrarlo como decisión abierta en lugar de descubrirlo tarde.

Comandos que llegan con esta fase (movidos desde la Fase 2):

- [ ] `osascript`, con una Open Scripting Architecture para las apps.
- [ ] `opendiff`, con una app de comparación de archivos.
- [ ] `tmutil`, con copias de seguridad.
- [ ] Los comandos de git en Perl, Python y Tcl (`git send-email`, `gitk`…), con esos lenguajes (movidos de la Fase 2 el 31 oct).

Adelanto: `hdiutil attach` ya monta imágenes en `/Volumes` a través de DiskArbitration (30 oct 2026); falta leer el formato UDIF de los DMG.

**Criterio de salida (M6):** se descarga un DMG, se monta, se copia la app a `/Applications` y corre; Terminal, TextEdit y el navegador se usan a diario en la propia máquina de desarrollo.

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

M0 y M2 están cumplidos, M1 solo espera el segundo PC y la Fase 2 está cerrada (31 oct). El trabajo activo está en las Fases 3 y 4, que deben cerrar antes de Gienah. El frente de GPU arranca en paralelo y alimenta el requisito de rendimiento de Minkar 21.0: igual o mejor que el sistema de referencia en el mismo equipo. La Fase 7 sigue después de Minkar 19.6.0.

| Gate | Fase | Estado al 31 oct 2026 |
| --- | --- | --- |
| M0 | Build, toolchain, CI | Cumplido (CI verde desde `05fa75cb65`) |
| M1 | Arranque Darwin | Cumplido en QEMU y en el PC 1 (SATA, NVMe, SMP); falta el PC 2 |
| M2 | Userland base | Cumplido (`boottest GATE=M2`, 13 marcadores); Fase 2 cerrada el 31 oct |
| M3 | Frameworks y Swift | Sin empezar; hay trabajo previo en Foundation |
| M4 | Gráficos y WindowServer | En curso: WindowServer aún no se ejecuta |
| M5 | Escritorio Gienah | Sin empezar |
| M6 (Minkar 19.6.0) | Apps y distribución | Sin empezar; `hdiutil attach` ya monta |
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
- [ ] **PC 1 (Fase 1):** arranque sin `pci_keep_console`, IPv6 en la red (RDNSS, DNS sobre IPv6) y reposo ACPI S3; después, un segundo PC distinto (gate M1; para medir la referencia, el Mac Pro 6,1; el equipo con la RX 6600, como equipo de pruebas).
- [x] **Userland (Fase 2):** cerrada el 31 oct (exFAT en lectura y escritura, `launchctl kickstart`, rutas de dyld, notifyd e IPv6 probados; el resto pasó a las Fases 3, 5 y 6).
- [ ] **Fase 4:** entrega de eventos de IOHIDSystem en el kernel, el driver `ApplePS2Mouse` y el lector de entrada de WindowServer.
- [ ] **Fase 4:** instalar el job de WindowServer y lograr el primer arranque que dibuje en `IOGOPFramebuffer`.
- [ ] **Fase 3:** las clases de Foundation que más faltan según el informe de símbolos (`NSURLSession`), y lo que llegó de la Fase 2 (libSystem, stubs ruidosos, `logd`).
- [ ] **Fase 3:** los prototipos del toolchain Swift.

Además, lo que este plan añade y aún no está en el repo:

- [x] **Versiones (31 oct):** `PROD_VERSION = 19.6.0` para `ProductUserVisibleVersion`, `PROD_COMPAT_VERSION = 10.15` para `ProductVersion`, y `sw_vers` mostrando «Minkar 19.6.0 (kernel Darwin 19.6.0)», con su manual (`d2b559759f`). Probado en QEMU: `@available(10.15)` verdadero y `(11.0)` falso, `operatingSystemVersion` 10.15.0, `uname -r` 19.6.0. `libsystem_info` y el kext de pthread conservan sus propias versiones. En el mismo trabajo: `launch_activate_socket` (`5051c781b4`), `os_log` y `os_activity` (`0709544df7`), `_availability_version_check` (`c0923c1bc7`) y `NSProcessInfo` (`fb6e25b877`).
- [x] **`Docs/ROADMAP.md` al día (31 oct):** reescrito desde este plan, con Gienah y Kraz, el esquema API 19/20/21 → Minkar 19.6.0/20.0/21.0, el frente de GPU y el requisito de rendimiento.
- [ ] **Compatibilidad de versión:** mecanismo tipo `SystemVersionCompat.plist` y `SYSTEM_VERSION_COMPAT=1` para binarios enlazados con un SDK anterior.
- [ ] **Decidir la familia del primer driver de GPU** y escribir el ADR con el prototipo KMS. También hace falta para apagar la pantalla, que el framebuffer GOP no permite.
- [ ] **Medición:** medir el sistema de referencia en hardware original (Mac Pro 6,1) y guardar los valores fuera del repo público, para los gates de rendimiento de Minkar 21.0.
- [ ] **Rendimiento NVMe en QEMU:** las lecturas por el dispositivo de bloques van a 1 MB/s.

## Fuentes

- [Repositorio minkaros/minkar](https://github.com/minkaros/minkar), rama `devel`, verificada en `fb6e25b877` (31 oct 2026).
- [`Docs/ROADMAP.md`](https://github.com/minkaros/minkar/blob/devel/Docs/ROADMAP.md): estado de cada fase.
- [`Docs/Hardware.md`](https://github.com/minkaros/minkar/blob/devel/Docs/Hardware.md): equipos de referencia.
- [`Docs/Commands.md`](https://github.com/minkaros/minkar/blob/devel/Docs/Commands.md): comandos de Minkar frente a la lista de referencia.
- [`Docs/Userland.md`](https://github.com/minkaros/minkar/blob/devel/Docs/Userland.md): historial de comandos portados (exFAT, `notifyutil`, `launchctl kickstart`).

El estado sale del repositorio; las decisiones de arquitectura, los requisitos de rendimiento y la secuencia de la pila gráfica son propuestas de este plan, y las que se confirman se registran en `Docs/ROADMAP.md`.
