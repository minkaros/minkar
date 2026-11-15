# Frente de GPU: RDNA2 (la RX 6600 del PC 1)

*Traducción de [GPU-RDNA2.md](GPU-RDNA2.md); el documento en inglés es el de referencia.*

La segunda familia de GPU del [ADR 0002](ADR/0002-GPU-First-Family.es.md), tras
GCN 1.0 (G1–G4 en la FirePro D500 del PC 2). La tarjeta del PC 1 es una AMD
Radeon RX 6600: Navi 23 (PCI 1002:73ff), IP gráfica GFX 10.3, motor de pantalla
DCN 3.0.2 (el `dcn302` de amdgpu), 8 GB de VRAM, VBIOS 115-D534P00-100. Hoy el
PC 1 dibuja con `IOGOPFramebuffer`, sobre el framebuffer que deja el GOP del
firmware; nada está acelerado, y la pantalla no puede cambiar de modo ni
apagarse.

Decidido el 2026-11-07 (mantenedor): la RX 6600 va después del lote de ese
día, y su primer objetivo es la pantalla nativa, como lo fue G1 en la D500.

## Qué cambia respecto a GCN 1.0

- **El motor de pantalla** es DCN, no DCE: cada tubo es un lector de planos
  (HUBP) que alimenta un generador de tiempos (OTG), con relojes (DCCG) y
  salidas (DIO) propios. Un modo se fija programando esos bloques, no solo con
  las tablas de órdenes de la VBIOS; amdgpu lo hace en su capa `dc`, con un
  microcontrolador de pantalla (DMCUB) que ejecuta firmware de AMD.
- **El firmware lo carga el PSP**, el procesador de seguridad: el del DMCUB,
  el de los motores DMA (SDMA 5.2) y el microcódigo del bloque gráfico pasan
  por él, firmados. Arrancar el PSP (con su propio firmware, `sos` y `ta`) va
  antes que todos ellos.
- **La energía y los relojes** son del SMU (SMU 11.0.7), también a través del
  PSP.
- El espacio de registros se describe por bloques IP y segmentos
  (`dimgrey_cavefish_ip_offset.h`); MinkarDRM toma los registros que usa de
  las cabeceras MIT de drm-kmod con un generador
  (`tools/drm-import/dcn302-regs.py`).

Los archivos de firmware son de AMD (`dimgrey_cavefish_*.bin` en
linux-firmware), redistribuibles con su propia licencia: se descargan en una
versión fijada con su SHA-256, como los de Tahiti (ADR 0002).

## Pasos

- [x] **R0 — El motor de pantalla, leído.** MinkarAMDGPU nombra la familia y,
  solo leyendo, publica lo que muestra cada tubo (`MinkarDisplayPipes`: si está
  encendido, el tamaño activo y total, la dirección del plano, el ancho de
  línea y el formato de píxel, y el lugar de la VRAM en el espacio de
  direcciones del motor). *Hecho (2026-11-08):* en el PC 1, el tubo 0 muestra
  3440x1440 (3600x1481 con los intervalos en blanco), ARGB8888, desde el
  principio de la VRAM (0x8000000000 en el espacio del motor; 8 GB). El campo
  del ancho de línea vale 3456, como informa el GOP, mientras que el `dc` de
  amdgpu escribe el ancho menos uno: R1 lo aclara.
- [x] **R1 — La pantalla tomada, en el modo del firmware.** Un framebuffer
  propio del driver en VRAM, dentro de la ventana de la CPU (BAR 0, 256 MB en
  el PC 1), y la dirección del plano llevada a él en un intervalo vertical en
  blanco; el servidor de ventanas dibuja ahí. `IOGOPFramebuffer` sigue de
  respaldo, como hace `MinkarAMDFramebuffer` en GCN 1.0. No hace falta
  firmware: el tubo que encendió el GOP sigue funcionando. Criterio: el
  escritorio del PC 1 dibujado desde el búfer del driver, el ancho de línea
  confirmado con un patrón de prueba, cambios de página en el intervalo
  vertical. *Hecho (2026-11-15, `603ab40577`):* en el PC 1
  `MinkarAMDFramebuffer` toma el tubo 0 y muestra su propio framebuffer en
  la VRAM 0x2000000, con el modo nativo a 49,999 Hz; el servidor de
  ventanas, el Dock y la ventana de inicio de sesión dibujan ahí, y el
  mantenedor lo vio sin parpadeos. Al arrancar, el driver cambia 30 veces
  entre el búfer del firmware y el suyo: los 30 cambios, en el intervalo
  vertical y a un fotograma (20,0 ms) uno de otro (`MinkarFlipTest`). El
  ancho de línea: un patrón de prueba a 3520 píxeles por fila, distinto de
  los 3456 del firmware (`minkaramd_pattern=1`), salió recto, medido con
  una regla, tanto con el campo igual al ancho como con el ancho menos uno,
  como lo escribe el `dc` de amdgpu: el motor redondea el ancho de un plano
  lineal hacia arriba a 64 píxeles, así que las dos convenciones dan la
  misma imagen. El driver conserva la del firmware.
