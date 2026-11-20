# ADR 0003: El salto del kernel a Darwin 25

Estado: propuesto, 20 nov 2026. La versión está decidida (decisión D5 del
roadmap, 20 nov 2026: Minkar salta a Darwin 25, después de publicar Minkar
19.6.0); este ADR dice cómo. Es el hito KX0 del carril KX de
`Docs/ROADMAP.md` y por sí mismo no cambia nada del kernel. La referencia
es la versión en inglés, [0003-Kernel-Upgrade.md](0003-Kernel-Upgrade.md);
esta copia se actualiza en el mismo commit.

## 1. Contexto

- Minkar corre sobre Darwin 19.6.0 (xnu-6153.141.1), solo en x86-64. Su
  objetivo desde el 20 nov 2026 es ejecutar los binarios publicados de las
  apps de la plataforma de referencia, un nivel de API por detrás del
  actual, y llegar a arm64 (decisiones D1, D4 y D5).
- El xnu de Darwin 19 no tiene una plataforma arm64 que pueda arrancar
  fuera de las máquinas del fabricante, y libpthread, libdispatch y dyld
  modernos esperan interfaces del kernel que no tiene. D5 sustituye a la
  regla anterior de mantener Darwin 19.6.0 para las API 20 y 21.
- Minkar 19.6.0 y sus correcciones 19.6.x siguen sobre Darwin 19.6.0. El
  salto es el carril KX: un prototipo en una rama (KX1), el sistema
  completo (KX2) y después `devel` sobre Darwin 25 con Darwin 19 en una
  rama de mantenimiento (KX3, Minkar 25.x). Hasta KX3, lo nuevo de
  frameworks y escritorio debe compilar sobre ambos kernels.
- Sala limpia (principio 5): solo se lee el código abierto publicado por
  el fabricante (`github.com/apple-oss-distributions`) y otro código
  abierto. Ni SDK, ni Kernel Debug Kit, ni binarios, ni desensamblados.

## 2. La versión exacta

**xnu-12377.121.6**, el último kernel de Darwin 25 publicado, que lista la
etiqueta `macos-265` de `distribution-macOS` (26.5, 17 jun 2026).

Cómo se comprobó (20 nov 2026):

- `git ls-remote --tags https://github.com/apple-oss-distributions/xnu`
  lista la serie `xnu-12377.*`. Cada etiqueta de `distribution-macOS` de
  `macos-260` a `macos-265` se clonó en superficie y su gitlink `xnu` se
  resolvió a una etiqueta: 26.0 → xnu-12377.1.9, 26.1 → .41.6,
  26.2 → .61.12, 26.3 → .81.4, 26.4 → .101.15, 26.5 → .121.6. `macos-265`
  es la más reciente.
- El árbol en `xnu-12377.121.6` declara las dos arquitecturas:
  `osfmk/x86_64` y `osfmk/arm64`; `config/MASTER.x86_64`, `MASTER.arm64` y
  `MASTER.arm64.MacOSX` con sus listas de exportación;
  `SUPPORTED_ARCH_CONFIGS := X86_64 X86_64H ARM64` en
  `makedefs/MakeInc.def`.

**Lo que esto no resuelve para arm64.** La parte arm64 publicada solo
compila para los SoC del propio fabricante (`T6000` a `T8142`) y
`VMAPPLE`; no hay una máquina arm64 genérica ni QEMU `virt`. Para
cualquiera de esas configuraciones el enlace del kernel carga a la fuerza
una biblioteca de soporte que solo viene en el Kernel Debug Kit, que es
cerrado, y SPTM, TXM y Exclaves necesitan bibliotecas y headers cerrados
(`sptm/*`, `Tightbeam/*`, `xnuproxy/*`). La compilación x86-64 no necesita
ninguno. Así, Darwin 25 se puede usar como está decidido para x86-64 (KX1
a KX3), pero arrancarlo en arm64 (ARM1) necesita una configuración de
máquina, un platform expert, un controlador de interrupciones y un modo de
pmap propios de Minkar, escritos a partir del árbol publicado: ver 7.1.

## 3. Qué sube con el kernel

La base que Minkar toma hoy de Darwin 19, frente a la distribución de
Darwin 25 (`macos-265`). Las versiones de Minkar se comprobaron comparando
sus ficheros con cada etiqueta de origen.

