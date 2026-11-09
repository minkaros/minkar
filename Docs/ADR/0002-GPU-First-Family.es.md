# ADR 0002: Las familias del frente de GPU y su primer driver

Estado: aceptado, 2 nov 2026. El orden de las familias lo decidió el
mantenedor el 1 nov 2026; este ADR lo registra junto con el plan que se
apoya en él. Cierra la "decisión abierta" de `Docs/ROADMAP.es.md`
(Requisitos de rendimiento). Es el primer paso del frente de GPU (G1) y de
WS6 en `Docs/WindowServer-Rewrite.es.md`.

## Contexto

- Minkar 21.0 exige un rendimiento igual o mejor que el del sistema de
  referencia en la misma máquina física, incluidos 3D y composición
  (`Docs/ROADMAP.es.md`, Requisitos de rendimiento). El sistema de
  referencia solo puede medirse en el hardware de su propio fabricante; la
  máquina que tenemos para eso es el PC 2 (Intel Xeon E5-2697 v2 (12 núcleos, 24 hilos), 40 GB, NVMe Samsung, xHCI Fresco Logic FL1100, Broadcom BCM57762, dos AMD FirePro D500 (Tahiti)).
- El modelo PC 2 viene con FirePro D300 (Pitcairn) o D500/D700
  (Tahiti), dos de ellas: chips distintos, cada uno con su firmware. El
  PC 2 tiene dos FirePro D500 (Tahiti, PCI 1002:679e), la GPU que se probó
  y midió; Pitcairn nunca se ha probado. Ambas son GCN 1.0, "Southern Islands", IP gráfica GFX6, motor de pantalla
  DCE 6. Es la más antigua de las tres familias que tenemos, y la de
  drivers abiertos menos maduros.
- El PC 2 estará disponible poco tiempo.
- El PC 1 tiene las otras dos: una tarjeta AMD Radeon RX 6600 (RDNA2,
  Navi 23, GFX10.3, motor de pantalla DCN 3.0.x, PCI 1002:73ff), que lleva
  su pantalla, y la Intel HD 630 (Gen9) integrada en su i7-7700. Sirve
  para probar Minkar, no para publicar mediciones de referencia.
- Hoy Minkar dibuja con drivers IOFramebuffer sin aceleración:
  `IOGOPFramebuffer` (el framebuffer del firmware) y `MinkarVirtioGPU`
  (virtio-gpu 2D en VMs, WS5). El servidor de ventanas compone con pixman
  detrás de un compositor con la forma de `WSRenderBackend`; WS6 añade un
  backend Vulkan.
- Los drivers abiertos de estas GPU son `amdgpu` y `radeon` de Linux (e
  `i915`), llevados a FreeBSD por drm-kmod sobre su capa LinuxKPI, y los
  drivers de espacio de usuario de Mesa (RADV para Vulkan, radeonsi para
  OpenGL, ANV e iris para Intel). Casi todo el código de AMD del kernel y
  de Mesa está bajo licencia MIT; la LinuxKPI de drm-kmod es BSD-2; algunos
  ficheros del núcleo DRM de Linux son solo GPL-2.0.

## Decisión

### 1. Orden de las familias

1. **AMD GCN 1.0 (GFX6, DCE 6)**, las FirePro del PC 2: primero,
   porque la referencia se mide ahí y la máquina no estará disponible mucho
   tiempo.
2. **AMD RDNA2 (GFX10.3, DCN 3.0.x)**, la RX 6600 del PC 1: segundo. Comparte con la
   primera el driver del kernel (`amdgpu`) y RADV de Mesa, y pasa a ser la
   máquina de desarrollo de AMD cuando no esté el PC 2.
3. **Intel Gen9**, la HD 630 del PC 1: tercero, con `i915` y ANV/iris.

### 2. Cómo entran los drivers

- **Kernel:** `amdgpu` (que cubre GCN 1.0 con su soporte SI, y RDNA2) se
  porta desde drm-kmod a XNU mediante una capa tipo LinuxKPI propia sobre
  IOKit y Mach (`Kernel/Extensions/MinkarDRM`, BSD-2): PCI, memoria,
  interrupciones, colas de trabajo, colas de espera, carga de firmware. El
  driver se envuelve como un `IOFramebuffer` (pantalla) más un user client
  (memoria y envío de comandos), sin nodos de dispositivo DRM.