- [ ] **R2 — Modos.** El EDID de la pantalla por el AUX y el DDC del motor; el
  PSP arrancado y el firmware del DMCUB cargado; modos fijados programando el
  OTG, el plano, los relojes y la salida (HDMI y DisplayPort, con
  entrenamiento del enlace); la pantalla apagada y encendida de nuevo;
  conexión en caliente. Después, el audio por HDMI y DisplayPort (traído
  aquí desde AU1 el 14 nov 2026): los pines digitales que MinkarHDA ya
  encuentra, activados con el ELD de la pantalla. Criterio: `wsctl mode`
  cambia el modo del PC 1, la pantalla se duerme y despierta, y un seno se
  oye por la pantalla.
  *En curso (2026-11-15), sin el PSP (decidido ese día con el mantenedor:
  primero las partes HDMI de R2):* la VBIOS de DCN 3.0.2 no guarda ninguna
  de las tablas de comandos de la pantalla salvo la de apagado de bloques;
  las ejecuta el microcontrolador de pantalla (DMCUB), y el GOP del
  firmware lo deja funcionando con su buzón listo (en VRAM más allá de la
  ventana de la CPU, escrito por MM_INDEX/MM_DATA). Hecho en el PC 1: el
  EDID por DDC con el I2C del motor (`a19fcb683a`); la pantalla se duerme y
  despierta con el gestor de pantalla, con su transmisor HDMI apagado y
  encendido por el microcontrolador (`4e462ab890`); al volver un KVM o un
  cable, la línea de conexión lo detecta y el transmisor se enciende de
  nuevo (`7541ea2c93`); el firmware enciende la pantalla como DVI, y el
  driver pasa el enlace a HDMI (con un infoframe AVI) y prepara el audio:
  el ELD al endpoint del códec, el DTO, N/CTS y los paquetes de audio, y
  las salidas digitales de MinkarHDA, un dispositivo propio en Sound con
  el nombre de la pantalla; un seno se oyó por la pantalla (`1d2ea76c1f`,
  `688e101c2a`). Falta: los modos. Un refresco se fija por el
  microcontrolador (SetPixelClock entre el apagado y el encendido del
  transmisor, `a00c2b2ae8`), pero 60 Hz necesita subir DISPCLK por encima
  de los 304 MHz que deja el firmware, y eso lo hace el SMU, que no
  contesta hasta que el PSP carga su firmware: esos modos aún no se
  ofrecen. Encender una pantalla que el firmware no encendió (el PC 1
  arrancado con el KVM en otro equipo) necesita los mismos relojes.
  DisplayPort espera a una pantalla DisplayPort.
- [ ] **R3 — Memoria y DMA.** La traducción de direcciones de los hubs de
  memoria (la GART y los espacios de direcciones de GPU por proceso), SDMA 5.2
  con su firmware, las interrupciones (IH) y el cliente de usuario; el
  servidor de ventanas presenta sus fotogramas por DMA, como hizo G2 en
  GCN 1.0. Criterio: las comprobaciones de G2 en el PC 1.
- [ ] **R4 — El bloque gráfico y Mesa.** GFX 10.3 con su microcódigo, la
  libdrm de Minkar y RADV de Mesa encima; apps de Vulkan en ventanas; el
  backend de Vulkan del compositor. Criterio: las comprobaciones de G3 y G4 en
  el PC 1.
- [ ] **R5 — Relojes y energía.** El SMU, a través del PSP: la GPU a sus
  relojes completos, y estados de reposo. Después, las mediciones de G4.6 otra
  vez.

Los modos de R2, y R3 y los siguientes, esperan al PSP, la pieza más
grande de esta familia: su arranque se estudia primero, en el `psp_v11_0`
de amdgpu.
