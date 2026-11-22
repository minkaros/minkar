# Compilar Minkar desde las fuentes

Cómo compilar Minkar, hacer una imagen arrancable y arrancarla en QEMU, con el anfitrión
que necesita, las capas y los gates de la compilación, y los errores frecuentes. Para
quien compila el árbol. Última revisión: 21 nov 2026. Es una copia de
[BUILDING.md](BUILDING.md) y se actualiza en el mismo commit.

## Inicio rápido

En Ubuntu 24.04 (x86-64, una CPU con AVX), con 16 GB de RAM, unos 40 GB libres en un
sistema de archivos que distinga mayúsculas, y root:

    git clone https://github.com/minkaros/minkar && cd minkar && mkdir -p ../build
    sudo tools/ci/host-setup.sh packages image
    sudo bmake -m $PWD/BSD/share/mk -j$(nproc) > ../build/world.log 2>&1
    sudo bmake -m $PWD/BSD/share/mk boottest-key
    sudo env INIT= ROOT_PASSWORD=secret ROOT_AUTHORIZED_KEYS=../build/boottest-ssh/id_ed25519.pub \
        bmake -m $PWD/BSD/share/mk image
    sudo env GATE=M2 ROOT_PASSWORD=secret BOOTTEST_SSH_KEY=../build/boottest-ssh/id_ed25519 \
        bmake -m $PWD/BSD/share/mk boottest

La primera compilación compila el toolchain (LLVM, Swift) y tarda horas. `boottest`
imprime la orden de QEMU para arrancar la imagen a mano. El resto de la página es
referencia.

## Cómo se organiza la compilación

Minkar se compila con BSD make (bmake). La compilación crea primero un toolchain para el
anfitrión (Default.xctoolchain: clang/LLVM, Swift, cctools, ld64, xcbuild, mig, ...) y
después lo usa, con el Minkar.sdk que produce, para compilar el resto del sistema. El
directorio de compilación es `../build`, junto al árbol de fuentes (`ROOT_BINARY_DIR`).

## Requisitos del anfitrión

El anfitrión de referencia es Ubuntu 24.04.5 LTS (x86-64; kernel 6.8,
clang 18, GCC 13, glibc 2.39, QEMU 8.2, Python 3.12) con 24 CPU y 32 GB de
RAM; la CI usa los runners `ubuntu-24.04` de GitHub. (Hasta el 7 nov 2026
era Ubuntu 22.04.) Los nombres de paquete son los de Debian/Ubuntu;
`tools/ci/host-setup.sh packages image` los instala todos.

El compilador del anfitrión solo compila el toolchain (LLVM, cctools, Swift
y las herramientas de compilación); el propio Minkar lo compila el clang del
toolchain, así que la versión del compilador del anfitrión no cambia el
sistema que sale.

Requisitos: un sistema de archivos que distinga mayúsculas (ver abajo), al
menos 16 GB de RAM para `-j10` (los enlazados de LLVM son el pico), unos
40 GB libres de disco y una CPU x86-64 con AVX si la prueba de arranque
corre bajo KVM.

* Compilación del toolchain: `clang lld cmake ninja-build python3
  python3-venv git curl`, `libxml2-dev libpng-dev uuid-dev libssl-dev
  zlib1g-dev libbsd-dev`, `libacl1-dev` (xar), `libc++-dev libc++abi-dev`
  (plktool), `libedit-dev libffi-dev`, `libblocksruntime-dev` (el
  `Block.h` de ld64), `libsqlite3-dev` (el llbuild de Swift Build), y
  `libicu-dev libkqueue-dev autoconf automake libtool pkg-config` para
  Open CF-lite (su run loop incluye `sys/event.h`, que da libkqueue). (Las
  herramientas CTF ya no enlazan con el LLVM ni la libdispatch del
  anfitrión: se compilan con las fuentes de LLVM del toolchain y una pequeña
  capa de dispatch.)