| Componente | En Minkar hoy | Darwin 25 | Licencia |
| --- | --- | --- | --- |
| xnu (y libsyscall) | xnu-6153.141.1 | xnu-12377.121.6 | APSL-2.0 |
| dyld | dyld-750.6 | dyld-1378 | APSL-2.0 |
| Libc | Libc-1353.100.2 | Libc-1752.120.2 | APSL-2.0, ficheros BSD |
| libplatform | libplatform-220.100.1 | libplatform-375.120.2 | APSL-2.0 |
| libpthread (biblioteca y kext) | libpthread-416.100.3 | libpthread-539.100.4 | APSL-2.0 |
| libdispatch | libdispatch-1173.100.2 | libdispatch-1542.100.32 | Apache-2.0 |
| objc4 | objc4-787.1 | objc4-951.7 | APSL-2.0 |
| libmalloc | libmalloc-283.100.6 | libmalloc-812.100.31 | APSL-2.0 |
| Libsystem | Libsystem-1281.100.1 | Libsystem-1356 | APSL-2.0 |
| Libinfo | Libinfo-538 | Libinfo-600 | APSL-2.0 |
| Libnotify | Libnotify-241.100.2 | Libnotify-348.120.4 | APSL-2.0 |
| IOKitUser | IOKitUser-1726.140.1 | IOKitUser-100231.120.3 | APSL-2.0 |
| kext_tools | kext_tools-623.120.1 | kext_tools-779 | APSL-2.0 |
| configd | configd-1061.141.1 | configd-1405.120.5 | APSL-2.0 |
| ICU | ICU 78.3 con las extensiones de la plataforma | ICU-76142.5.1.200 | Unicode-3.0 |
| hfs | hfs-556.60.1 (el de Darwin 20, adaptado) | hfs-715.120.4 | APSL |
| IOPCIFamily | IOPCIFamily-370.141.1 | IOPCIFamily-726.100.6 | APSL-1.1, APSL-2.0 |
| IOHIDFamily | IOHIDFamily-1446.140.2 | IOHIDFamily-2238.120.5 | APSL-2.0 |
| IOGraphics | IOGraphics-576.1 | IOGraphics-600 | APSL-2.0 |
| IOStorageFamily | IOStorageFamily-238.120.1 | IOStorageFamily-337.100.1 | APSL-2.0 |
| IOATAFamily | IOATAFamily-256 | IOATAFamily-264 | APSL-2.0 |
| msdosfs | msdosfs-411.100.3 | msdosfs-788.100.31 | APSL-2.0 |
| AppleFileSystemDriver | AppleFileSystemDriver-23 | AppleFileSystemDriver-31 | APSL-2.0 |
| CommonCrypto | dentro de Libsystem | CommonCrypto-600035 | APSL-2.0 |
| AvailabilityVersions | (headers generados) | AvailabilityVersions-157.2, necesario para compilar xnu | APSL-2.0 |

**Sin versión publicada de Darwin 25:**

| Componente | En Minkar hoy | Nota |
| --- | --- | --- |
| launchd | launchd-842 | La última publicada es launchd-842.92.1 |
| CoreFoundation | CF-Lite, CF-1153.18 ampliado | La última publicada es CF-1153.18 |
| libclosure | no se usa: `libsystem_blocks` es el BlocksRuntime de swift-corelibs (Apache-2.0) | libclosure-96 está publicado, si se quiere |
| libunwind, libc++, compiler-rt | del toolchain (LLVM 21.1.6 de swiftlang) | El fabricante no los publica; el toolchain los sigue dando |
| corecrypto | el corecrypto de PureDarwin | No publicado; su licencia es una pregunta abierta (7.6) |
| IOUSBFamily, AppleAPIC, IOACPIFamily, AppleI386PCI, AppleIntelPIIXATA, ApplePS2Controller | versiones antiguas | Sus últimas versiones publicadas; Minkar los sigue manteniendo |
| IOAudioFamily, IONetworkingFamily, exfat | propios de Minkar (BSD-2) | No les afectan las versiones del fabricante |

## 4. Qué falta en las fuentes publicadas

Encontrado leyendo el árbol completo de `xnu-12377.121.6` y la lista de
la distribución. Cada punto dice lo que Minkar tendría que escribir.

1. **Colecciones del kernel.** Darwin 25 arranca de una colección de
   kernel de tipo fileset hecha con `kmutil`, que no está publicado (ni
   KernelManagement ni kernelmanagerd). El código x86-64 aún reconoce el
   kernel preenlazado antiguo, sin fileset, que es el formato que escribe
   el `plktool` de Minkar y carga su cargador EFI, pero no se sabe si
   sigue arrancando de principio a fin. Minkar no escribiría nada (si el
   formato antiguo arranca) o escribiría un constructor de colecciones
   fileset, y enseñaría al cargador a rellenar `KC_hdrs_vaddr`.
