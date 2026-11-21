# Estructura del proyecto Minkar

Estado: **propuesto**, 20 nov 2026. La decisión es del mantenedor (20 nov 2026); este
documento dice cómo se haría cada paso, con los números del inventario hecho ese mismo día
(paso R0). No se ha movido nada. La referencia es la versión en inglés,
[Structure.md](Structure.md); esta copia se actualiza en el mismo commit.

## 1. La decisión

**El núcleo del sistema sigue en un solo repositorio y no se divide:** Kernel, Libraries,
Frameworks, WindowServer, los daemons de CoreServices, Dock, Files, loginwindow y
SystemUIServer. Cambian juntos, y los gates arrancan la imagen completa: un cambio del
kernel, de un framework o de la sesión solo se comprueba cuando todos están en la misma
imagen.

**Sale del árbol lo que no evoluciona con el núcleo**, en este orden y en estos momentos:

| Paso | Qué | Cuándo | Por qué entonces |
| --- | --- | --- | --- |
| R0 | El inventario y este documento. No se mueve nada | Ya (20 nov 2026) | No toca el build |
| R1 | El toolchain (`Developer/Default.xctoolchain`) a su propio repositorio; el árbol lo usa como artefacto precompilado fijado por SHA-256. En el mismo paso, AArch64 entra en los destinos de LLVM | Justo después de publicar 19.6.0, antes de KX1 | Hay que reconstruirlo de todos modos para arm64, y el kernel nuevo empieza sobre la estructura ya limpia |
| R2 | Las fuentes de terceros de BSD pasan a descargas fijadas más un directorio de parches, a través de `minkar.extlib.mk` | Durante 19.6.x, un componente por commit | Cambio mecánico y reversible, sin riesgo para la primera versión |
| R3 | La CI publica cada capa ya compilada (toolchain, kernel y kexts, sysroot base), para que un cambio compile solo su capa | Durante 19.6.x, después de R1 | Depende de que el toolchain ya sea un artefacto |
| R4 | Las apps de Atlas (Terminal, Text Editor, System Monitor, Logs, y A5–A10 según lleguen) a su propio repositorio, compiladas contra el `Minkar.sdk` publicado | Después de 19.6.0, con el SDK publicado | Necesita un SDK publicado y estable |

**Antes de publicar 19.6.0 no se hace nada de R1–R4.** Mientras tanto, arm64 en la CI
(ARM0) usa el clang de la versión de swift.org que el build ya descarga, con la capa de
headers generada desde `Kernel/xnu`; AArch64 en el LLVM propio del árbol llega con R1.

**Reglas:**

- Un paso cada vez; cada paso es un commit reversible, y después de cada uno pasan los
  gates y las pruebas de lote.
- No se reescribe el historial. Un repositorio nuevo recibe su historial con
  `git subtree split`; en el repositorio principal el directorio se quita con un commit
  normal.
- Las licencias, las líneas de copyright y las entradas de `NOTICE` viajan con el código.
- Los repositorios los crea el mantenedor.
- Cada paso R empieza solo cuando el mantenedor lo confirma.

## 2. Qué encontró el inventario (R0)

Del inventario del 20 nov 2026 (solo ficheros con seguimiento, comparados con su origen
por hash de blob cuando fue posible; el informe se guarda fuera del repositorio):

| Clase | Ficheros | Tamaño |
| --- | ---: | ---: |
| Propio de Minkar | 4.248 (1,7 %) | 60 MB (1,0 %) |
| De terceros, sin modificar | 215.871 (88,3 %) | 2.762 MB (45,3 %) |
| De terceros, modificado | 1.058 (0,4 %) | 15 MB (0,3 %) |
| De terceros, sin verificar | 9.370 (3,8 %) | 201 MB (3,3 %) |
| Generado | 13.981 (5,7 %) | 3.058 MB (50,2 %) |
| Total | 244.528 | 6.096 MB |

- **El toolchain es la mayor parte del árbol:** `Developer/Default.xctoolchain` tiene
  151.677 ficheros (62 %) y 1.716 MB. Su LLVM es el `swift-6.4.2-RELEASE` de swiftlang en
  147.681 de 147.885 ficheros; los cambios de Minkar son 14 ficheros (124 líneas), un
  fichero añadido y los ficheros de build.
- **La mitad de los bytes es salida de compilación subida por error:**
  `BSD/lib/ncurses/obj_s` tiene 10.786 volcados del compilador (3,03 GB), de antes del fork;
  las fuentes de ncurses en sí están sin modificar. Se quitaron el 21 nov 2026
  (`723a0f77ba`; el historial los conserva).