* libdispatch (swift-corelibs-libdispatch, etiqueta `swift-6.0-RELEASE`,
  Apache-2.0) en `/usr/local`: las cabeceras de Open CF-lite incluyen
  `<dispatch/dispatch.h>`, y Ubuntu 24.04 no tiene `libdispatch-dev`.
  `tools/ci/host-setup.sh packages` la compila antes que Open CF-lite (cmake
  y ninja); como Open CF-lite, es solo una herramienta de compilación.
* Open CF-lite 635.21.8 (gerickson/opencflite, APSL) en `/usr/local`:
  plktool y xcbuild enlazan con un CoreFoundation del anfitrión. Es solo
  una herramienta de compilación y nada de ella entra en Minkar.
  `tools/ci/host-setup.sh packages` lo compila (`./configure
  --with-tz-includes=<sus fuentes>`, ya que Ubuntu no tiene `tzfile.h`;
  después `make && make install`).
* Compilación del sistema: `bmake make csh bison flex time`, y `byacc`
  (Berkeley yacc, dominio público: el analizador de libxkbcommon se genera
  con él, no con bison), `ruby` (2.5 o posterior: la compilación de
  JavaScriptCore genera código con él), y `gperf` y `unifdef` (la
  compilación de WebCore genera con ellos sus tablas de búsqueda y las
  cabeceras GLib de WebKit).
* Imagen arrancable y prueba de arranque: `nasm gdisk dosfstools mtools hfsprogs`,
  `qemu-system-x86 ovmf` (la prueba de arranque usa QEMU con TCG salvo con
  `BOOTTEST_ACCEL=kvm`; su VM es un Ivy Bridge, `-cpu IvyBridge`, porque
  Minkar necesita AVX, y QEMU emula AVX con TCG desde la 7.2, así que basta
  la 8.2 de Ubuntu 24.04), `openssh-client openssl` para el gate M2, y el
  módulo `hfsplus` del kernel (en `linux-modules-extra-*` en los kernels de
  la nube).

La compilación llama directamente a `/usr/bin/clang` (`HOST_CC`,
`HOST_CXX`), así que `cc` puede seguir siendo gcc.

El árbol de fuentes debe estar en un sistema de archivos que distinga
mayúsculas (ext4, no un volumen HFS+ o APFS por omisión): algunos árboles
tienen nombres que solo se diferencian en mayúsculas. Espacio en disco, medido el 21 nov
2026 tras quitar los volcados de compilación de ncurses y los 21 componentes que no se
compilaban: 3,0 GB de fuentes versionadas (232.104 ficheros) y 1,9 GB de historial de git;
en el árbol de compilación del anfitrión de desarrollo, 2,7 GB de toolchain, 16 GB de
directorios de objetos, 3,5 GB del Swift del anfitrión con el que se compila el toolchain,
una sysroot de 1,2 GB y una imagen de 1,2 GB. Antes de esa limpieza las fuentes ocupaban
unos 7 GB con su historial.

## Compilar

La compilación se ejecuta como root (los directorios de objetos y la
sysroot son de root):

    cd /work/minkar
    sudo bmake -m /work/minkar/BSD/share/mk -j$(nproc) > ../build/world.log 2>&1

Una fase cada vez, en este orden: `Developer Kernel Libraries Frameworks
Kernel/Extensions CoreServices BSD Frameworks/WebKit SystemLibrary`. Nombra
la fase tanto en `ONLY_STAGE` como en el objetivo:

    sudo env ONLY_STAGE=Frameworks bmake -m /work/minkar/BSD/share/mk -j$(nproc) Frameworks

(`SUBDIR=<fase>` en el entorno hace lo mismo; no lo pases nunca en la línea
de órdenes, donde se colaría en cada sub-make.) La imagen de disco se crea
después a partir de la sysroot:

    sudo bmake -m /work/minkar/BSD/share/mk image

`-m /work/minkar/BSD/share/mk` es obligatorio. Los registros llevan colores
ANSI; quítalos antes de buscar: `sed 's/\x1b\[[0-9;]*m//g' ../build/world.log`.

Resultados, bajo `../build`:

* `Developer/Platforms/Minkar.platform/Developer/Toolchains/Default.xctoolchain`
* `Developer/Platforms/Minkar.platform/Developer/SDKs/Minkar.sdk`
* `sysroot/` - el sistema de archivos raíz
* `kernelcache` - el kernel preenlazado y las extensiones