2. **Carga de kexts en marcha.** `config_kxld` ya no está en
   `MASTER.x86_64` (el código sigue en `libkern/kxld`). Sin él, un kext que
   no está en la colección de arranque solo se carga desde una colección
   auxiliar. Minkar reactivaría kxld, si aún compila, o escribiría un
   constructor de colecciones auxiliares.
3. **Argumentos de arranque.** `boot_args` versión 2 gana la revisión 1:
   `KC_hdrs_vaddr` y campos de hash de la raíz. Se actualiza la estructura
   del cargador EFI.
4. **Headers privados.** Los incluyen ficheros a los que llega una
   compilación x86-64 y no están publicados: `CoreEntitlements/V2` (que
   `amfi.h` incluye sin condiciones), `TrustCache/API.h`,
   `CodeSignature/Entitlements.h` e `image4/dlxk/interface.h`. Minkar
   escribiría headers de sala limpia, y las implementaciones a las que se
   llame, solo a partir de los puntos de llamada del árbol publicado.
5. **Entitlements y firma de código.** Darwin 25 consulta una interfaz de
   AMFI (`amfi_interface_register`, CoreEntitlements) que normalmente da
   el kext AMFI, cerrado. Minkar daría esa interfaz desde
   MinkarCodeIntegrity (ADR 0001).
6. **Políticas MAC.** La tabla de operaciones de política pasa de la
   versión 62 a la 93: MinkarCodeIntegrity y MinkarSandbox se portan a
   ella.
7. **corecrypto.** El kernel sigue enlazando `libcc_kext` y registra una
   tabla de funciones criptográficas que cambió; corecrypto no está
   publicado, así que el kext corecrypto de Minkar se actualiza a la tabla
   nueva.
8. **Kexts cerrados en todas las versiones**, ya sustituidos en Darwin 19
   y que hay que seguir sustituyendo: la plataforma ACPI, AppleSMC,
   AppleKeyStore, AppleImage4, AMFI, el kext del sandbox,
   AppleFSCompression y APFS.

## 5. Los kexts de Minkar

Hoy todos los kexts se preenlazan en un único kernelcache
(`Kernel/Extensions/Makefile`, `plktool`). Las líneas son `wc -l` de las
fuentes de cada kext (C, C++, headers, ensamblador; incluyen código
importado). Los «símbolos que faltan» son los imports del kext que no
están en ninguna lista de exportación de Darwin 25 (solo nombres,
comprobados contra `config/*.exports` de `xnu-12377.121.6`; las
disposiciones no se comprueban). «KX1» marca lo mínimo para llegar al gate
M2 en QEMU.

| Kext | Líneas | Depende de | Símbolos que faltan | Qué cambia en Darwin 25 | KX1 |
| --- | ---: | --- | --- | --- | --- |
| IOACPIFamily (con la plataforma genérica) | 144.471 | kpi, IOPCIFamily, AppleAPIC, ApplePS2Controller | 3: `IOInterruptController::_RESERVED*` | Los huecos reservados de IOInterruptController ya se usan | sí |
| AppleAPIC | 1.549 | kpi | los mismos 3 | Como arriba | sí |
| IOPCIFamily | 23.592 | kpi | los mismos 3 | La versión de origen añade PCIDriverKit y VT-d | sí |
| AppleI386PCI | 1.422 | IOPCIFamily | 0 | — | sí |
| ApplePS2Controller | 3.370 | IOHIDFamily | 0 | — | sí (IOACPIFamily lo enlaza) |
| corecrypto | 11.733 | kpi | 0 | La tabla de funciones criptográficas | sí |
| pthread | 4.466 | kpi, corecrypto | 0 | Lo sustituye el kext de libpthread-539, con los shims del kernel | sí |
| IOStorageFamily | 22.088 | kpi | 0 | La versión de origen añade una parte DriverKit | sí |
| MinkarAHCIPort | 1.993 | IOStorageFamily, IOPCIFamily | 0 | — | sí |
| MinkarNVMe | 1.283 | IOStorageFamily, IOPCIFamily | 0 | — | con `--disk nvme` |
| hfs | 161.988 | kpi | 2: `nspace_snapshot_event`, `resolve_nspace_item` | El resolvedor de espacios de nombres ya no existe; hfs-715 está publicado | sí |
| IONetworkingFamily (propio) | 4.834 | kpi | 0 | Los KPI antiguos de ifnet siguen exportados | sí |
| MinkarE1000 | 38.517 | IONetworkingFamily, IOPCIFamily | 0 | — | sí |
| IOHIDFamily | 198.039 | kpi | 0 | La versión de origen añade HIDDriverKit | sí |
| MinkarCodeIntegrity | 529 | kpi | 0 | Tabla MAC v93, la interfaz de AMFI | sí |
| AppleFileSystemDriver | 565 | IOStorageFamily | 0 | — | abierto (7.10) |
| MinkarFSCompression | 510 | kpi | 0 | Revisar el KPI de decmpfs | abierto (7.10) |
| MinkarSandbox | 574 | kpi | 0 | Tabla MAC v93 | abierto (7.10) |
| msdosfs | 37.756 | kpi | 0 | La versión de origen añade un módulo FSKit | abierto (7.10) |
| IOGraphics (IOGOPFramebuffer) | 45.630 | IOPCIFamily | 0 | — | no |
| MinkarVirtio | 2.729 | IOStorageFamily, IONetworkingFamily, IOGOPFramebuffer, IOPCIFamily | 0 | — | no |
| MinkarDRM | 33.808 | IOGOPFramebuffer, IOPCIFamily | 0 | — | no |
| IOATAFamily | 9.776 | kpi | 0 | — | no |
| AppleIntelPIIXATA | 4.585 | IOATAFamily, IOPCIFamily | 0 | — | no |
| exfat (propio) | 3.625 | kpi | 0 | — | no |
| MinkarDiskImage | 1.278 | IOStorageFamily | 0 | — | no |
| IOUSBFamily | 8.763 | kpi | 0 | — | no |
| MinkarXHCIPort | 3.675 | IOUSBFamily, IOHIDFamily, IOStorageFamily, IOPCIFamily | 0 | — | no |
| MinkarUSB | 1.566 | IOUSBFamily, IOHIDFamily | 0 | — | no |
| MinkarBGE | 4.999 | IONetworkingFamily, IOPCIFamily | 0 | — | no |
| IOAudioFamily (propio) | 1.956 | kpi | 0 | — | no |
| MinkarHDA | 1.737 | IOAudioFamily, IOPCIFamily | 0 | — | no |

