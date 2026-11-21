# ADR 0003: El salto del kernel a Darwin 25

Estado: **aceptado para x86-64**, 20 nov 2026, con dos condiciones:

1. El informe de licencias de corecrypto (7.6) queda resuelto por el mantenedor antes de empezar KX1.
   *Cumplida el 21 nov 2026 (sección 10).*
2. Las preguntas 7.3, 7.4 y 7.10 se cierran con los experimentos de KX1, como dice la sección 7.

Es el hito KX0 del carril KX de `Docs/ROADMAP.md` y por sí mismo no cambia nada del
kernel. La referencia es la versión en inglés, [0003-Kernel-Upgrade.md](0003-Kernel-Upgrade.md);
esta copia se actualiza en el mismo commit.

**Marco (decidido por el mantenedor, 20 nov 2026):**

- **Versión:** xnu-12377.121.6 (Darwin 25, distribución `macos-265`). Aprobada.
- **Alcance:** el salto de kernel se define y se valida en x86-64 (QEMU, PC 1 y PC 2).
  No depende de arm64.
- **Momento:** KX1 empieza después de publicar Minkar 19.6.0 y después del paso R1 de la
  reestructuración del proyecto (el toolchain como artefacto propio, con AArch64 en LLVM;
  [Structure.es.md](../Structure.es.md)).
- **Kernel Debug Kit:** no se usa nunca, ni para compilar ni como referencia.
- **Cadenas de versión:** hasta KX3, `devel` sigue en Darwin 19.6.0 y no cambia
  `kern.osrelease`, `uname -r`, `DARWIN_VERSION` ni `TARGET_TRIPLE`. El trabajo del kernel
  nuevo vive en su rama, donde esos valores pasan a los de Darwin 25. La primera versión
  sobre el kernel nuevo es Minkar 25.x; `ProductVersion` es independiente del kernel y sube
  por conjuntos de apps.

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
pmap propios de Minkar, escritos a partir del árbol publicado: ver la sección 8.

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

## 7. Decisiones sobre las preguntas abiertas

La numeración es la de las preguntas de la versión propuesta.

1. **Hardware arm64 de destino.** arm64 no se elimina y no se cambia de versión de kernel
   por él, pero su arranque queda fuera de este ADR y sin decidir: ver la sección 8. Sigue
   ARM0 (solo compilar). No está en alcance, por ahora, escribir una configuración de
   máquina arm64, una plataforma pexpert, un controlador de interrupciones ni un modo de
   pmap, y ARM1 no empieza.
2. **x86-64 después de Darwin 25.** Aceptado como consecuencia, no como riesgo abierto: si
   Darwin 26 deja de publicar x86-64, los PC x86-64 de Minkar se quedan en una rama de
   Darwin 25, que sería la última para esa arquitectura. La continuidad más allá de Darwin
   25 dependerá de arm64.
3. **El formato preenlazado antiguo.** No se decide sobre el papel: es el primer
   experimento de KX1, arrancar Darwin 25 con la salida actual de `plktool`. Si arranca, se
   conserva ese formato; si no, se escribe un constructor de colecciones fileset y se enseña
   al cargador EFI a cargarlas. En ambos casos el cargador pasa a la revisión 1 de
   `boot_args` (`KC_hdrs_vaddr` y los campos nuevos).
4. **kxld.** Minkar preenlaza todos sus kexts en el kernelcache, así que la carga en
   tiempo de ejecución no hace falta para ningún gate. En KX1 y KX2 todos los kexts van en
   la colección de arranque. Reactivar `config_kxld`, o construir una colección auxiliar,
   se decide después de KX2, y solo si aparece la necesidad.
5. **Headers ausentes** (CoreEntitlements V2, TrustCache, `CodeSignature/Entitlements.h`,
   image4 dlxk) y el proveedor de la interfaz de AMFI. Se aceptan sustitutos de sala
   limpia: escritos solo a partir de los puntos de llamada del xnu publicado, bajo
   BSD-2-Clause, cada uno con una nota de procedencia que cite los ficheros de xnu de los
   que se dedujo su interfaz. Sin ninguna otra fuente.