## Tiempos esperados

Medido desde cero el 21 y 22 nov 2026, en `devel` `de48918522`, en el
anfitrión de compilación: Ubuntu 24.04.5 LTS (kernel 6.8), 24 CPU (un Xeon
E5-2697 v2, 12 núcleos con 2 hilos cada uno), 32 GB de RAM, `JOBS=20`, turbo
apagado (`minkar-heavy`), en un worktree limpio con un árbol de compilación
vacío, con `tools/ci/build-stages.sh`. Tres fases se pararon por problemas de
orden de compilación que encontró esta medición (arreglados en `11a96ec85c`,
`0dd9fece36`, `ff19ade12e` y `de48918522`) y se retomaron donde se pararon;
su tiempo es la suma de sus pasadas, muy cercano al de una pasada sin
interrupciones, porque la segunda solo compila lo que la primera no. En la
CI, el toolchain tarda unas dos horas cuando falla su caché, y el gate BC de
2 a 3 horas bajo TCG.

| Fase | Tiempo real |
| --- | --- |
| `Developer` (el toolchain: LLVM y clang, luego Swift) | 3 h 12 min la primera vez (LLVM, 1 h 30 min aprox.; Swift, 1 h 40 min aprox.); segundos una vez compilado |
| `Kernel` | 10 min |
| `Libraries` | 55 s |
| `Frameworks` | 34 min |
| `Kernel/Extensions` | 56 s |
| `CoreServices` | 1 min 30 s |
| `BSD` (WPE WebKit y JavaScriptCore, Mesa, FFmpeg, Ruby, Python, Perl...) | 62 min |
| `Frameworks/WebKit` | 7 s (su motor se compila en `BSD`) |
| `SystemLibrary` | 37 s |
| Todo lo posterior a `Developer` | 1 h 50 min |
| La compilación entera desde cero | unas 5 h |
| `image`, la primera vez (descarga y compila el cargador EFI) | 3 min 15 s |
| `image`, después | 13 s |
| `boottest GATE=M1` | unos 20 s |
| `boottest GATE=M2` | unos 2 min (superado a los 117 s) |

Un cambio en las fuentes de LLVM vuelve a compilar LLVM (1 h 30 min aprox.)
y lo que el toolchain compila sobre él.

Los objetivos por capa compilan solas las fases de una capa, en orden,
sobre las capas inferiores ya compiladas, y `<capa>-check` comprueba lo que
instaló sin arrancar (`layers-check` las ejecuta todas; `image-check`
arranca la imagen hasta el gate M2):

| Capa | Fases | `<capa>-check` |
| --- | --- | --- |
| `toolchain` | `BSD/hier`, `Developer` | el manifiesto del toolchain (abajo) |
| `kernel` | `Kernel`, `Kernel/Extensions` | el kernel es un Mach-O, el kernelcache está bien formado, cada kext es un bundle de kext |
| `libsystem` | `Libraries` | toda biblioteca que carga `/usr/lib` está en la sysroot |
| `frameworks` | `Frameworks` | lo mismo para los frameworks; sus plists se leen bien |
| `userland` | `BSD` | lo mismo para las órdenes |
| `sysapps` | `CoreServices`, `SystemLibrary` | lo mismo para CoreServices; las plists del sistema se leen bien |

Las comprobaciones son `tools/check/sysroot-check.py` (enlaces, plists,
kexts, macho, kernelcache).

El toolchain está fijado por `tools/toolchain/MANIFEST`: la versión de
origen de cada componente, el árbol git de sus fuentes y la línea de
versión que imprime su herramienta. `tools/toolchain/manifest.sh check
<toolchain>` comprueba todo eso y que la misma entrada compile, enlace y
archive a los mismos bytes dos veces; tras cambiar un componente,
`tools/toolchain/manifest.sh update` reescribe los árboles. La compilación
exporta `SOURCE_DATE_EPOCH` (la fecha del último commit), así que
`__DATE__` y `__TIME__` no hacen diferentes dos compilaciones.
`ONLY_STAGE` (arriba) compila una fase sin recorrer las anteriores.