La deriva de símbolos es pequeña (8 nombres, 3 causas). Los cambios
mayores están en las disposiciones y en el modelo de carga: las tablas MAC
y criptográfica, los shims de pthread, los huecos reservados de
OSMetaClass, y colecciones de arranque y auxiliares en lugar de kxld.

## 6. Los parches de Minkar al kernel actual

`git log -- Kernel/xnu` tiene 101 commits (2 nov 2025 a 16 nov 2026).
Frente a xnu-6153.141.1 de origen hay 164 ficheros cambiados y 37
añadidos; la mayoría de las líneas cambiadas son un
`AvailabilityInternal.h` regenerado, y los cambios de código del kernel
son unos pocos miles de líneas. El cargador EFI tiene 10 parches en
`Image/patches`. Por grupos, con los commits que los traen:

| Grupo | Commits | En Darwin 25 |
| --- | --- | --- |
| Compilar en Linux con el toolchain del árbol | `88bd140dde`, `6f2bbe85e3`, `4200306f93`, `51cd14312f`, `0d987b5f5c`, `9db0205544`, `3f619c9dfe`, `10f5a67837` | Se rehace sobre los makefiles nuevos, no se traslada línea a línea |
| Entrada desde UEFI, modo largo | `9b383d1974`, `d21f786ecc`, `88ada6d898` | Se traslada |
| Kexts preenlazados y su arranque | `15f5d2b8a3`, `be65f9afa3`, `f6dc603d3e`, `5e0658b615`, `476276b734`, `5540b4abf7`, `cd28566bfe`, `2d8de3d75d`, `cd96179c26`, `57c6d5734b`, `b80c77c673` | Depende del formato de colección (4.1, 4.2) |
| Plataforma PC y procesadores (AMD, CPU híbridas, KVM, MADT, IO-APIC, topología) | `97ceca6300`, `e5e8bdd0b7`, `b75d445e8d`, `8298b20fdf`, `bb0db8f21f`, `5345cde8e2`, `c858199650`, `de2b1998d8`, `4a3ac92b47`, `97e03bd7f0` | Se traslada, tras ver cuáles ya están en origen |
| Consola (vídeo y serie) | `a547964e38`, `60ae142c5a`, `5550839382`, `288c5dad4f`, `917a98c0c1`, `c0de370b52`, `78a269266b` | Se traslada |
| Correcciones BSD y POSIX | `febbeed8ad`, `8cb55a33ea`, `1c82773307`, `b717c9c263`, `854fb2e231`, `19bee3dab6`, `10f482285b`, `efa0bdf1e3`, `2b0e88aef7`, `3035b06ad9`, `788965f768`, `39dcee2291` | Cada una se comprueba antes contra Darwin 25 |
| Sistemas de archivos y volumen raíz | `aa6e66549c`, `54e969ab06` | Se traslada |
| os_log | `529bae7db4`, `f3de82c1c7`, `0709544df7`, `aca373f14f` | Se comprueba contra el registro de Darwin 25 |
| Firma de código y corecrypto | `39e9807e2c`, `e716c9e478`, `82b86ab6a4` | Lo sustituyen 4.4 a 4.7 |
| Nombres y licencias | `fbe3410dae`, `da1633edf3`, `a0874f9817`, `35a7fe13ec` | Se traslada |

