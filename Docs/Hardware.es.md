# Hardware de referencia

*Traducción de [Hardware.md](Hardware.md); el documento en inglés es el de referencia.*

Minkar se prueba en una lista corta de máquinas: una máquina virtual de QEMU
y dos PC físicos documentados. Los fallos en ellas se corrigen primero; el
resto del hardware es "lo mejor posible". Esta página recoge lo que funciona
en cada máquina, lo que aún no, y cómo se encontró cada arreglo. El
[roadmap](ROADMAP.es.md) (Fase 1) sigue la lista en conjunto.

| Máquina | Papel | Estado |
|---|---|---|
| VM de QEMU | Cada cambio: `boottest`, la suite de tests local, la CI | Arranca hasta el login con TCG y KVM |
| PC 1: Gigabyte H270N-WIFI | Primer PC físico | Arranca hasta el login desde SATA; pantalla, teclado y red funcionan |
| PC 2: Intel Xeon E5-2697 v2 | Segundo PC físico (gate M1); hardware original del fabricante, para medir la referencia | Arranca hasta el login desde NVMe; 24 hilos, pantalla, teclado y red funcionan |
| PC AMD | Un procesador AMD | Pendiente: aún no hay hardware (ver abajo) |

**Requisito (decidido el 7 nov 2026): un procesador x86-64 con AVX** (Intel desde Sandy Bridge, 2011; AMD desde Bulldozer y Jaguar, 2011 y 2013). El software hecho para la plataforma de referencia puede usar AVX sin comprobarlo, y el JIT de JavaScriptCore lo hace, porque todas las máquinas en las que corre el sistema de referencia lo tienen; los procesadores más antiguos quedan para sistemas operativos más genéricos. Las VM emulan un Ivy Bridge (`-cpu IvyBridge`). Con TCG, QEMU emula AVX desde la versión 7.2 (Ubuntu 24.04 trae la 8.2); con un QEMU más antiguo la VM arranca, pero el código que usa AVX se detiene con una instrucción no válida.

## Máquina virtual de QEMU

Máquina `q35`, firmware OVMF, disco AHCI, `qemu-xhci` con teclado y tableta
USB, tarjeta de red e1000e o e1000; `-cpu IvyBridge` (AVX, el requisito de
arriba; era `-cpu Westmere` hasta el 7 nov 2026; con `-cpu host` QEMU falla
en un MSR que lee el kernel).

- **TCG y KVM** (27 oct 2026): el kernel leía dos MSR que KVM no tiene
  (número de núcleos e hilos, flex ratio) antes de poder tratar un fallo;
  con un hipervisor ahora usa CPUID y el PIT. Las tres VM de la suite de
  tests corren con KVM (el driver xHCI ahora duerme hasta su interrupción
  en vez de sondear cada milisegundo): la suite completa tarda unos 9
  minutos en vez de 32. *Parcial:* en los runners de la CI el kernel aún no
  arranca con KVM, así que `boottest` usa TCG salvo que se pida KVM.
- **SMP** (30 oct 2026): corren todos los procesadores que la MADT da como
  habilitados. Tres arreglos: ACPICPU registra los procesadores en el kernel
  y los arranca, primero el de arranque, como hace la plataforma ACPI del
  sistema de referencia (`startCPU` era un esqueleto, y cada CPU creaba su
  propio controlador de interrupciones de uno, lo que fijaba el máximo del
  kernel en un procesador); el número de procesadores del kernel se fija
  solo cuando todos están registrados (la commpage toma entonces la cuenta
  de los lógicos, y libmalloc crea un magazine para cada uno: un hilo en
  cpu1 leía más allá del último); y un AP pasa a modo largo en
  `slave_pstart` (el BSP llega de UEFI ya en modo largo, así que el cambio
  quedó fuera del camino común y un AP hacía triple fault). El kernel
  también numeraba paquetes y dies dividiendo los IDs de APIC por el número
  real de hilos; con 3 o 6 núcleos por paquete y más de un paquete, los IDs
  dejan huecos y salían demasiados paquetes (un pánico en
  `x86_validate_topology`): el paquete sale ahora del desplazamiento de la
  hoja de topología de CPUID. Probado con 2, 3, 4 y 5 procesadores, 2
  paquetes de 3 núcleos (con y sin 2 hilos por núcleo) y un paquete de 4
  núcleos de 2 hilos; las VM de la suite de tests corren con 2. Correr en
  dos procesadores mostró dos cosas más: `kprintf` escribía directamente al
  puerto serie mientras la consola también escribía ahí, y las líneas de dos
  procesadores salían mezcladas carácter a carácter (ahora un mensaje sale
  con el cerrojo serie de la consola tomado); y `hdiutil` buscaba el
  dispositivo de la imagen nueva antes de que terminara su registro, ahora
  terminado en otro procesador (lo espera). Los números de disco tampoco son
  fijos ya: los drivers se emparejan en paralelo, como en el sistema de
  referencia.