## Imagen arrancable y prueba de arranque

Después de compilar:

    sudo env SUBDIR= bmake -m /work/minkar/BSD/share/mk image
    sudo env SUBDIR= bmake -m /work/minkar/BSD/share/mk boottest           # gate M0
    sudo env SUBDIR= GATE=M1 bmake -m /work/minkar/BSD/share/mk boottest   # gate M1

`image` descarga el cargador EFI (ravynsoft/efiloader, fijado en
`Image/Makefile` y parcheado desde `Image/patches/`), lo compila con el
clang del toolchain y escribe `../build/Minkar.img`: un disco GPT con una
partición de sistema EFI (cargador, kernelcache, plist de arranque) y un
volumen raíz HFS+ creado a partir de la sysroot. `boottest` arranca la
imagen en QEMU con OVMF, sigue la consola serie y pasa cuando aparecen las
marcas del gate; la consola se guarda en `../build/boottest-<gate>.log`.
Para arrancar la imagen tú mismo, usa la orden que imprime `boottest`.

Gates (ver el plan de arquitectura):

* M0 - el kernel imprime su primer mensaje.
* M1 - se monta la raíz HFS+, el PID 1 se ejecuta y lanza un shell, y el
  shell responde a una orden escrita en la consola serie.
* M2-login - launchd como PID 1 lanza getty en la consola, root entra con
  login y PAM, y el shell responde. Necesita launchd como PID 1 y una
  contraseña de root en la imagen:

      sudo env SUBDIR= INIT= ROOT_PASSWORD=secret bmake -m /work/minkar/BSD/share/mk image
      sudo env SUBDIR= GATE=M2-login ROOT_PASSWORD=secret bmake -m /work/minkar/BSD/share/mk boottest

  Las imágenes no tienen contraseña de root salvo que se dé ROOT_PASSWORD
  (con el hash de `openssl passwd -6` del anfitrión). Para poner en marcha
  launchd, LAUNCHD_DEBUG=1 hace que launchd y launchctl registren todo en
  la consola.
* M2 - M2-login, y después la red: la tarjeta e1000e (MinkarE1000 sobre
  nuestro IONetworkingFamily, con una interrupción MSI) aparece como en0,
  dhcpclient obtiene una concesión de QEMU (10.0.2.15) y root entra por ssh
  desde el anfitrión con una clave; el shell remoto responde, `scutil --dns`
  muestra el servidor de nombres y el nombre del anfitrión se resuelve con
  `dscacheutil` (el resolv.conf de IPMonitor, mDNSResponder, getaddrinfo).
  QEMU envía las consultas DNS del invitado al resolutor del anfitrión, así
  que el anfitrión debe resolver su propio nombre (systemd-resolved y
  /etc/hosts lo hacen); no hace falta Internet. QEMU reenvía el puerto 2222
  del anfitrión (BOOTTEST_SSH_PORT) al puerto 22 del invitado.
  `boottest-key` crea el par de claves una vez:

      sudo env SUBDIR= bmake -m /work/minkar/BSD/share/mk boottest-key
      sudo env SUBDIR= INIT= ROOT_PASSWORD=secret \
          ROOT_AUTHORIZED_KEYS=../build/boottest-ssh/id_ed25519.pub \
          bmake -m /work/minkar/BSD/share/mk image
      sudo env SUBDIR= GATE=M2 ROOT_PASSWORD=secret \
          BOOTTEST_SSH_KEY=../build/boottest-ssh/id_ed25519 \
          bmake -m /work/minkar/BSD/share/mk boottest

  ROOT_AUTHORIZED_KEYS instala cualquier archivo de clave pública como el
  authorized_keys de root. sshd mantiene el PermitRootLogin
  prohibit-password por omisión de OpenSSH: root entra por ssh con una
  clave, los demás usuarios con su contraseña (PAM). `boottest.py --nic
  e1000` prueba la 82540EM, que no tiene MSI: usa su línea INTx,
  encaminada por el _PRT de ACPI mediante ACPICA en IOACPIFamily.

  configd (el APSL, con sus plugins dentro) da nombre a las interfaces: su
  InterfaceNamer da a IONetworkStack la unidad que conserva cada una entre
  arranques (/Library/Preferences/SystemConfiguration/NetworkInterfaces.plist)
  una vez que IOKit está tranquilo; kextstart avisa a IOKit al arrancar de
  que no hay kextd al que esperar. Sin quien les dé nombre, IONetworkStack
  las nombra por sí mismo pasados 30 s. dhcpclient atiende cada interfaz
  Ethernet según aparece y publica lo que configura en configd
  (State:/Network/Service/<id>), cuyo IPMonitor fija la ruta por omisión y
  resolv.conf y sirve la configuración DNS (`scutil --dns`, `scutil
  --nwi`); sin configd lo hace él mismo. `ipconfig set en0
  DHCP|BOOTP|NONE|MANUAL <dir> <máscara>|INFORM <dir> <máscara>` e
  `ipconfig set en0 AUTOMATIC-V6|LINKLOCAL-V6|NONE-V6|MANUAL-V6 <dir>
  <longitud de prefijo>` cambian su método, `ipconfig getsummary en0`
  muestra lo que tiene.