6. **Licencia de corecrypto.** Prioritaria e independiente del salto de kernel, porque
   afecta al árbol público de hoy. Hay un informe sobre su origen y la licencia de cada
   fichero (20 nov 2026); corecrypto no se cambia, ni se lleva a la tabla de funciones de
   Darwin 25, hasta que el mantenedor decida con él. **KX1 no empieza antes.**
7. **Texto de licencia de hfs.** Manda la cabecera de cada fichero. `Kernel/_PROVENANCE` y
   `NOTICE` dicen ya lo que dicen las cabeceras (20 nov 2026): el kext es APSL 2.0;
   `hfs_util` y `hfs_japanese` son APSL 1.2; el `APPLE_LICENSE` de hfs es el texto de la
   APSL 1.2, y no se toca. Los ficheros bajo una APSL anterior a la 2.0, en hfs y en las
   familias antiguas, se listan para el mantenedor (sección 10).
8. **Procedencia desfasada.** `Libraries/_PROVENANCE` corregido (20 nov 2026): dyld-750.6
   y la etiqueta real de Darwin 19 de cada componente.
9. **El userland sube con el kernel.** dyld, libsyscall, Libc, libplatform, libpthread
   (biblioteca y kext), libmalloc, libdispatch, objc4, Libinfo, Libnotify, IOKitUser,
   kext_tools y configd suben a sus versiones de Darwin 25 junto con el kernel.
   launchd-842 y CF-Lite no tienen versión de Darwin 25: se quedan y se adaptan. Comprobar
   launchd contra el libxpc y las interfaces de arranque de Darwin 25 es parte de KX1.
   libunwind, libc++ y compiler-rt siguen saliendo del toolchain del árbol.
10. **El mínimo del gate M2.** Se determina en KX1 con arranques de prueba, quitando kexts
    de uno en uno. La columna «KX1» de la sección 5 es el punto de partida.
11. **Política de versión del kernel.** Resuelta por el marco de arriba.
12. **Toolchain.** Una prueba de solo compilación del árbol x86-64 de xnu-12377.121.6 con
    el clang del árbol, fuera del árbol, es parte de KX0: sección 9.

Cuando llegue arm64: clang pone `minos 11.0` en los objetos arm64 por sí mismo, así que
dyld y las comprobaciones de carga deberán aceptarlo mientras `ProductVersion` sea 10.15.

## 8. Destino de arranque arm64 (abierto)

Sin decidir y fuera del alcance de este ADR. Los hechos (20 nov 2026, solo de las fuentes
publicadas):

- El kernel arm64 publicado solo compila para los SoC del fabricante (`T6000` a `T8142`) y
  `VMAPPLE`. Para cualquiera de ellos el enlace carga a la fuerza
  `lib<máquina>.os.<config>.a`, que solo viene en el Kernel Debug Kit, cerrado; SPTM, TXM y
  Exclaves necesitan bibliotecas y headers cerrados. No hay máquina arm64 genérica ni QEMU
  `virt`.

Los dos candidatos, sin recomendar ninguno:

| Candidato | Qué exige | Dónde corre |
| --- | --- | --- |
| VMAPPLE | Sustituir `lib<máquina>.os.<config>.a` (solo en el Kernel Debug Kit) y un cargador de arranque propio; VMAPPLE no está en la lista de SoC con SPTM, así que usaría el pmap PPL | Solo en un anfitrión con procesadores arm64 del fabricante: ni en el servidor de compilación ni en la CI |
| Plataforma propia para QEMU `virt` | Una plataforma arm64 nueva dentro de XNU (configuración de máquina, pexpert, controlador de interrupciones, modo de pmap) | En el servidor de compilación y en la CI; no depende de nada cerrado |