- **Límites** (30 oct 2026): el kernel admite 64 procesadores (`MAX_CPUS`,
  una máscara de 64 bits) con IDs de APIC local hasta 255 (xAPIC). Una
  máquina más grande corre en sus 64 primeros, y un procesador con un ID de
  APIC mayor queda fuera, ambas cosas indicadas en el log (el kernel daba
  pánico con una topología de menos procesadores de los que tienen sus
  paquetes). Probado con 64 procesadores (corren todos) y 128 (corren 64).
- **Las lecturas NVMe a través del dispositivo de bloques son lentas**
  (alrededor de 1 MB/s con lecturas de 4 KB, igual con uno o dos
  procesadores): pendiente de mirar.
- **Test de consola** (`run.sh console`): arranca una imagen con los
  argumentos de arranque de un PC y sin consola serie, escribe en el teclado
  USB a través de QMP y lee la pantalla con la fuente de la consola.
- **Fallo conocido:** sin `pci_keep_console=1` el arranque se detiene
  cuando la configuración PCI vuelve a encender la consola. Las imágenes de
  PC y el test de consola pasan ese argumento.

## PC 1: Gigabyte H270N-WIFI

| Pieza | Dispositivo | Estado |
|---|---|---|
| Placa | Gigabyte H270N-WIFI (Intel H270), firmware UEFI de AMI | Arranca |
| CPU | Intel Core i7-7700 (4 núcleos, 8 hilos) | Los ocho corren (SMP, 30 oct 2026): `hw.ncpu` 8, 4 físicos; ocho bucles ocupados llegan al 99,8 % |
| Disco | SATA a través de AHCI (`MinkarAHCIPort`) | Sistema de archivos raíz, fsck |
| Disco | NVMe (M.2), "SK 256GB" (Silicon Motion, PCI 126f:2263) | Sistema de archivos raíz: arranca desde él (`MinkarNVMe`, 29 oct 2026), finalizaciones por interrupción |
| USB | xHCI (`MinkarXHCIPort`), hub USB 2 de un conmutador KVM | Teclado y ratón, directamente y a través del conmutador |
| Pantalla | Framebuffer GOP, ultrapanorámica Xiaomi de 34" | Consola de texto nativa a 3440x1440 |
| Red | Intel I219-V (LAN del PCH) | ping en la LAN (`MinkarE1000`) |
| Red | Intel I211 | DHCP y ping en la LAN (`MinkarE1000`) |
| Wi-Fi | Intel (M.2) | Sin driver |

Sus imágenes se compilan con `GRAPHICS_MODE=native` y los argumentos de
arranque `-v debug=0x14e keepsyms=1 kprintf_console=1 pci_keep_console=1
ahci_debug=1`, y se escriben en un disco SATA. `LOADER_PAUSE=10` deja la
salida del cargador en pantalla para las fotos.

### Pendiente

- IOKit no queda en silencio en 30 segundos, así que algunos drivers (el de
  la pantalla) arrancan después del prompt de login; `kextstart` nombra
  ahora los servicios que siguen ocupados.
- SMP (30 oct 2026): la primera imagen con él dio pánico aquí, en la
  comprobación de topología: los hilos de un núcleo salían del número real
  de hilos de un paquete (la hoja de topología de CPUID, 8) entre el límite
  de núcleos de la hoja 4 (8), un hilo por núcleo, así que la topología
  mostraba 8 núcleos donde se esperaban 4. La hoja 4 de QEMU da la cuenta
  real, así que allí nunca se vio. Los hilos de un núcleo salen ahora del
  nivel SMT de la hoja de topología; corren los ocho hilos.
- NVMe (30 oct 2026, el SK hynix de 256 GB): unos 1,4 GB/s leyendo el
  dispositivo en bruto y 1,1 GB/s escribiendo un archivo, casi todas las
  finalizaciones por interrupción.