* BC - M2 y el servidor de ventanas, y después el binario publicado de
  cada app designada (carril BC en [ROADMAP.es.md](ROADMAP.es.md); las apps
  de `tools/symbols/reference-apps.tsv`). `bc-check` las descarga en
  `../build/bc-apps` por su URL y su SHA-256 (se quedan allí; nunca van al
  repositorio ni a la imagen), y para cada una copia su archivo a la VM
  desde el disco de apps (`../build/bc-apps/bc-apps.img`, una imagen FAT32
  de las descargas hecha con `mtools` y conectada en solo lectura;
  `--no-apps-disk` en `BC_ARGS` los envía por ssh), donde `Tests/BC/bc-run.sh` la instala como lo haría un usuario (se
  conecta la imagen de disco y se copia la app a /Applications, o se
  expande el zip o el tar) y la arranca con una sonda. El resultado de cada
  app es hasta dónde llegó (de `install` a `window`); una que llega menos
  lejos que en `Docs/progress/compatibility.tsv` hace fallar el gate.
  `BC_UPDATE=yes` escribe allí los resultados nuevos, y `bmake progress`
  genera con ellos [Compatibility.es.md](Compatibility.es.md). La imagen
  necesita la sonda (`bc-tools` antes de `image`):

      sudo env SUBDIR= bmake -m /work/minkar/BSD/share/mk bc-tools
      sudo env SUBDIR= INIT= ROOT_PASSWORD=secret \
          ROOT_AUTHORIZED_KEYS=../build/boottest-ssh/id_ed25519.pub \
          bmake -m /work/minkar/BSD/share/mk image
      env SUBDIR= BC_ARGS="--password secret --ssh-key ../build/boottest-ssh/id_ed25519 --accel kvm" \
          bmake -m /work/minkar/BSD/share/mk bc-check

Hasta que launchd se ejecuta, el PID 1 es `minkarinit`
(`BSD/sbin/minkarinit`), un pequeño init temporal que la imagen instala
como `/sbin/launchd`, la única ruta que prueba un kernel RELEASE. Compila
la imagen con `INIT=` para arrancar en su lugar el `/sbin/launchd` de la
propia sysroot.

## Errores frecuentes

* `Could not find bsd.mkopt.mk` (y `bsd.opts.mk`, `bsd.linker.mk`)\
  Se ejecutó bmake sin `-m /work/minkar/BSD/share/mk`.
* `cd: .../Frameworks/Frameworks: No such file or directory`\
  Se dio `SUBDIR=` en la línea de órdenes. Pásalo por el entorno
  (`env SUBDIR=...`) o usa `ONLY_STAGE`.
* `SYSROOT_DIR is not set: build BSD/etc from the top of the tree`\
  Se ejecutó bmake dentro de un subdirectorio. Ejecútalo siempre desde la
  raíz del árbol: un subdirectorio solo no conoce la sysroot y podría
  instalar en el anfitrión de compilación.