- **`Developer/Minkar.sdk` tiene 2.946 copias** de otros ficheros con seguimiento (headers
  de libc++ y de xnu, páginas de manual), que el build podría generar.
- **Acoplamiento:** desde el fork, el 69 % de los commits toca solo una de las áreas Kernel,
  Libraries, Frameworks, CoreServices, BSD y Developer. Los commits de CoreServices tocan
  también Frameworks el 55 % de las veces; los de Developer tocan Libraries el 40 %; los de
  Frameworks tocan Developer el 2 %. Esto respalda el núcleo tal como está decidido:
  CoreServices y Frameworks cambian juntos, y el toolchain es lo que cambia solo.
- **Tiempos** (solo mediciones que ya existían): desde cero, en el host anterior, el
  toolchain tardó 52 minutos y todo lo demás entre 16 y 19 ([BUILDING.es.md](BUILDING.es.md));
  en la CI el toolchain tarda unas dos horas cuando falla su caché. La mediana de un lote es
  de 18 minutos (86 lotes, 17–20 nov). El gate BC añade de 2 a 3 horas con TCG.

## 3. R1 — el toolchain como artefacto

**Qué se mueve:** `Developer/Default.xctoolchain` (LLVM y clang, Swift, cctools y ld64,
mig, xar, gmake, unifdef, las herramientas CTF, rpcgen, plktool, xcbuild, ibtool, iig,
codesign). `tools/toolchain` (su manifiesto y `manifest.sh`) se va con él.

**Repositorio:** `minkaros/minkar-toolchain` (decidido el 21 nov 2026).

**Cómo se publica:** la CI del repositorio nuevo compila el toolchain para el host de
compilación (Ubuntu 24.04, x86-64) y publica un archivo por versión, con su SHA-256,
etiquetado con la versión del toolchain (por ejemplo `21.1.6-swift6.4.2-1`). Su manifiesto
registra los árboles de las fuentes, las versiones de las herramientas y la comprobación de
reproducibilidad que hace hoy `manifest.sh check`.

**Cómo lo usa el árbol:** un manifiesto fijado en el repositorio principal (url, sha256,
versión), descargado con el mismo mecanismo que las demás descargas fijadas
(`BSD/lib/libdrm/fetch.sh`), desempaquetado en `../build/Developer/Platforms/Minkar.platform`
y comprobado con `manifest.sh check` frente al manifiesto publicado. `bmake` compila las
fases posteriores a `Developer` exactamente como ahora.

**El camino alternativo:** `TOOLCHAIN_SOURCE=<un checkout de minkar-toolchain>` lo compila
desde sus fuentes, como hace hoy el árbol, para un cambio en el propio toolchain o un host
que el artefacto no cubra.

**plktool y las herramientas propias del build del sistema:** `plktool` (el constructor del
kernelcache) y `codesign` son herramientas del build del sistema, no del compilador: podrían
quedarse en el repositorio principal (`tools/` o `Developer/`) y compilarse con el
compilador del host. Se quedan (decidido el 21 nov 2026).

**AArch64:** en el mismo paso, `LLVM_TARGETS_TO_BUILD` pasa a `X86;AArch64`, con el triple
por defecto todavía en x86-64. clang, lld y el compilador de Swift generan entonces código
arm64, y ARM0 pasa al toolchain propio del árbol.

**Cómo se deshace:** se restaura el directorio con un commit normal desde el historial y el
build vuelve a apuntar a él.

**Qué lo valida:** el build completo desde el artefacto, `bmake layers-check`, los gates M2
y M3, el gate BC, las pruebas de lote con `LOTE_SCOPE=base` y `manifest.sh check`.

## 4. R2 — las fuentes de terceros de BSD como descargas fijadas

**Qué se mueve:** cada componente de terceros bajo `BSD` (y `GNU`) pasa a un manifiesto
(url, sha256, versión) más un directorio `patches/`, compilado a través de
`minkar.extlib.mk`, un componente por commit. 33 componentes ya son descargas fijadas (31
por URL y SHA-256; WebKit y JavaScriptCore por commit de git).

**Orden, de más fácil a más difícil** (según el inventario):

1. Los cinco que `minkar.extlib.mk` ya compila desde una copia en el árbol
   (`EXTLIB_SOURCE`): fontconfig, freetype, libxml2, libjpeg, libpng. La regla existe; solo
   cambia el origen.
2. Los componentes sin modificar con su propio sistema de build (66), por ejemplo vim,
   openssl, bind, git, sudo, zsh, nano, libtiff, ksh, tcpdump, rsync, tcsh, htop, liburcu,
   libuv, dash y ncurses (cuya salida de compilación subida se va con él).