- **Licencias:** solo se importan ficheros bajo MIT, BSD o licencia dual
  MIT/GPL; un script comprueba la licencia de cada fichero importado antes
  del commit, y los ficheros solo GPL se reescriben o se dejan fuera. Sala
  limpia como siempre: nada de código de quien haya visto fuentes cerradas
  de la plataforma de referencia.
- **Firmware:** GCN 1.0 y RDNA2 necesitan los ficheros de microcódigo de
  AMD (de `linux-firmware`, binarios, redistribuibles bajo la licencia de
  AMD). Se cargan desde `/usr/share/firmware/amdgpu` y van en la imagen
  (decidido por el mantenedor el 2 nov 2026), sin cambios, con su licencia
  al lado y anotados entre los avisos de terceros. No son código fuente ni
  son nuestros: nada se deriva de ellos.
- **Espacio de usuario:** primero RADV de Mesa (Vulkan), después OpenGL
  (radeonsi, o Zink sobre RADV); un winsys sobre nuestro user client y un
  WSI que presenta en IOSurfaces. En VMs, lavapipe y llvmpipe mantienen
  esos mismos caminos funcionando.

### 3. Los hitos del frente

| Hito | Qué | Gate |
| --- | --- | --- |
| G1 | Este ADR; la primera parte de la capa LinuxKPI; un prototipo KMS: `MinkarAMDGPU` fija un modo por DCE 6 y muestra un búfer en VRAM como `IOFramebuffer` | El PC 2 arranca hasta la ventana de login con él, en la resolución nativa de la pantalla; `wsctl mode` cambia el modo |
| G2 | Gestor de memoria (VRAM y GTT, memoria virtual de la GPU), anillos (GFX, SDMA), fences, envío de comandos por el user client; IOSurfaces en memoria de la GPU | Un búfer de comandos se ejecuta y su fence se señala; una copia entre superficies por SDMA |
| G3 | RADV de Mesa sobre G2; WSI de Vulkan a IOSurfaces; OpenGL después | Un subconjunto de la conformidad Vulkan; un triángulo presentado en una ventana |
| G4 | `WSVulkanBackend` en el servidor de ventanas (WS6), flips al ritmo de la pantalla | El gate de rendimiento de composición de Minkar 21.0 en el PC 2 |
| G5 | Metal sobre Vulkan (darling-metal e Indium, MPL-2.0) | Una app de ejemplo de Metal dibuja |

La RX 6600 sigue G1–G4 con DCN 3.0.x y GFX10.3; Intel Gen9 después, con su
propio port del kernel (`i915`) sobre la misma capa LinuxKPI.

### 4. Mientras esté el PC 2

El tiempo de la máquina va primero a lo que no se puede hacer después: las
mediciones de referencia (fuera del repositorio), y volcados para
desarrollar sin ella (VBIOS, configuración PCI, EDID, estado de los
registros de pantalla y de relojes desde el sistema de referencia y desde
Minkar), tomados con los kits de medición y de captura. El prototipo de G1
se prueba en él en cuanto exista.

## Consecuencias

- El soporte de GCN 1.0 en `amdgpu` es el camino menos usado de ese driver
  (Linux sigue usando `radeon` por defecto ahí); qué funciona en él queda
  por verificar en G1 y G2 (pantalla por DC en DCE 6, gestión de energía,
  el nivel de Vulkan que alcanza RADV en GFX6). Si `amdgpu` no puede con
  ella, la alternativa es `radeon` para la pantalla (G1) sobre la misma
  capa LinuxKPI.
- Cuando no esté el PC 2, el trabajo en GCN 1.0 sigue solo con los
  volcados; la RX 6600 lleva el frente de AMD, y el gate de rendimiento en
  GCN 1.0 espera a que vuelva a haber una máquina con esa GPU.
- La capa LinuxKPI la comparten todas las familias, así que su primera
  parte se escribe para G1 y se reutiliza.