* `Permission denied` en `../build`\
  La compilación se ejecuta como root (`sudo`); los directorios de objetos
  y la sysroot son de root.
* `byacc (Berkeley yacc) is required on the build host`\
  Instala `byacc` (el analizador de libxkbcommon, en `CoreServices`).
* Errores de enlace del toolchain: `cannot find -lacl`,
  `-lCoreFoundation`, `-ldispatch`, `-lLLVM` o `-lc++`\
  Faltan paquetes del anfitrión, u Open CF-lite no está en `/usr/local`
  (ver «Requisitos del anfitrión»). `tools/ci/host-setup.sh packages` los
  instala.
* Símbolos sin definir al enlazar una biblioteca base en una compilación
  limpia, aunque una incremental funciona\
  Una biblioteca base ganó un símbolo que usa otra, y los stubs de arranque
  aún no lo listan. Ejecuta `Libraries/bootstrap/regen-stubs.sh` y haz
  commit de los archivos `.tbd` que cambie.
* Todo LLVM se vuelve a compilar aunque el toolchain ya está compilado\
  Las herramientas del toolchain son más antiguas que sus fuentes (un
  toolchain copiado o restaurado, o una copia recién clonada): bmake vuelve
  a compilar mig, y la regla de LLVM depende de él.
  `tools/ci/host-setup.sh touch-toolchain` marca al día un toolchain
  restaurado.
* Un cambio en el kernel no aparece en la imagen\
  El kernelcache lo crea `Kernel/Extensions`. Después de compilar `Kernel`,
  compila también `Kernel/Extensions`.
* AppKit no ve una cabecera de AppKit editada\
  AppKit incluye `<AppKit/...-Private.h>` desde la copia del framework en
  su directorio de objetos, que solo se renueva al enlazar. Copia ahí la
  cabecera (`AppKit.framework/Headers`) o vuelve a enlazar.
* Todo se recompila tras mover el árbol de fuentes\
  Los directorios de objetos se nombran por la ruta de las fuentes
  (`../build/<ruta de las fuentes>/...`). Deja el árbol donde se compiló.
* Archivos que faltan sin explicación en el sistema de referencia o en un
  disco HFS+ o APFS\
  El sistema de archivos no distingue mayúsculas; algunos árboles tienen
  nombres que solo se diferencian en mayúsculas. Usa un volumen que las
  distinga.
* `mount: unknown filesystem type 'hfsplus'` (imagen)\
  Al kernel del anfitrión le falta el módulo `hfsplus`: instala
  `linux-modules-extra-$(uname -r)` y ejecuta `modprobe hfsplus`.
* `boottest: gate M2 needs --password or $BOOTTEST_PASSWORD`,
  `needs --ssh-key or $BOOTTEST_SSH_KEY`\
  Compila la imagen con `ROOT_PASSWORD` y `ROOT_AUTHORIZED_KEYS`, y pasa a
  `boottest` la misma contraseña y la clave privada (ver arriba).
* `boottest: ... the host cannot resolve it either`\
  El gate M2 resuelve desde el invitado el nombre propio del anfitrión.
  Añádelo a `/etc/hosts` (`127.0.1.1 <nombre>`).
* El kernel entra en pánico justo después del cargador al lanzar QEMU a
  mano\
  El cargador necesita `-device virtio-rng-pci` (EFI_RNG_PROTOCOL), y usa
  `-cpu IvyBridge` (AVX), no `max` ni `host`. `Image/boottest.py` muestra
  una línea de órdenes que funciona.
* Errores relacionados con `'stdarg.h'` en ld64 (`va_start` sin declarar),
  o `Block.h` no encontrado en el `Snapshot.cpp` de ld64\
  Un árbol antiguo sobre glibc 2.39 (Ubuntu 24.04), que ya no incluye
  `stdarg.h` desde otras cabeceras, o falta `libblocksruntime-dev`.
  Actualiza el árbol y ejecuta `tools/ci/host-setup.sh packages`.
* `dispatch/dispatch.h` o `sys/event.h` no encontrados al compilar Open
  CF-lite\
  libdispatch no está en `/usr/local` o falta `libkqueue-dev`:
  `tools/ci/host-setup.sh libdispatch`, y después `packages`.