- Energía (30 oct 2026): el modelo y el UUID desde SMBIOS, `powerd` y
  `pmset` (ajustes, aserciones, programaciones, reposo rechazado por no
  admitido) funcionan. El reposo de la pantalla no hace nada: el framebuffer
  GOP no puede apagar la pantalla (sin driver de GPU); dejarla en negro está
  por hacer. El botón de encendido no hacía nada: IOACPIFamily toma ahora el
  botón de encendido de ACPI (el evento fijo, o un dispositivo PNP0C0C) y
  `powerd` pide un apagado ordenado (reposo en su lugar donde la máquina
  puede dormir y "Sleep On Power Button" está activo). Aquí (hw-test38): el
  firmware tiene tanto el evento fijo como un dispositivo PNP0C0C; una
  pulsación apaga y el PC se desconecta; los ajustes de `pmset` sobreviven a
  un reinicio.
- `shutdown`, `reboot` y `poweroff` tardaban 20 segundos más desde que llegó
  DiskArbitration: `diskarbitrationd` solo deja de montar con SIGTERM, y a
  su job le faltaba la clave `EnableTransactions` del original, con la que
  launchd lo termina enseguida cuando está inactivo. Ahora unos 2 segundos,
  también aquí (hw-test38: `reboot` y `shutdown -h now`, que apaga).
- `dmesg` guarda 16 KB, como un kernel release del sistema de referencia:
  un arranque detallado empuja su principio fuera. Las imágenes de prueba de
  PC arrancan con `msgbuf=1048576` para eso (hw-test38: unos 100 KB desde la
  primera línea).
- IPv6 en la LAN: RDNSS (los servidores de nombres del router en
  resolv.conf) y respuestas DNS sobre IPv6, que la red de usuario de QEMU no
  puede dar.
- Arrancar sin `pci_keep_console=1` (ver el fallo conocido de QEMU).

### Cómo se llegó

**NVMe** (29 oct 2026). `MinkarNVMe`, escrito a partir de la especificación
NVM Express, encontró el SSD (un Silicon Motion 126f:2263 "SK 256GB"), la
imagen escrita en él arrancó con su sistema de archivos raíz ahí, y de 958
finalizaciones 19 llegaron por el temporizador que sustituye a una
interrupción tardía (`ioreg -r -c MinkarNVMe -l`). El SATA y el SSD Intel
siguen visibles.

**Cargador** (26 oct 2026). Desde el disco SATA, el cargador leía el kernel
de la partición EFI de otro disco y luego sobrescribía memoria del firmware:
el kernel se copiaba a 1 MB mientras corrían los boot services (el firmware
de QEMU resulta que deja ese rango libre). Después el kernel reiniciaba la
máquina en su primera página no ejecutable: el firmware de AMI deja
`EFER.NXE` apagado, OVMF lo enciende. Con el cargador arreglado, el kernel
arranca pasando por `bsd_init`, el APIC, las ocho CPU y el UUID raíz, y se
detenía en la configuración PCI, donde la consola está apagada.

**PCI** (26 oct 2026). Con `kprintf_console=1 pci_keep_console=1` para
verlo: la configuración PCI se congelaba reescribiendo la ventana de memoria
del puente x16; IOPCIFamily conserva ahora las ventanas del firmware al
arrancar cuando contienen lo asignado. AHCI y xHCI esperaban después para
siempre su interrupción: el I/O APIC se llamaba según su ID de APIC (2)
mientras todos los dispositivos piden `io-apic@0`; ahora se nombra por su
posición en la MADT. Un pánico intermitente en QEMU (27 oct 2026) venía de
que IOPCIFamily leía la tabla de bits pendientes de MSI-X de un dispositivo
con MSI simple mientras un driver desactivaba su último vector; el MSI
simple llama ahora a su primer vector.

**SATA** (27 oct 2026). Los discos se encontraban, pero IDENTIFY nunca
corría: el puerto se reiniciaba con su motor de comandos en marcha y
`PxSERR` no se limpiaba después, así que el HBA de Intel mantenía BSY
activado; el puerto sigue ahora el orden de la especificación AHCI. **El
PC 1 arranca hasta el prompt `login:`**: raíz en el disco SATA, launchd,
fsck, sshd y una shell. Los drivers AHCI y xHCI se llaman `MinkarAHCIPort` y
`MinkarXHCIPort` (`org.minkar.driver.*`), conservando el copyright de sus
autores.