Investigación para hacer junto con KX1, no ahora: medir el tamaño real de esa biblioteca
enlazando el kernel arm64 de VMAPPLE sin ella y listando los símbolos sin definir (solo
desde las fuentes publicadas), y determinar si el pmap PPL, o uno sin monitor, funciona
fuera de los núcleos del fabricante.

## 9. Prueba del toolchain (KX0)

Una prueba de solo compilación del kernel x86-64 RELEASE de xnu-12377.121.6 con el clang
21.1.6 del árbol, fuera del árbol (20 nov 2026): los cambios de Minkar a los ficheros de
build para Linux trasladados a los nuevos `SETUP/` y `makedefs/` (14 ficheros), `setup` y
`exporthdrs` ejecutados, y después cada objeto compilado con `gmake -k`. No se enlazó ni
se arrancó nada.

- **Compilan 685 de 1085 objetos (63 %).** Con un sustituto de tres builtins del
  compilador del fabricante (con semántica falsa a propósito, solo para ver qué hay
  detrás), **1016 (93,6 %)**.

| Componente | Objetos | Compilan | Builtins del fabricante | Headers ausentes | Generadores | Otros |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| osfmk | 369 | 300 | 54 | 15 | — | 0 |
| bsd | 501 | 276 | 198 | 26 | — | 1 |
| libkern | 76 | 53 | 22 | 1 | 1 | 1 |
| iokit | 105 | 25 | 78 | 2 | 22 | 0 |
| pexpert | 10 | 9 | 1 | 0 | — | 0 |
| libsa | 1 | 0 | 1 | 0 | — | 0 |
| security | 23 | 22 | 1 | 0 | — | 0 |

(Los fallos de generadores se cuentan tras el sustituto; en la primera pasada quedan
ocultos detrás de los builtins.)

Qué falla, y qué necesita una compilación completa:

1. **Los builtins de tipos del compilador del fabricante** (355 objetos):
   `__builtin_xnu_type_signature`, `__builtin_xnu_type_summary` y
   `__builtin_xnu_types_compatible`, que usan `kalloc.h` e `IOLib.h` sin alternativa.
   Ninguna *opción* exclusiva del compilador del fabricante rompió nada en x86-64 (allí
   están apagados los límites seguros y la autenticación de punteros).
2. **Headers ausentes de las fuentes publicadas** (44 objetos): `TrustCache/API.h` (38, a
   través de `osfmk/vm/pmap_cs.h`, en la VM, el cargador y los sistemas de archivos),
   `CoreEntitlements/V2/API.h` (5), `CodeSignature/Entitlements.h` (1). Son los sustitutos
   de sala limpia de 7.5.
3. **Generadores:** el MIG del árbol no conoce `-mach_msg2` (sin esa opción los stubs de
   osfmk compilan, pero con el formato de mensaje antiguo); el IIG del árbol deja los
   `.iig` sin traducir (23 objetos).
4. **Otros** (2 objetos): el `os/firehose_buffer_private.h` del SDK es el antiguo; el de
   libdispatch de Darwin 25 es el que necesita xnu.

El informe, su parche de ficheros de build y los resultados por objeto se guardan fuera
del repositorio, con los demás informes del carril. No se comprobaron el enlace, CTF, las
exportaciones de los kexts ni las configuraciones DEBUG y DEVELOPMENT.

## 10. Preguntas nuevas

Aparecieron al redactar esta versión. Ninguna se responde aquí; cada una lleva una
recomendación.

1. **Código GPL-3 en el kext de corecrypto.** El informe de corecrypto encontró 13 headers
   idénticos a los del corecrypto de Darling (GPL-3); uno, `ccrsa.h`, se compila en el kext
   que va en la imagen, y los otros 12 no se usan. *Recomendación:* decidirlo con el informe
   de corecrypto, antes de KX1 e independientemente de él: que el kext use el
   `EXTERNAL_HEADERS/corecrypto/ccrsa.h` publicado del propio xnu y retirar los que no se
   usan.