* Errores `-Wint-conversion` o `-Wincompatible-pointer-types` en las
  herramientas CTF\
  clang 15 y posteriores los convierten en errores; el árbol las compila con
  `-Wno-error=...`. Un árbol antiguo necesita actualizarse.
* `'CoreFoundation/CFBase.h' file not found` en libdispatch, o
  `'Foundation/Foundation.h' file not found` en el runtime de Swift,
  durante `Kernel`\
  Una compilación desde cero en un árbol antiguo: `Kernel` compila
  `Libraries` antes de que la fase `Frameworks` instale CoreFoundation y
  Foundation. El árbol copia ahora primero las cabeceras de CoreFoundation
  al SDK, y compila el runtime de Swift en `Frameworks`, después de
  Foundation. Actualiza el árbol.
* Los registros cuestan de buscar\
  Llevan colores ANSI: `sed 's/\x1b\[[0-9;]*m//g' archivo.log`.
  `tools/ci/build-stages.sh` guarda un registro por fase e imprime las
  primeras líneas de error de una fase que falla.

## Pruebas en el anfitrión

Las partes portables del sistema tienen pruebas en el anfitrión (clang,
ASan/UBSan):

* `make -C Frameworks/OpenGL/tests`
* `python3 -B Developer/Default.xctoolchain/codesign/tests/test_codesign.py`
* `make -C Libraries/Libsystem/libsystem_c/minkar_crypt/tests` (SHA-crypt de crypt(3))
* `cc -DTEST -I Libraries/Libsystem/libsystem_coreservices -o /tmp/sysdir_test Libraries/Libsystem/libsystem_coreservices/sysdir.c && /tmp/sysdir_test`

## Compilaciones cronometradas

`tools/ci/build-stages.sh` compila las fases una a una, cada una con su
propio registro y tiempo (`../build/logs/<fase>.log`,
`../build/logs/times.txt`), e imprime el final del registro de una fase
que falla:

    cd /work/minkar
    sudo tools/ci/build-stages.sh
    sudo env STAGES="Frameworks BSD" tools/ci/build-stages.sh

## Integración continua

`.github/workflows/build-boottest.yml` se ejecuta con los push y pull
requests a `main` y a mano (Actions > build and boottest > Run workflow, en
cualquier rama): el repositorio es privado en una cuenta gratuita, cuyos
minutos de Actions se agotan con ejecuciones de más de una hora, así que
los lotes de `devel` se verifican en local. Los cambios van primero a
`devel`. En un runner Ubuntu 24.04 alojado por GitHub:

1. comprueba, en segundos y antes de compilar nada, que las páginas de
   progreso son las que escribe `bmake progress` y que cada etapa lista lo
   que necesita un directorio antes que él (`tools/check/subdir-order.py`:
   Frameworks compila su `SUBDIR` en orden, así que una dependencia
   listada después solo rompe la compilación desde cero, horas después de
   empezar);
2. compila el toolchain (`Developer`), o lo toma de la caché;
3. compila el resto del sistema con `tools/ci/build-stages.sh`, y comprueba
   el toolchain frente a `tools/toolchain/MANIFEST`;
4. crea una imagen con launchd como PID 1, una contraseña de root aleatoria
   y una clave ssh nueva, y ejecuta `boottest GATE=M2`.

El toolchain tarda unas dos horas en un runner alojado, así que se guarda
en caché. La clave de la caché es el árbol git de
`Developer/Default.xctoolchain`: cualquier cambio ahí lo vuelve a compilar.
La caché contiene el toolchain instalado y sus directorios de objetos,
pero no el árbol de compilación de LLVM; después de restaurarla,
`tools/ci/host-setup.sh touch-toolchain` lo hace más nuevo que la copia del
repositorio, o bmake volvería a compilar mig y, con él, LLVM. Sube
`CACHE_VERSION` en el workflow para descartar la caché.

Cuando falla una fase o la prueba de arranque, el final de su registro
aparece como una anotación de error en la ejecución; los registros
completos están en los artefactos de la ejecución.