**USB** (27 oct 2026). Cada endpoint de interrupción IN tiene su propio
anillo y se configuran todos los de un dispositivo (las teclas multimedia
del teclado, las interfaces extra del ratón); se manejan teclados en
protocolo de informes e interfaces con teclado y ratón
(`MinkarUSBHIDInterface`); un dispositivo que no se puede leer ya no inunda
la consola. El teclado detrás del conmutador KVM (un hub USB 2): el driver
borraba una característica de puerto solo de SuperSpeed, el hub respondía
STALL y su endpoint de control quedaba detenido; ahora la envía solo a hubs
SuperSpeed y reinicia un endpoint de control detenido tras cualquier STALL.
Un dispositivo con una sola dirección bulk ya no causa un pánico del kernel.

**Pantalla** (27 y 28 oct 2026). El cargador fija el modo de pantalla desde
`graphics-mode` en el plist de arranque (`GRAPHICS_MODE=`); `native` lee el
EDID de la pantalla, y la ultrapanorámica de 34" funciona a 3440x1440 (3456
píxeles por línea). El texto salía revuelto porque el cargador limpiaba la
pantalla a través de la consola del firmware después de leer el modo, y el
firmware volvía a poner el suyo; el cargador limpia ahora el framebuffer él
mismo y lee el modo al final.

**Red** (28 oct 2026). La I219-V responde al ping en la LAN desde que
MinkarE1000 usa el código e1000 compartido de Intel (el de FreeBSD, BSD-3)
para la LAN del PCH y las I210/I211.

**Terminal de la consola** (28 oct 2026). Retroceso borraba la línea entera
y el historial dibujaba las órdenes en el sitio equivocado. La consola hace
ahora lo que pide el terminfo de xterm (CHA, insertar y borrar caracteres y
líneas, desplazamiento, la pantalla alternativa, 256 colores como sus 8) y
decodifica UTF-8; la edición de línea y el historial se redibujan en su
sitio, y btop dibuja sus recuadros en ASCII (`km.c` cortaba cada byte a 7
bits).

**Después del login** (28 oct 2026, hw-test28). El driver de la pantalla
arrancaba después del prompt de login y volvía a configurar la consola: el
cursor volvía arriba y el prompt se dibujaba encima de los mensajes de
arranque. La consola conserva ahora su texto y su cursor cuando un driver le
da el modo que ya tiene, y una vez está el login los mensajes del kernel no
salen en pantalla (`kern.console_quiet`, que pone `kextstart`; siguen
llegando a `dmesg` y al log, y un pánico o un apagado los muestran de
nuevo). El getty de la consola limpia la pantalla en cada login salvo el
primero.

**Dos tarjetas de red** (28 oct 2026). Pasar el cable de la I219-V a la I211
daba una dirección a la I211, pero el ping salía por la I219-V, que mantenía
su dirección y sus rutas sin enlace. Como en el sistema de referencia,
`dhcpclient` deja ahora las direcciones (IPv4 e IPv6) y las rutas de una
interfaz cuando cae su enlace, y las vuelve a pedir cuando vuelve.

**El intérprete ACPI** (28 oct 2026, hw-test32). ACPICA nunca terminaba de
arrancar: esperaba en `\_SB.PCI0._INI` al Global Lock, que la DSDT del PC 1
toma y la de QEMU no. Leíamos la firma de la FACS ("FACS", cuyo bit de
propiedad está activado) como el cerrojo, así que nunca quedaba libre.
Leyendo el campo del cerrojo, el intérprete arranca y los mensajes de
arranque desaparecen; `shutdown -h` sigue apagando.

**Apagado** (28 oct 2026, hw-test30). `shutdown -h` se detenía con la
máquina encendida: el intérprete ACPI nunca había terminado de arrancar. Sin
intérprete, el kernel toma ahora SLP_TYP de `_S5_` en la DSDT y escribe
SLP_EN él mismo, y el PC 1 se apaga. `arp -a` responde enseguida allí; solo
iba lento mientras la red estaba mal configurada.

**Programas a pantalla completa** (28 oct 2026). La consola dibuja líneas de
recuadro, elementos de bloque y puntos Braille, así que btop muestra sus
recuadros y gráficas como en un emulador de terminal en vez de `+ - | #`.

## PC 2: Intel Xeon E5-2697 v2, AMD FirePro D500

