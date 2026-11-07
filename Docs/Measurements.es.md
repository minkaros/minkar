# Minkar — mediciones

Las cifras de rendimiento de Minkar, tomadas en las máquinas físicas, para
comparar cada paso de la pila gráfica con los anteriores y, desde la puerta
de rendimiento de Minkar 21.0, con el sistema de referencia. Las cifras del
sistema de referencia nunca se suben al repositorio (se toman en el
hardware de su fabricante y se guardan fuera). Inglés:
[Measurements.md](Measurements.md).

## Cómo se toman

- `wsctl stats` (versión 17 de la interfaz): frames, tiempo de composición
  (el frame dibujado), tiempo de frame (dibujado y presentado; con cambios
  de página incluye la espera al retrazado vertical), tiempo del daño a la
  pantalla (desde el primer cambio tras un frame hasta el frame que lo
  muestra), frames tardíos (más de dos refrescos), latencia de entrada a
  pantalla (desde la hora de IOHIDSystem de un movimiento del puntero hasta
  la presentación del frame siguiente). Percentiles de los últimos 1.024
  frames; `wsctl stats reset` empieza una medición.
- `minkar-vkcopy`: lo que cuesta presentar un frame de Vulkan; las copias de
  la GPU cronometradas con sus marcas de tiempo y la de la CPU con su reloj
  (medianas de 100 repeticiones).
- Las escenas las ejecuta un script que reinicia el servidor de ventanas
  con cada backend del compositor (`WSCompositor=pixman|vulkan`), de 15 a
  30 segundos cada una: el escritorio quieto con dos ventanas; una ventana
  de 600x900 deslizándose a 1.200 puntos por segundo con el puntero
  girando; una app de Vulkan presentando a través de una CAMetalLayer;
  alguien moviendo el ratón real.

## G4.6 — el servidor de ventanas sobre Vulkan (3 nov 2026)

Mac Pro 6,1, AMD FirePro D500 (Tahiti, PCI 1002:679e; la que lleva la pantalla), una pantalla a 3440x1440 y 60 Hz (la
escena del ratón a 1152x864, 75 Hz), ratón y teclado a través de un KVM a
unos 690 eventos del puntero por segundo. **La GPU funciona con los relojes
que deja el firmware** (memoria a 150 MHz, motor a 450 MHz, de 1.270 y 725
MHz): su gestión de energía aún no está hecha, así que toda cifra que toca
la VRAM es varias veces más lenta que el hardware. Son una línea base, para
medir de nuevo cuando estén los relojes.

| Escena | | pixman | Vulkan |
| --- | --- | --- | --- |
| Escritorio quieto | frames, CPU del servidor | 0, 0 | 0, 0 |
| Ventana deslizándose | composición p50 / p95 | 0,5 / 0,9 ms | 1,8 / 2,0 ms |
| | frame, presentado | 3,0 ms (copiado, sin esperar) | 16,6 ms (espera al retrazado vertical) |
| | del daño a la pantalla p50 / p95 | 12,9 / 19,9 ms | 25,5 / 32,9 ms |
| | frames tardíos | 0 de 1.001 | 12 de 975 |
| | CPU del servidor | 1,2 s en 15 s | 2,4 s en 15 s |
| App de Vulkan | composición p50 | 14,4 ms | 1,0 ms |
| | CPU del servidor | 13,4 s en 15 s (89 %) | 1,4 s en 15 s (9 %) |
| | frames por segundo de la app | 49 | 59,7 (los de la pantalla) |
| Ratón real | de la entrada a la pantalla p50 / p95 | 16,0 / 19,0 ms | 28,0 / 29,9 ms |
| | CPU del servidor | 18 % | 17 % |

Presentar un frame de Vulkan de 3440x1440 (`minkar-vkcopy`):

| | Tiempo | |
| --- | --- | --- |
| Desde G4.5: la GPU copia la imagen al drawable en VRAM | 2,65 ms | 7,5 GB/s |
| Antes de G4.5: la GPU la copia a memoria del sistema | 7,0 ms | 2,8 GB/s |
| y después la CPU a la VRAM | 6,7 ms | 2,9 GB/s |

Lo que muestran:

- Las apps de Vulkan son donde cuenta el backend de Vulkan: la composición
  14 veces más barata, una décima parte de la CPU del servidor y la app al
  ritmo de la pantalla. Con pixman el servidor lee el drawable de la app en
  VRAM a través de la ventana de la CPU.
- En escenas sencillas pixman todavía es más rápido: la CPU dibuja en
  memoria del sistema mientras la memoria de la GPU va a 150 MHz. Se mide
  de nuevo con los relojes de la GPU.
- Los cambios de página cuestan un refresco de latencia (28 ms frente a los
  16 de pixman, que muestra los frames enseguida y puede cortar la imagen).
- La copia que quedó en G4.5 (de la imagen del swapchain al drawable) tarda
  aquí 2,65 ms, frente a unos 0,3 ms esperados con la memoria a su reloj
  completo.

Trabajo pendiente que señalan estas cifras, para la puerta de rendimiento
de Minkar 21.0:

- Los relojes de la GPU (si_dpm y el SMC), y después estas mediciones otra
  vez.
- Componer justo antes del retrazado vertical, en lugar de con el primer
  daño, para acercar la latencia de entrada a un refresco.
- Agrupar los movimientos del puntero dentro de un frame: unos 690 eventos
  por segundo cuestan al servidor un 17 a 18 % de un núcleo.
- Dibujar en el drawable sin la copia que quedó en G4.5, si la copia sigue
  importando con los relojes de la GPU.