2. **Ficheros APSL 1.1 que se compilan hoy.** 45 fuentes bajo la APSL 1.1, en AppleAPIC,
   AppleI386PCI, AppleIntelPIIXATA, ApplePS2Controller, IOACPIFamily e IOPCIFamily, se
   compilan ahora en la imagen; la regla es que la APSL 1.1 no se incorpora a Minkar.
   *Recomendación:* listarlos como sustituciones pendientes; en KX, tomar IOPCIFamily de la
   versión de Darwin 25 si allí sus ficheros son APSL 2.0 (sin comprobar aún), y sustituir los kexts antiguos de
   PC (APIC, puente PCI, PS/2) por otros propios de Minkar; quitar AppleIntelPIIXATA de la
   imagen si ninguna máquina soportada lo necesita.
3. **APSL 1.2 en hfs_util.** `/sbin/hfs.util` se compila de dos ficheros APSL 1.2.
   *Recomendación:* sustituirlo por el hfs_util de la versión de hfs que adopte KX, tras
   comprobar sus cabeceras.
4. **Los builtins de tipos.** *Recomendación:* para KX1, una alternativa en los headers
   (sin el aislamiento por tipos de kalloc); después, implementar los tres builtins en el
   clang del árbol, en sala limpia, a partir de sus usos en el xnu publicado.
5. **MIG e IIG.** *Recomendación:* tomar MIG de la versión publicada que conozca
   `-mach_msg2` (bootstrap_cmds), si la de Darwin 25 está publicada; para IIG, averiguar de
   dónde salieron las fuentes de DriverKit pregeneradas del árbol (`10f5a67837`) antes de
   decidir entre salidas generadas y un generador de sala limpia.
6. **Headers de firehose.** *Recomendación:* instalar en el SDK los headers de firehose de
   la libdispatch de Darwin 25 cuando libdispatch suba (7.9).

**Decidido por el mantenedor el 21 nov 2026:**

1. corecrypto: un `ccrsa.h` de sala limpia y los 13 headers GPL-3 retirados
   (`433d7cc1ee`); los catorce ficheros sin concesión de licencia sustituidos por
   resúmenes y AES propios de Minkar, y los headers del fabricante del kext otra vez
   idénticos a los publicados de xnu (`d21db0b446`, `cd552fcb3a`). Con esto se cumple
   la condición 1 de este ADR.
2. Los ficheros APSL 1.1 se quedan por ahora, listados en `NOTICE`, y se sustituyen en KX
   (IOPCIFamily de Darwin 25 si allí es APSL 2.0; los kexts de APIC, puente PCI y PS/2 por
   otros propios). AppleIntelPIIXATA se mantiene hasta KX.
3. `hfs_util` se sustituye en KX por el de la versión de hfs que adopte KX.
4. Una alternativa en los headers para KX1, y después los tres builtins en el clang del
   árbol, en sala limpia.
5. MIG de la versión publicada de Darwin 25; para IIG, averiguar primero de dónde salieron
   las fuentes de DriverKit pregeneradas del árbol (`10f5a67837`).
6. Los headers de firehose suben con libdispatch en KX.

## Consecuencias

- KX1 empieza después de publicar Minkar 19.6.0, después de R1, y cuando el mantenedor haya
  decidido sobre el informe de corecrypto; en una rama, con los kexts marcados «KX1» en la
  sección 5, y con 7.3, 7.4 y 7.10 como sus experimentos.
- El arranque de arm64 queda fuera de este ADR (sección 8); ARM0, solo compilar, sigue.
- Hasta KX3, `devel` compila sobre Darwin 19.6.0, conserva sus cadenas de versión, y el
  código nuevo de frameworks y escritorio no debe depender de interfaces que solo tenga uno
  de los dos kernels.
- Si Darwin 26 deja x86-64, Minkar en x86-64 se queda en una rama de Darwin 25 (7.2).