Elegido el 1 nov 2026. El gate M1 se cumplió en él ese día: `minkar-hwcheck`
corrió desatendido con 20 comprobaciones superadas (arranque desde el NVMe
interno, los 24 hilos en línea y cargados durante 30 s con una aceleración
de 12,7x, 40 GB de memoria, 1 GB escrito y leído de vuelta, servicios, ACPI
y un reinicio que volvió en 68 s), tras una hora en reposo con tráfico de
red cada 10 minutos y sin ningún error.

| Pieza | Dispositivo | Estado |
|---|---|---|
| Placa | La placa y el firmware EFI del fabricante | Arranca el cargador de Minkar desde el disco interno |
| CPU | Intel Xeon E5-2697 v2 (12 núcleos, 24 hilos) | Los 24 corren (SMP) |
| Memoria | 40 GB | Tamaño completo visto, ida y vuelta comprobada |
| Disco | NVMe, Samsung MZVLB256HAHQ (PCI 144d:a808) | Sistema de archivos raíz (`MinkarNVMe`) |
| USB | xHCI Fresco Logic FL1100 (1b73:1100) | Teclados, directamente y a través del hub de un conmutador KVM; lector de tarjetas |
| USB | EHCI USB 2 de Intel (8086:1d26) | Sin driver |
| Pantalla | Dos AMD FirePro D500 (Tahiti, GCN 1.0, PCI 1002:679e); framebuffer GOP, ultrapanorámica Xiaomi de 34" | Consola de texto nativa a 3440x1440 en la tarjeta que la lleva |
| Red | Dos Broadcom BCM57762 (14e4:1682) | DHCP, ssh (`MinkarBGE`); 100 Mb/s en un switch de 100 Mb/s |

Sus imágenes son las imágenes de prueba de siempre; `minkar-hwcheck
--display` comprueba la pantalla, y el cargador escribe
`\Minkar\loader.log` en la partición EFI, ya que este firmware nunca muestra
el texto del cargador.

### Pendiente

- Una vez, durante la prueba de almacenamiento de la comprobación de
  hardware, los dos enlaces de red cayeron a la vez y volvieron unos 30 s
  después (con sus direcciones); en una hora de tráfico antes, nunca. Hay
  que volver a verlo antes de culpar al driver o al switch.
- La configuración PCI no asigna ventana a los dispositivos detrás de un
  puente (GPU, red, USB): las del firmware se quedan, así que funcionan,
  pero los drivers no reciben `IODeviceMemory` para las BAR de 64 bits y
  `MinkarBGE` mapea las suyas.
- El dispositivo del botón de encendido (PNP0C0C) no está en el árbol ACPI;
  el evento fijo aún no se comprueba. El controlador EHCI no tiene driver.

### Cómo se llegó

- Hasta el 31 oct 2026 la máquina se colgaba con ruido creciente en la
  pantalla, a veces también en el diagnóstico del propio fabricante. El 1 nov
  2026 resultó que la causa era el conmutador KVM a través del que estaba
  conectada, alimentado por el USB de la máquina: a través de él el EDID de
  la pantalla cambiaba de un arranque a otro (3440x1440 en uno, 1024x768 en
  el siguiente, visto en el log del cargador) y un teclado a veces fallaba en
  Address Device. Conectadas directamente, la pantalla y la máquina
  funcionaron sin problemas durante más de una hora.
- Arreglos encontrados por el camino: un IOGOPFramebuffer por GPU reclamaba
  el mismo framebuffer (ahora solo la tarjeta cuya ventana lo contiene,
  `9c3dbe9873`); Address Device se reintenta tras un reinicio del puerto
  (`c129adf018`); el cargador guarda su salida en la partición EFI
  (`e9a4446222`); el código de topología del kernel cortaba un die cada 24
  IDs de APIC (ahora redondeado a una potencia de dos), y las BAR de 64 bits
  de la familia PCI se mapeaban a mano en `MinkarBGE` (31 oct 2026).

## AMD

Pendiente, sin fecha (30 oct 2026): no hay ninguna máquina AMD a mano. El
kernel reconoce los procesadores AMD desde un cambio anterior y arranca los
demás procesadores igual que en Intel (INIT y SIPI a través del APIC
local), pero Minkar no ha corrido en ninguno. Primer paso sin hardware: QEMU
con un procesador AMD emulado (`-cpu EPYC` con TCG, ya que KVM en un host
Intel no puede presentar uno). Qué mirar: los MSR que solo tiene Intel
(energía, temperatura), el TSC y el arranque de los demás procesadores.