## 7. Riesgos y preguntas abiertas

Para el mantenedor. Ninguna se decide aquí.

1. **Objetivo arm64.** El kernel arm64 publicado solo compila para los SoC
   del fabricante y `VMAPPLE`, con una biblioteca de soporte cerrada.
   ¿Entra en ARM1 escribir una configuración de máquina, un platform
   expert, un controlador de interrupciones y un modo de pmap (sin SPTM ni
   PPL) propios de Minkar, y en qué máquina: QEMU `virt` primero? No se
   sabe si PPL, o un pmap sin monitor, funciona en núcleos que no sean del
   fabricante.
2. **x86-64 después de Darwin 25.** El fabricante dijo públicamente en
   junio de 2025 que su versión 26.x es la última para sus equipos Intel.
   Si Darwin 26 quita `osfmk/x86_64`, Minkar en x86-64 se queda en Darwin
   25. ¿Se acepta?
3. **El formato preenlazado antiguo en Darwin 25.** ¿Sigue arrancando lo
   que escribe `plktool`, o hace falta un constructor de colecciones
   fileset? Lo responde un arranque de prueba (KX1).
4. **kxld.** ¿Reactivar `config_kxld` en x86-64, o pasar a colecciones
   auxiliares?
5. **Headers de sala limpia.** ¿Son aceptables sustitutos de
   CoreEntitlements V2, TrustCache, los entitlements de CodeSignature e
   image4 dlxk, escritos desde los puntos de llamada del árbol publicado?
6. **Licencia de corecrypto.** El corecrypto de Minkar (de PureDarwin)
   dice «All rights reserved» sin conceder licencia, y `Kernel/_PROVENANCE`
   no lista ninguna. Conviene confirmar su origen y licencia antes de
   cambiarlo para la tabla nueva.
7. **Texto de licencia de hfs.** Su fichero `APPLE_LICENSE` es el texto de
   la APSL 1.2, mientras que `Kernel/_PROVENANCE` dice APSL-2.0.
8. **Procedencia desfasada.** `Libraries/_PROVENANCE` dice dyld-1066.8,
   pero el árbol es dyld-750.6 (`47603124d4`), y nombra una rama de la que
   no vienen los componentes de Libsystem.
9. **El userland con el kernel.** ¿Pasan dyld, libsyscall, Libc,
   libpthread, libplatform y libmalloc a Darwin 25 junto con el kernel
   (números de llamadas al sistema, commpage, shims de pthread, workqueues)?
   launchd-842 y CF-1153 no tienen versión de Darwin 25; no se comprobó
   cómo encajan el bootstrap y el XPC de launchd con el libxpc de Darwin 25.
10. **El mínimo del gate M2.** ¿Hacen falta AppleFileSystemDriver,
    MinkarFSCompression, MinkarSandbox y msdosfs para M2? Hoy todos los
    kexts se preenlazan juntos; un arranque de prueba sin ellos lo
    responde.
11. **Cadenas de versión.** `uname -r`, `kern.osrelease`,
    `DARWIN_VERSION` y `TARGET_TRIPLE` (`x86_64-apple-darwin19`) cambian con
    el kernel en KX3, y con ellos todas las reglas que suponían Darwin
    19.6.0. ¿Qué valor toman en la rama de KX antes de KX3?
12. **Toolchain.** La compilación de Darwin 25 sondea opciones del
    compilador del fabricante (`-fbounds-safety`, desactivada en x86-64);
    no se comprobó si el LLVM 21.1.6 del árbol compila todo el kernel sin
    otras.

## Consecuencias

- KX1 empieza después de Minkar 19.6.0, en una rama, con los kexts
  marcados «KX1» arriba y con respuesta a 7.3, 7.4, 7.5, 7.9 y 7.10.
- ARM1 no es solo portar lo publicado: necesita una plataforma arm64
  propia de Minkar (7.1).
- Hasta KX3, `devel` compila sobre Darwin 19.6.0, y el código nuevo de
  frameworks y escritorio no debe depender de interfaces que solo tenga
  uno de los dos kernels.
