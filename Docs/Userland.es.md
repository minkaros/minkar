# Userland BSD

*Traducción de [Userland.md](Userland.md); el documento en inglés es el de referencia.*

Las órdenes del sistema base de Minkar: de dónde viene cada una, qué se
añadió para la compatibilidad con el sistema de referencia y qué se encontró
y corrigió al portarla. El [roadmap](ROADMAP.es.md) (Fase 2) sigue este
trabajo en conjunto; [Commands.es.md](Commands.es.md) cuenta las órdenes de
la imagen frente a las del sistema de referencia.

## Política

Decisión (26 oct 2026): las órdenes Unix genéricas vienen todas de FreeBSD
`main`. Donde la orden del sistema de referencia se comporta distinto
(opciones, salida), nuestro propio código bajo `__APPLE__` sigue la página
de manual del sistema de referencia, cada diferencia con una prueba. Las
órdenes que llegaron de los `shell_cmds` y `adv_cmds` originales a través de
ravynOS se sustituyen. Los componentes propios de Darwin (launchd, configd,
notifyd, syslogd, las herramientas de HFS, mDNSResponder) se quedan como
están. Las licencias que no sean BSD ni MIT necesitan antes la aprobación
del proyecto.

## Estado

| Grupo | Estado |
|---|---|
| [Órdenes de arranque](#órdenes-de-arranque) | Hecho |
| [Los conjuntos de órdenes originales, sustituidos](#conjuntos-de-órdenes-originales-sustituidos) | Hecho; pequeños huecos en `grep` |
| [Herramientas de uso diario](#herramientas-de-uso-diario) | Hecho: todas las herramientas BSD de [Commands.es.md](Commands.es.md) |
| [Usuario avanzado](#usuario-avanzado) | `zsh`, `tar`, `ssh`, `curl`, `make`, `git`, Ruby, Python y Perl en la imagen; `/bin/sh` ejecuta bash |
| [Herramientas de la plataforma](#herramientas-de-la-plataforma) | Hecho: incluidas `open` y `hdiutil` |
| [Datos del sistema](#datos-del-sistema) | Locales, zonas horarias, páginas de manual, tablas de iconv |

## Órdenes de arranque

`sh`, `ls`, `cp`, `mv`, `rm`, `mount`, `ifconfig`, `ps`, `kill`, probadas en
la imagen el 26 oct 2026. `cp -p` fallaba con todo archivo sin ACL, y `mv`
entre volúmenes avisaba: Darwin informa de «sin ACL» como `ENOENT`.

## Conjuntos de órdenes originales sustituidos

No queda ninguna orden de los conjuntos de órdenes originales.

| Fecha | Órdenes | Capa de compatibilidad y notas |
|---|---|---|
| 26 oct 2026 | `uname` | `-p` imprime `i386` |
| 26 oct 2026 | `date` | Ahora en `/bin`; el orden `mmddHHMM[yy]` del sistema de referencia |
| 26 oct 2026 | `rm`, `xargs`, `cat`, `env`, `tail`, `test` | |
| 26 oct 2026 | `find` | `-xattr`, `-xattrname` |
| 26 oct 2026 | `ls` | `-@`, `-e`, `-O`, `-o` y la marca `@` del sistema de referencia |
| 26 oct 2026 | `id`, `who`, `time` | `id -F` |
| 26 oct 2026 | `grep` | Ahora en `/usr/bin`; `zgrep` es el script de FreeBSD. Falta: `-Z`/`-J` (descomprimir) del sistema de referencia |
| 27 oct 2026 | `killall` | Ahora en `/usr/bin`; como en el sistema de referencia, compara nombres completos, más allá de los 16 caracteres de `p_comm` |
| 27 oct 2026 | `kill` | En `/bin`, a partir del código de la orden interna de `sh` |
| 27 oct 2026 | `w` | Tenía las partes originales y no se compilaba; la cabecera del sistema de referencia |
| 27 oct 2026 | `ps` | Las vistas, opciones y palabras clave del sistema de referencia; los registros de procesos con `kvm_getprocs()` de libbsdcompat sobre sysctl y libproc |
| 27 oct 2026 | `top` | Propio, con las opciones, teclas y resumen del sistema de referencia; `btop` 1.4.7 (Apache-2.0) junto a él |
| 27 oct 2026 | `sysctl` | Traduce los formatos de OID de Darwin a los tipos de FreeBSD, salta los nodos sin formato, imprime `vm.swapusage` |
| 27 oct 2026 | `cal`/`ncal`, `getconf`, `locale` | `getconf` tiene los nombres `DARWIN_USER_*` del sistema de referencia |
| 27 oct 2026 | `login`, `su` | `login` es propio (BSD-2-Clause); `su` es el de FreeBSD main |

## Herramientas de uso diario

| Fecha | Órdenes | Notas |
|---|---|---|
| 26 oct 2026 | `dd`, `expr`, `printenv`, `od`/`hexdump`, `paste`, `nohup`, `install`, `ed`, `pax` | |
| 26 oct 2026 | `pgrep`/`pkill` | Las de FreeBSD, leyendo los procesos con `sysctl` |
| 26 oct 2026 | `readlink`, `link`, `unlink`, `egrep`, `fgrep`, `zgrep` | Segundos nombres |
| 26 oct 2026 | `m4`, `bc`/`dc`, `cpio` | `cpio` es el de libarchive |
| 26 oct 2026 | `cron`, `crontab` | Con un trabajo de launchd. Los trabajos nunca se ejecutaban: cron dejaba `environ` en NULL, y el `setenv` de Darwin no puede partir de ahí |
| 26 oct 2026 | `rev`, `column`, `comm`, `join`, `fold`, `fmt`, `split`, `csplit`, `tsort`, `nl`, `expand`, `unexpand` | |
| 26 oct 2026 | `jot`, `lam`, `rs`, `col`, `colrm`, `look`, `logname`, `renice`, `nice`, `lockf`, `cksum`/`sum`, `what`, `vis`/`unvis` | |
| 26 oct 2026 | `mkfifo` | Las tuberías con nombre fallaban en HFS+: el kext hfs estaba compilado sin FIFO |
| 26 oct 2026 | `uuencode`/`uudecode`/`base64` | Las opciones de `base64` del sistema de referencia |
| 26 oct 2026 | `xattr` | Escrita para Minkar, con las opciones del sistema de referencia; aún sin página de manual |
| 26 oct 2026 | `chflags` | Fallo del kernel encontrado por su prueba: el dueño de un archivo podía borrarlo estando `uchg` |
| 27 oct 2026 | `tty` | |
| 27 oct 2026 | `man`, `apropos`, `whatis`, `makewhatis` | El `man.sh` de FreeBSD main, como en el sistema de referencia, con `mandoc` 1.14 |
| 27 oct 2026 | `units` | `/usr/share/misc/units.lib` es el nombre que da a su archivo el sistema de referencia |
| 27 oct 2026 | `locate` | `locate.updatedb` indexa los volúmenes locales hfs, ufs y apfs (Darwin no tiene `lsvfs`) |
| 27 oct 2026 | `iconv` | La de FreeBSD main, sobre el libiconv de Citrus (ver [Datos del sistema](#datos-del-sistema)) |
| 28 oct 2026 | `lsof` | lsof-org 4.99.7 (licencia de Purdue), su dialecto darwin sobre libproc, en `/usr/sbin`: archivos, sockets, tuberías, `-t`, `-F`, `-J`, `+D`; un usuario solo ve sus propios procesos, como en el sistema de referencia |
| 28 oct 2026 | `zic`, `zdump` | tzcode, en `/usr/sbin` |
| 28 oct 2026 | `calendar`, `pr`, `sdiff`, `diff3`, `script`, `whois`, `ul`, `mesg`, `wall`, `write`, `banner`, `compress`/`uncompress`, `gencat`, `cap_mkdb`, `indent`, `ctags` | Las de FreeBSD main |
| 28 oct 2026 | `last` | Lee el registro wtmpx de Darwin |
| 28 oct 2026 | `timeout` | Un grupo de procesos, a falta de un reaper |
| 28 oct 2026 | `passwd` | `-i`, `-l`, `-u` y los textos del sistema de referencia; set-user-ID root. pam_unix cambia la contraseña con código propio (el de OpenPAM no podía): un hash SHA-512 en `/etc/master.passwd`, que libinfo lee directamente, así que no hacen falta las funciones `pw_*` de FreeBSD ni `pwd_mkdb` |
| 28 oct 2026 | `sync`, `mknod`, `nologin`, `asa`, `c99`, `newgrp`, `pagesize` | Las de FreeBSD main, en los sitios del sistema de referencia (`/bin`, `/sbin`, `/usr/bin`) |
| 28 oct 2026 | `finger` | El último acceso, de los registros lastlog de Darwin (`getlastlogxbyname`) |
| 28 oct 2026 | `ac` | Lee el registro wtmpx de Darwin, como `last` |
| 28 oct 2026 | `uuidgen` | Escrita para Minkar, como en el sistema de referencia: un UUID aleatorio en mayúsculas, `-hdr` para una línea de cabecera de Core Foundation (la de FreeBSD se basa en la API de UUID de DCE) |
| 28 oct 2026 | `arp`, `ndp`, `traceroute6` | A través del socket de enrutamiento (el camino de FreeBSD sin netlink). XNU rellena las sockaddr hasta 4 bytes y da a `arp` su caducidad en tiempo de calendario; `ndp -i` pone los indicadores con `SIOCSIFINFO_FLAGS` de Darwin; `traceroute6 -S` (SCTP) con cabeceras de paquete propias, ya que Darwin no tiene |
| 28 oct 2026 | `unzip` | El bsdunzip de libarchive 3.8.1; sustituido el 29 oct 2026 por el de Info-ZIP, como en el sistema de referencia, y retirado (el sistema de referencia no tiene bsdunzip) |
| 28 oct 2026 | `at`, `atq`, `atrm`, `batch` | Set-user-ID root; trabajos en `/var/at`. `atrun` los ejecuta bajo launchd cada 30 segundos, desactivado como en el sistema de referencia hasta `launchctl load -w /System/Library/LaunchDaemons/org.minkar.atrun.plist`; launchd deja sus trabajos corriendo (`AbandonProcessGroup`) |
| 28 oct 2026 | `newsyslog` | Nuestro `/etc/newsyslog.conf` para los archivos de registro propios de los demonios y `/etc/newsyslog.d/`; `system.log` y el almacén ASL siguen siendo de syslogd y aslmanager, como en el sistema de referencia. launchd lo ejecuta a la media de cada hora |
| 28 oct 2026 | `ipcs`, `ipcrm` | Leen las tablas IPC System V de XNU entrada a entrada con los sysctl `kern.sysv.ipcs.*` (Darwin no tiene `kvm(3)`, así que se rechazan `-C`, `-N` e `-y`); `ipcs -S` muestra los límites de semáforos de `kern.sysv.*`, ya que el kernel informa de lo que está en uso |
| 28 oct 2026 | `htop` | htop 3.4.1 (GPL-2.0) sobre su backend de Darwin (libproc, Mach e IOKit), junto a `top` y `btop`; no está en el sistema de referencia |
| 28 oct 2026 | `xxd` | Propia (BSD-2-Clause), escrita para comportarse como la del sistema de referencia (la de Vim): mismas columnas, opciones, código C con `-i` y `-r`, que parchea un archivo en el sitio |
| 28 oct 2026 | `alias`, `bg`, `cd`, `command`, `fc`, `fg`, `getopts`, `hash`, `jobs`, `read`, `type`, `ulimit`, `umask`, `unalias`, `wait` | Como archivos en `/usr/bin`, como en el sistema de referencia: un script propio que ejecuta la orden interna de `sh`, para que `env`, `find -exec` y `xargs` puedan lanzarlas; `builtin(1)` |
| 28 oct 2026 | `nano`, `rnano`, `pico` | nano 9.3 (GPL-3.0), la versión del proyecto; `pico` es nano, como en el sistema de referencia; `/etc/nanorc` colorea la sintaxis de los archivos que conoce |
| 28 oct 2026 | `bash` | bash 5.3 con los parches 001-020 (GPL-3.0) como `/bin/bash`, en `/etc/shells`; el sistema de referencia trae la 3.2, la última bajo GPL-2, y sus scripts funcionan en la 5.3 |
| 28 oct 2026 | `rsync` | rsync 3.5.1 (GPL-3.0); las ACL (`-A`) y atributos extendidos (`-X`) de Darwin; solo MD5 y zlib hasta que Minkar tenga OpenSSL, xxhash, zstd y lz4. El sistema de referencia trae openrsync; versiones anteriores, rsync 2.6.9 |
| 28 oct 2026 | `vim`, `vi`, `ex`, `view`, `vimdiff`, `rvim`, `rview`, `vimtutor` | Vim 9.2.1252 (licencia de Vim), el conjunto de funciones «huge» sin interfaz gráfica, como el del sistema de referencia; su runtime en `/usr/share/vim/vim92`. `vi`, `ex` y `view` son Vim, como en el sistema de referencia: nvi ya no está |
| 29 oct 2026 | `ksh` | ksh 93u+m/1.0.10 (EPL-2.0) como `/bin/ksh`, como en el sistema de referencia. Su compilación ejecuta pruebas de configuración en el destino: se ejecutaron una vez en Minkar y `BSD/usr.bin/ksh/iffe-results` devuelve sus respuestas, así que se compila en cruzado sin una máquina Minkar (`make record` las rehace); no se pregunta al `getconf` de la máquina de compilación, así que todas las máquinas compilan las mismas pruebas |
| 29 oct 2026 | `tcsh`, `csh` | tcsh 6.24.16 (BSD-3) como `/bin/tcsh` y `/bin/csh`, como en el sistema de referencia, con UTF-8 y los archivos de inicio del sistema en `/etc`; sustituye a los archivos de compilación de FreeBSD que había en `BSD/bin/csh` y nunca se compilaron |
| 29 oct 2026 | `dash` | dash 0.5.13.5 (BSD-3) como `/bin/dash`, con libedit; su tabla de señales es propia, del `<signal.h>` de Darwin (el generador del proyecto es GPL-2 y daría los números de Linux) |
| 29 oct 2026 | `mtree` | El de FreeBSD main (el mtree de NetBSD) como `/usr/sbin/mtree`, con resúmenes MD5, SHA-1 y SHA-2 de nuestra libmd (sin RIPEMD-160) |
| 29 oct 2026 | `zip`, `unzip`, `zipinfo`, `zipgrep` | Zip 3.0 y UnZip 6.0 de Info-ZIP, como en el sistema de referencia, con las correcciones de Debian (el proyecto no publica desde 2008-2009) y marcados como modificados para Minkar, como pide su licencia |
| 29 oct 2026 | `ar`, `ranlib`, `size`, `strings` | llvm-ar, llvm-size y llvm-strings de LLVM 17, del llvm-project con el que se compila nuestro compilador, compilados en cruzado con CMake; `ar` escribe el formato de archivo de Darwin con su índice de símbolos, contra el que enlaza ld64; páginas de manual propias |
| 29 oct 2026 | `vipw`, `pwd_mkdb`, `chpass`, `chfn`, `chsh` | Los de FreeBSD main, compartiendo `BSD/lib/libpw` (pw_util y pw_scan de FreeBSD, con nuestro `pw_compat.h`: el `struct passwd` de Darwin no tiene `pw_fields`, así que esos bits se guardan aparte). Los identificadores por encima de INT_MAX se leen y escriben negativos, como el `nobody` (-2) de Darwin; el primer cambio de `chpass` construye `pwd.db` y `spwd.db` enteros, ya que un sistema recién instalado no los tiene |
| 29 oct 2026 | `accton`, `sa`, `shlock` | `accton` y `sa` son los de FreeBSD main; `sa` lee los registros acct(2) de Darwin (el antiguo `struct acct` de tamaño fijo con tiempos `comp_t`) con nuestro propio `readrec.c`, y `/var/account` está en la jerarquía, como en el sistema de referencia. `shlock` es el de NetBSD (FreeBSD no tiene) |
| 29 oct 2026 | `fstyp`, `vifs` | `fstyp` es el de FreeBSD main con los nombres del sistema de referencia (`hfs`, `msdos`), los ioctl de disco de Darwin y sin los tipos que necesitan bibliotecas de FreeBSD o DragonFly (geli, ufs, zfs, HAMMER); su comprobación de HFS+ leía la versión big-endian tal cual y nunca coincidía en x86 (corregido aquí). `vifs` es propio: edita `/etc/fstab` con un bloqueo, en una copia que lo sustituye solo si cambió |
| 29 oct 2026 | `mail`, `mailx` | Los de FreeBSD main, con `/var/mail` (grupo mail) y `/etc/mail.rc`; envían por `/usr/sbin/sendmail` cuando existe (Minkar aún no tiene agente de transporte de correo) |
| 29 oct 2026 | `lex`, `flex`, `flex++` | flex 2.6.4 como lo compila FreeBSD main, de su analizador léxico, analizador sintáctico y esqueleto ya generados; ejecutado en Minkar sobre el propio `scan.l` de flex, vuelve a dar el `initscan.c` de FreeBSD |
| 29 oct 2026 | `telnet` | El de FreeBSD main, sencillo: linemode, termios, IPv6 y la opción de entorno, sin cifrado, Kerberos, OPIE ni política IPsec (bibliotecas que Minkar no tiene) |
| 29 oct 2026 | `quota`, `quotacheck`, `quotaon`, `quotaoff`, `edquota`, `repquota` | Propias, sobre los archivos hash de cuotas de Darwin (`<sys/quota.h>`) con `BSD/lib/libdqfile`, y las cuotas HFS+ del kernel, ahora compiladas en el kext hfs (`-DQUOTA`). Un volumen quiere cuotas cuando tiene `.quota.ops.user` o `.quota.ops.group` en su raíz; los archivos son big-endian, como los escribe el kernel |
| 29 oct 2026 | `openssl` | OpenSSL 3.5.9 (LTS, Apache-2.0) con `libcrypto.3.dylib`, `libssl.3.dylib` y el proveedor legacy; `/etc/ssl` es su directorio (`cert.pem`, `openssl.cnf`). El sistema de referencia trae LibreSSL como `openssl`; su licencia SSLeay lo deja fuera de Minkar |
| 29 oct 2026 | `dig`, `host`, `nslookup` | BIND 9.20.30 (MPL-2.0), como en el sistema de referencia, sobre libuv 1.53.0 (`BSD/lib/libuv`, estática), liburcu 0.15.7 (`BSD/lib/liburcu`, LGPL-2.1, compartida en `/usr/lib`) y OpenSSL; las bibliotecas propias de BIND (rechaza el enlace estático) en `/usr/lib/bind9`; sin `named`. Su configure exige un pkg-config: `tools/build/pkg-config-none.sh` no conoce ningún paquete, así que no se toma nada de la máquina de compilación |
| 29 oct 2026 | `ftp` | tnftp como lo compila FreeBSD main, con libedit e IPv6; también descarga URL `ftp://` y `http://`. `closefrom`, que Darwin no tiene, está ahora en libbsdcompat (desde `/dev/fd`) |
| 29 oct 2026 | `rlogin` | El de NetBSD (FreeBSD ya no tiene), setuid root para el puerto reservado de rcmd(3) |
| 29 oct 2026 | `tcpdump` | tcpdump 4.99.7 con libpcap 1.11.0 (estática, solo suya) sobre el BPF de Darwin |
| 29 oct 2026 | `fdisk` | El editor de MBR de FreeBSD main sobre los ioctl de disco de Darwin (nuestro pequeño `<libgeom.h>`; sin GEOM), sin el aviso de obsolescencia de FreeBSD; sin `/boot/mbr` (Minkar arranca por EFI) un MBR nuevo no tiene código de arranque |
| 29 oct 2026 | `arch`, `vm_stat`, `wait4path`, `tzlink`, `purge`, `caffeinate`, `dot_clean`, `ioreg`, `nvram` | Herramientas propias del sistema de referencia que no necesitan nada más, escritas para Minkar: `purge` con vfs_purge del kernel; `caffeinate` mantiene aserciones de energía de IOKit (aún no responde ningún gestor de energía, y nada duerme); `dot_clean` con copyfile(3); `ioreg` y `nvram` con IOKit.framework (`nvram` lee; escribir necesita un driver de NVRAM del firmware que Minkar no tiene) |
| 29 oct 2026 | `lsbom` | Propia, leyendo el formato BOM tal como se ha descrito públicamente; comprobada con una lista de materiales real hecha por las herramientas originales (el paquete de GitHub CLI para el sistema de referencia: sus 247 rutas, tamaños y sumas `cksum`) |
| 29 oct 2026 | `rtsol` | El rtsold de FreeBSD main como `/sbin/rtsol`, sin casper; Darwin acepta los anuncios de router según `net.inet6.ip6.accept_rtadv`, así que solo los solicita |
| 29 oct 2026 | `talk` | talk y talkd de FreeBSD main (como `/usr/libexec/ntalkd`), lanzado por launchd para el puerto UDP 518 mediante `org.minkar.ntalkd` (apagado, como en el sistema de referencia) y launchproxy, el ayudante inetd de launchd, ahora compilado |
| 29 oct 2026 | `kinit`, `klist`, `kdestroy` | Heimdal 7.8.0, el Kerberos que usa el sistema de referencia, compilado estático; las credenciales van a una caché en archivo (`/tmp/krb5cc_<uid>`), ya que la caché API que Heimdal usa por omisión en el sistema de referencia necesita un servicio que Minkar no tiene |
| 29 oct 2026 | `pfctl` | El de OpenBSD 4.3, la versión cuyo pf lleva el kernel de Darwin, sobre el `net/pfvar.h` privado del kernel: los campos de puerto y dirección de Darwin, sin grupos de interfaces, una sola tabla de enrutamiento, sin ALTQ (como en el sistema de referencia); `/etc/pf.os` de la misma versión, `/etc/pf.conf` propio (sin reglas; pf está apagado al arrancar) |
| 29 oct 2026 | `newfs_msdos`, `fsck_msdos`, `mount_msdos` | FAT12/16/32: newfs_msdos y fsck_msdosfs de FreeBSD main (como `fsck_msdos`), con el tamaño del disco de los ioctl de Darwin y el `-v` del sistema de referencia para la etiqueta; `mount_msdos` propio, para el kext msdosfs original, que ahora carga al arrancar (no tenía función de inicio y nada lo cargaba) |
| 29 oct 2026 | `dtrace` | El dtrace-338.100.1 original (el de API 19, CDDL y APSL), compilado sin CoreSymbolication: los proveedores syscall, mach_trap, profile y lockstat, las agregaciones y las bibliotecas D que no necesitan tipos del kernel. Desde el 29 oct 2026 el kernel lleva su CTF (el toolchain compila las herramientas CTF propias de dtrace), así que funcionan `darwin.d`, `copyinstr` y los tipos del kernel. Faltan todavía fbt, el proveedor pid y `-c`, que necesitan nuestro propio CoreSymbolication |
| 29 oct 2026 | `mount_nfs` | El original (NFS-136.100.3, el de API 19) para el cliente NFS de xnu, con `/etc/nfs.conf`; probado hasta la red (aún no hay servidor NFS del que montar). El servidor (`nfsd`, `mountd`) necesita TI-RPC y rpcbind, que el sistema de referencia toma de un framework nunca publicado |
| 29 oct 2026 | `newfs_hfs` | El original (hfs-556), ya en el árbol pero nunca compilado (su Makefile llamaba al programa fsck_hfs): volúmenes HFS+ simples, con journal y que distinguen mayúsculas |
| 29 oct 2026 | `dynamic_pager` | Propio: fija el prefijo de los archivos de intercambio (`vm.swapfileprefix`) para el compresor del kernel, que los crea y borra por sí mismo; las antiguas opciones de tamaño y disparo se aceptan y se ignoran, como en el sistema de referencia |
| 29 oct 2026 | `csrutil` | Propio: `status` lee la configuración de SIP del kernel (`csr_get_active_config`); `enable`, `disable` y `clear` se niegan fuera del Recovery OS, con el mensaje del sistema de referencia |
| 29 oct 2026 | `vsdbutil` | Propio: `-c` lee si un volumen ignora los dueños (`MNT_IGNORE_OWNERSHIP`), `-a` y `-d` lo vuelven a montar con `owners` o `noowners`; `-i` se acepta y no hace nada, como en el sistema de referencia |
| 29 oct 2026 | `kextstat` | Propio, sobre `OSKextCopyLoadedKextInfo` de IOKit: índice, referencias, dirección, tamaños, versión, UUID y dependencias de cada kext cargado; `-b`, `-k` y `-l` |
| 29 oct 2026 | `kextload`, `kextunload` | Propios, sobre `OSKextLoad` y `OSKextUnloadKextWithIdentifier` de IOKit, por ruta o identificador `-b`; una extensión ya cargada vuelve a cargarse sin error, y una `com.apple` con servicios se niega a descargarse como en el sistema de referencia. Corregido de paso: IOKit fallaba ante una petición rechazada (`mach_error_string` no estaba declarada, así que su puntero se truncaba a int), y las codificaciones de hfs están ahora en disco como el plug-in de hfs |
| 29 oct 2026 | `system_profiler` | Propio: SPHardwareDataType, SPNetworkDataType, SPSoftwareDataType y SPStorageDataType como texto, `-xml` o `-json`, `-detailLevel mini` sin lo que identifica la máquina. Para él, los drivers de almacenamiento (AHCI, NVMe, virtio-blk, USB) publican ahora las Device and Protocol Characteristics del sistema de referencia. La plataforma aún no lee SMBIOS, así que `hw.model` es el nombre del procesador y no hay número de serie ni UUID de hardware |
| 29 oct 2026 | `pkgutil` | Propio: los recibos de `/var/db/receipts` (listados, descritos, buscados por ruta, olvidados, grupos, `--volume`), sus listas de materiales leídas con `lsbom`; aún no se tratan los `.pkg` planos (xar) |
| 29 oct 2026 | `systemsetup` | Propio: fecha, hora, zona horaria (con `tzlink`), nombres del equipo y de la subred local (SystemConfiguration), acceso remoto (el trabajo de launchd de sshd), el servidor de hora de `/etc/ntp.conf`; la gestión de energía, el disco de arranque y la hora por red figuran como no soportados hasta que existan |
| 29 oct 2026 | `sysadminctl` | Propio: `-addUser` (primer uid libre desde 501, staff, zsh, la carpeta personal con las carpetas del sistema de referencia, `-admin`), `-deleteUser` (procesos, registro, grupos, carpeta personal; `-keepHome`), `-resetPasswordFor`, con libpw y pwd_mkdb; `-secureTokenStatus` y `-guestAccount status` |
| 29 oct 2026 | `networksetup` | Propio, sobre la API de configuración de red de SystemConfiguration: servicios (listar, ordenar, crear, renombrar, quitar, activar), métodos IPv4 e IPv6, DNS, dominios de búsqueda, proxies, MTU, medios, ubicaciones. Para él, nuestro `dhcpclient` sigue ahora el `Setup:` de configd (manual, INFORM, BOOTP, DHCP, apagado; también IPv6). Aún sin Wi-Fi |
| 29 oct 2026 | `disklabel` | Propio: etiquetas de disco en el esquema de etiquetas de la plataforma de referencia (`-create`, `-properties`, `-status`), la cabecera de IOAppleLabelScheme.h del SDK, que el esquema de etiquetas del kernel publica como un medio hijo |
| 29 oct 2026 | `bless` | Propio (el usuario lo eligió frente al original): información de arranque HFS+ (`--info`, `--folder`, `--file`, `--unbless`) con getattrlist/setattrlist, `--getBoot`; `--setBoot` falla (salida 3) hasta que se puedan escribir variables NVRAM |
| 30 oct 2026 | `hdiutil` | Propio, sobre nuestro kext MinkarDiskImage (un archivo de imagen como IOBlockStorageDevice, así que `/dev/diskN` y sus particiones): `create` (raw, GPT o ninguno, HFS+ o MS-DOS, `-srcfolder`), `attach` (raw y UDIF de lectura y escritura, montado en /Volumes), `detach`, `info`, `imageinfo`; aún no UDIF comprimido |
| 30 oct 2026 | `emond` | Propio: reglas en /etc/emond.d/rules (eventTypes, criterion con sus operadores y coincidencias parciales, Log, RunCommand, SendEmail; `${event.key}` y `${builtin:...}`), el evento de arranque y los eventos encolados en /var/db/emond/events; launchd lo lanza para sus clientes, como en el sistema de referencia |
| 30 oct 2026 | `fs_usage` | Propio, sobre la traza del kernel (kdebug): llamadas al sistema emparejadas por hilo con las rutas buscadas, F=, B= y errores, procesos nombrados por el mapa de hilos y los eventos exec; `-w`, `-f`, `-e`, `-t`, por pid u orden |
| 30 oct 2026 | `pmset` | Propio, sobre las funciones de energía de IOKit y nuestro propio `powerd` (el servicio com.apple.PowerManagement.control, aprobado): ajustes (activos, personalizados, capacidades, por fuente de alimentación, restoredefaults), aserciones, fuentes de alimentación, eventos programados y periódicos, sleepnow, displaysleepnow, disablesleep, noidle. De paso: la plataforma lee SMBIOS (modelo, número de serie, IOPlatformUUID; el cargador guardaba los primeros bytes de las tablas de configuración en lugar de sus direcciones), así que gethostuuid() y las CFPreferences por equipo ya no se cuelgan; y como la plataforma aún no puede dormir (sin ACPI S3), lo dice y powerd desactiva el reposo |
| 30 oct 2026 | `nfsd` | El original (NFS-136.100.3, APSL; mountd dentro) para el servidor NFS de xnu, sobre nuestra propia capa oncrpc (rpcb_set/unset por transporte y dirección sobre el Sun RPC de libinfo) y un subconjunto de ServiceManagement sobre launchd; los UUID de volumen de getattrlist; etiquetas de launchd org.minkar.*. Con nuestro propio `rpcbind` (portmapper 2, rpcbind 3 y 4), lanzado bajo demanda. Probado con un montaje NFSv3 en la misma máquina. Para él, launchd actúa ahora sobre KeepAlive PathState, WatchPaths y QueueDirectories (launchctl se los pasaba a un proveedor de eventos del sistema de archivos que no tenemos), lo que también lanza emond para sus clientes. Faltan rpc.lockd (bloqueos de archivos NFS) y los transportes IPv6 |
| 31 oct 2026 | `notifyutil`, `launchctl kickstart` | `notifyutil` es el original (Libnotify, APSL), el cliente de notifyd: publicar, esperar y leer notificaciones y estados (estaba en el árbol sin compilar). `launchctl kickstart [-k] [-p] system/etiqueta` (también `gui/uid/` y `user/uid/`) es propio en el launchctl original: arranca un servicio ya, o con `-k` lo mata y lo vuelve a arrancar cuando ha salido; `-p` imprime el nuevo PID |
| 31 oct 2026 | `mount_exfat` y el kext exfat | Propios, a partir de la especificación de exFAT publicada por Microsoft; lee desde el 30 oct 2026 y escribe desde el 31 oct 2026. Los datos de archivo pasan por la caché unificada de búferes (`cluster_read`, `cluster_write`, `cluster_pagein`, `cluster_pageout`: se pueden ejecutar programas desde él, escrituras por mmap); clústeres del mapa de bits de asignación, archivos en cadenas FAT (un archivo contiguo venido de otro sistema sigue así mientras pueda crecer en el sitio); conjuntos de entradas creados, actualizados, movidos y borrados en el sitio, un directorio que crece con un clúster a cero; más allá de la longitud válida de un archivo se lee como ceros (read y mmap), y esos ceros se escriben antes de que el archivo cambie; un archivo borrado estando abierto conserva sus clústeres hasta que se cierra; los nombres se buscan sin distinguir mayúsculas con la propia tabla de mayúsculas del volumen, acentos en cualquiera de las formas Unicode, rechazados los caracteres que exFAT prohíbe; horas con su desfase UTC, escritas en hora local con él; los indicadores de oculto y solo lectura; sectores de 512 a 4096 bytes; el indicador de sucio mientras está montado en lectura y escritura, `mount -u -w` y `-r`; TexFAT (dos FAT) se monta solo en lectura. `mount_exfat` acepta `-u`, `-g` y `-m` como `mount_msdos`. El bundle `exfat.fs` y nuestro propio `exfat.util` (etiqueta, un UUID de versión 3 a partir del número de serie) permiten a diskarbitrationd montar discos exFAT en /Volumes; `diskutil eraseVolume ExFAT`. Probado contra el driver exFAT de Linux en ambos sentidos (lo que uno escribe lo lee el otro), con `fsck_exfat` tras cada cambio, y con volúmenes dañados montados en lectura y escritura (sin pánico) |
| 30 oct 2026 | `fsck_exfat`, `newfs_exfat` | Propios, a partir de la especificación de exFAT publicada por Microsoft (las herramientas libres son GPL). `newfs_exfat` crea la región de arranque y su copia, una FAT, el mapa de bits de asignación, nuestra propia tabla de mayúsculas (comprimida) y el directorio raíz; el driver exFAT de Linux monta y escribe lo que crea. `fsck_exfat` comprueba la región de arranque frente a su copia, los archivos del sistema, cada conjunto de entradas (suma de comprobación, hash del nombre, cadenas de clústeres: válidas, sin bucles, sin compartir) y el mapa de bits; repara la región de arranque principal, el mapa de bits (no mientras una cadena o un directorio rotos oculten lo que contenían), sumas y hashes, y el indicador de sucio; `-q` como el del sistema de referencia, para diskarbitrationd. Probado con volúmenes escritos por Linux |
| 30 oct 2026 | `leaks` | Propio: detiene el proceso, lista sus bloques de malloc con los propios enumeradores de libmalloc (aquí libmalloc no está en la misma dirección en todos los procesos, así que la lista de zonas y los enumeradores se trasladan por la diferencia), y marca lo que alcanzan sus raíces (su memoria escribible fuera de malloc, una pila solo desde el puntero de pila de su hilo hacia arriba, los registros de los hilos, los nombres de las zonas): el resto se ha perdido, y se muestra con tamaño, zona y primeros bytes. `-quiet`, `-nocontext`, por pid o nombre; estado de salida 1 si hay fugas. Aún no está dónde se reservó cada fuga (el MallocStackLogging del sistema de referencia) |
| 30 oct 2026 | `autodiskmount` | Propio (el sistema de referencia lo conserva por compatibilidad, sin opciones): pide a diskarbitrationd que monte los volúmenes que no están montados, con las reglas que sigue cuando aparece un disco (una partición EFI sigue sin montar, igual que los volúmenes que fstab marca noauto); `-v` dice qué montó. De paso, `hdiutil create` espera a que diskarbitrationd examine el disco nuevo (lo retenía un momento y el detach podía fallar por ocupado) y monta por él un volumen `-srcfolder` |
| 30 oct 2026 | `diskutil` | Propio, sobre DiskArbitration: `list`, `info` (y `-plist`), `mount`, `mountDisk`, `unmount`, `unmountDisk`, `eject`, `rename`, `verifyVolume`, `repairVolume`, `eraseVolume` (JHFS+, HFS+, JHFSX, HFSX, FAT32, FAT16, FAT12) y `listFilesystems`. DiskArbitration es el original (framework y `diskarbitrationd`, DiskArbitration-297.70.1, APSL) con los bundles `hfs.fs` y `msdos.fs` (`hfs.util` y `msdos.util` de los proyectos hfs y msdosfs originales); `hdiutil attach` monta a través de él. De paso: la raíz PCI cuelga de la raíz del árbol de dispositivos, como en el sistema de referencia (DiskArbitration nombra el bus de un disco por su ruta en él); `fsck_msdos -q` (solo si el volumen se desmontó limpio); `mount` pasa una opción que empieza por «-» al ayudante como propia (`-m=755` como `mount_hfs -m 755`); `mount_msdos` sin ninguna de `-u`, `-g` y `-m` deja usar el volumen a todos, como en el sistema de referencia. Aún sin verbos de particionado, APFS ni RAID |
| 29 oct 2026 | `localedef` | El de FreeBSD main (el de Nexenta, BSD-2), escribiendo lo que lee la libc de Darwin: LC_CTYPE como `RuneMagA` y `LC_MESSAGES/LC_MESSAGES` (nuestro `darwin_out.c`); LC_COLLATE en orden de punto de código, como todos los locales de Minkar (las tablas de ordenación de Darwin no están documentadas públicamente); manual propio (el del proyecto es CDDL) |

`btop` y `htop` también dibujan sus cuadros y gráficas UTF-8 en la consola
del kernel: tiene caracteres de línea, bloques y puntos Braille.

### Lo que falta y problemas conocidos

- `getconf DARWIN_USER_DIR` y `DARWIN_USER_CACHE_DIR` fallan con EIO (sin
  servicio de directorios por usuario).
- `sshd` registra un acceso pero no su salida: `last` muestra las sesiones
  ssh como todavía abiertas.
- Hasta el 28 oct 2026 las entradas de permiso de una ACL nunca se aplicaban
  (el kernel no tenía GUID para un proceso sin un demonio que lo resolviera),
  así que solo root podía leer los registros de acceso: `last` y `finger` no
  mostraban nada a los demás usuarios.
- La terminal de la consola no tiene nodo en `/dev` (el dispositivo serie
  que hay detrás), así que `ps` muestra `??` para getty.
- En cola (el kernel ya tiene lo que necesitan): `newfs_msdos` y
  `fsck_msdos` de FAT (los de FreeBSD) y nuestro propio `mount_msdos`, ya
  que `msdosfs` está en el kernelcache; las herramientas de cuotas (las de
  FreeBSD, para el formato de archivo de cuotas de XNU); `pfctl` (uno que
  hable los ioctl de pf de XNU); `mount_nfs` y `nfsd` (XNU tiene el cliente
  y el servidor NFS).
- exFAT: están `fsck_exfat`, `newfs_exfat`, `mount_exfat` y nuestro propio
  driver del kernel (lectura y escritura).
- Fuera hasta después de Minkar 19.6.0, sin driver con una licencia utilizable: SMB
  (`mount_smbfs`; Samba es GPL y el smbfs de FreeBSD ya no existe) y APFS
  (`fsck_apfs`, `mount_apfs`, `newfs_apfs`; sin implementación abierta). El
  sistema de archivos de Minkar es HFS+. También `fdesctl`.
- Las herramientas del sistema de referencia que necesitan una fase
  posterior figuran en ella, en [ROADMAP.es.md](ROADMAP.es.md).

## Usuario avanzado

- `tar`, `ssh` y `curl` están instalados.
- `zsh` 5.9, compilado y probado el 26 oct 2026: módulos enlazados dentro,
  la biblioteca de funciones, en `/etc/shells`.
- `make` es BSD make (bmake 20260704, con sus archivos mk en
  `/usr/share/mk`) desde el 27 oct 2026, elegido frente a GNU make.
- `git` 2.56.0 (GPL-2.0, aprobado el 27 oct 2026) desde el 28 oct 2026,
  compilado con su propio Makefile y sus ajustes de Darwin: HTTPS con
  libcurl, `git daemon`, nombres de archivo con acentos en HFS Plus
  (core.precomposeunicode), los manuales y el autocompletado. Sin las
  órdenes en Perl, Python o Tcl (`git svn`, `send-email`, `gitk`,
  `git gui`).
- Ruby 3.3.9 (`/usr/bin/ruby`), Python 3.13.18 (`/usr/bin/python3`) y
  Perl 5.40.5 (`/usr/bin/perl`) desde el 11 nov 2026, como el sistema de
  referencia trae sus lenguajes de scripts.
- `/bin/sh` ejecuta bash como shell POSIX desde el 11 nov 2026, como en el
  sistema de referencia: un pequeño lanzador y `/private/var/select/sh`, que
  nombra el shell; el `sh` de FreeBSD sigue como `/usr/libexec/bsdsh`.

## Herramientas de la plataforma

- `plutil`, `defaults`, `sw_vers` y `ditto`, probadas en la imagen el
  26 oct 2026 (`sw_vers` imprimía espacios en lugar de los tabuladores del
  sistema de referencia y una línea en blanco al final).
- `chmod` tiene la sintaxis de ACL del sistema de referencia (`+a`, `-a`,
  `=a#`, `-E`, `-C`, `-N`, `-i`, `-I`) desde el 26 oct 2026, y el kernel
  aplica las entradas.
- `open` es propia desde el 4 nov 2026, sobre LaunchServices (aplicaciones,
  archivos, URL, `-a`, `-b`, `-e`, `-t`, `-n`, `--args`, `-R` para mostrar
  en Files).
- `hdiutil` es propia desde el 30 oct 2026 (`create`, `attach`, `detach`,
  `info`, `imageinfo`), con el kext MinkarDiskImage presentando las imágenes
  de disco como discos.

## Datos del sistema

- **Locales** (27 oct 2026): 56 locales UTF-8 en `/usr/share/locale`,
  hechos por `tools/locale/mklocales.py` en los formatos que lee la libc de
  Darwin: clases de caracteres, mapas de mayúsculas y anchos de Unicode 16;
  hora, moneda, números y mensajes de las definiciones CLDR de FreeBSD;
  ordenación por punto de código.
- **Zonas horarias** (27 oct 2026): IANA tzdata 2026e compilado con el
  `zic` de tzcode, dispuesto como en el sistema de referencia
  (`/var/db/timezone/zoneinfo`). Hasta entonces todas las zonas eran UTC.
- **Páginas de manual** (27 oct 2026): las páginas que traen las fuentes de
  las órdenes; toda orden tiene una, o figura como todavía sin ella.
- **libiconv** (27 oct 2026): el Citrus de FreeBSD, como en el sistema de
  referencia. El de GNU que había antes exportaba solo `libiconv_open`, no
  `iconv_open`, y el SDK no tenía `<iconv.h>`. `/usr/lib/libiconv.2.dylib`
  lleva enlazados sus 23 módulos de codificación, con 687 tablas de
  conversión en `/usr/share/i18n`; los usan libxml2 y fontconfig de
  CoreText. UTF-8-MAC (28 oct 2026, propio, a partir de los datos de
  Unicode): los nombres descompuestos que guarda HFS Plus, compuestos de
  nuevo para git.