3. Los poco parcheados (122), por ejemplo heimdal (3 ficheros), curl (3 ficheros), pixman,
   dtrace, mDNSResponder, bash.
4. Los muy modificados o sin verificar (65), por ejemplo configd, launchd, las herramientas
   de hfs y nfs, Info-ZIP, SQLite, libedit, libarchive, pfctl.

**Lo que no encaja:** los comandos de FreeBSD compilados con las reglas propias de bmake (la
mayor parte de `BSD/bin`, `sbin` y `usr.bin`) no pueden pasar a descargas a través de
`minkar.extlib.mk` tal como es: compila proyectos con su propio sistema de build.
Se quedan en el árbol (decidido el 21 nov 2026).

**Sin referencias:** los 21 componentes que no nombraba ningún Makefile (entre ellos bash 5.2
y dmidecode de `GNU/`, y `BSD/lib/libutf8`) se quitaron el 21 nov 2026 (`1d3e679f11`).

**Cómo se deshace:** revertir el commit del componente. **Qué lo valida:** el build de la
fase, las pruebas del componente y las pruebas de lote a las que llegan sus ficheros.

## 5. R3 — la CI publica las capas compiladas

**Qué cambia:** la CI publica, por cada commit en `main`, el toolchain compilado (desde R1),
el kernel con sus kexts y el sysroot base, cada uno con su SHA-256. Quien cambie una capa
descarga las demás y compila solo la suya.

**Cómo se fija:** por el commit que compiló cada capa; el build comprueba que la capa
descargada corresponde a las fuentes que dice (el hash de árbol de sus directorios).

**Cómo se deshace:** el build vuelve a compilarlo todo, como hoy.

**Qué lo valida:** un build desde las capas publicadas da la misma imagen que un build
completo (comparada fichero a fichero), y los gates pasan sobre ella.

## 6. R4 — las apps de Atlas a su propio repositorio

**Qué se mueve:** las apps de usuario de `CoreServices`: Terminal, TextEditor,
SystemMonitor, Logs y ForceQuit, y A5–A10 según lleguen. SystemPreferences también es
candidata, pero sus paneles de Ratón y Teclado usan la interfaz privada de SkyLight, y sus
paneles comparten claves de notificación y nombres de preferencias con el Dock y
SystemUIServer: se queda en el núcleo (decidido el 21 nov 2026).

**Qué se queda:** el Dock, Files, loginwindow, SystemUIServer y los doce daemons.

**Qué usan las apps fuera del SDK público:** cinco de las seis candidatas usan solo headers
públicos. En el núcleo, Files usa un selector privado de AppKit y el `LSRevealInFiles` de
Minkar, y el Dock y loginwindow usan SkyLight; se quedan, así que esto no bloquea R4.

**Cómo se compila:** contra el `Minkar.sdk` publicado, con el mismo `xcbuild` y los mismos
ficheros de proyecto; la imagen toma las compilaciones publicadas de las apps.

**Cómo se deshace:** devolver los directorios con un commit normal.

**Qué lo valida:** las pruebas propias de las apps en la imagen, el escritorio del gate M5 y
las pruebas de lote.

## 7. Lo que este cambio no consigue

- **El historial no se reduce.** Sacar un directorio deja todas sus versiones pasadas en el
  historial del repositorio principal, incluidos los 3 GB de salida de compilación de
  ncurses; solo una reescritura los quitaría, y el historial no se reescribe.
- No divide el núcleo: los gates siguen arrancando una imagen hecha desde un repositorio.

## 8. Decidido el 21 nov 2026

El mantenedor respondió las preguntas abiertas de la versión propuesta:

1. El repositorio del toolchain es `minkaros/minkar-toolchain`, y su artefacto se publica
   en las versiones de GitHub (Releases) de ese repositorio.
2. El artefacto cubre por ahora solo Ubuntu 24.04 en x86-64.
3. `plktool`, `codesign` y las demás herramientas del build del sistema se quedan en el
   repositorio principal, compiladas con el compilador del host.
4. Los comandos de FreeBSD compilados con bmake se quedan en el árbol; R2 convierte solo
   los proyectos con su propio sistema de build.
5. La salida de compilación de `BSD/lib/ncurses/obj_s` se quitó ya (`723a0f77ba`).
6. SystemPreferences se queda en el núcleo; R4 se lleva las demás apps.
7. El build genera las copias de `Developer/Minkar.sdk` a partir de R1.
8. Los 21 componentes que no construía ningún Makefile se revisaron uno por uno y se
   quitaron (`1d3e679f11`).
