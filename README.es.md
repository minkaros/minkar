# Minkar

Minkar es un sistema operativo libre para PC x86-64 que ejecuta los binarios publicados
de las apps hechas para la plataforma de referencia, sin modificarlos ni recompilarlos. Se
apoya en el kernel y el runtime de Darwin (XNU, objc4, CoreFoundation), en su propio
Foundation y su AppKit basado en Cocotron, y en el userland de FreeBSD, con código nuevo
para los huecos. Su escritorio es Atlas, con su propio lenguaje visual.

Última revisión: 21 nov 2026. Es una copia de [README.md](README.md) y se actualiza en el
mismo commit.

## Estado

<!-- progress:readme:begin -->
**Funciona hoy**

- Arranca en PC x86-64 genéricos (PC 1, PC 2) y en QEMU, desde SATA, NVMe o virtio, con todas las CPU
- Una base Unix: launchd, red, SSH y 354 de las 367 órdenes del sistema de referencia
- Objective-C, Swift 6.4, Foundation y Combine; siete herramientas libres pasan sus pruebas
- Un servidor de ventanas que compone en la GPU y el escritorio Atlas: barra de menús, Dock, Files, Preferencias, notificaciones
- Terminal, Monitor del sistema y Registros; imágenes de disco, también las comprimidas
- WebKit sobre WPE, audio y vídeo (CoreAudio, AVFoundation, los medios de WebKit)
- Firefox, Thunderbird, Hex Fiend, Skim, TextMate, Platypus y Sloth desde sus binarios publicados

**En curso**

- Atlas para el uso diario (gate M5): la ventana de inicio de sesión con Directory Services, la pantalla bloqueada, el lenguaje visual
- El gate BC en la CI y el conjunto de apps de la API 19 (carril BC)
- Minkar 19.6.0 (gate M6): el instalador del sistema, la compresión de HFS+, el Editor de texto, W5 de WebKit
- Drivers de GPU: los modos de pantalla de la RDNA2 del PC 1 (las pruebas del PC 2, en 19.6.x)

**Siguiente**

- Audio AU7–AU10: audio USB, MIDI, Audio Units, dispositivos agregados
- Después de 19.6.0: las apps A5–A10, las pruebas en el PC 2, Darwin 25, arm64 en QEMU, SwiftUI

_Actualizado el 2026-11-26, devel `228f82b367`_
<!-- progress:readme:end -->

Todos los hitos: [Docs/Progress.md](Docs/Progress.md). Qué apps funcionan y hasta dónde:
[Docs/Compatibility.es.md](Docs/Compatibility.es.md).

## Características

- **Compatibilidad binaria**, el objetivo principal: arrancan y funcionan los binarios
  publicados de un conjunto de apps designadas, un nivel de API por detrás del actual de la
  plataforma de referencia.
- **Compatibilidad de fuentes:** una app Cocoa se puede compilar en Minkar y ejecutar.
- **El escritorio Atlas:** una barra de menús que refleja la app activa, un Dock, Files,
  Preferencias del sistema y apps de uso diario, sobre las API Cocoa que esperan las apps.
- **La organización de la plataforma de referencia:** `/Library`, `/System`, `/Users`,
  `/Volumes`, apps en paquetes, instaladas desde imágenes de disco.
- **Sistemas de archivos:** HFS+ (la raíz), FAT y exFAT (lectura y escritura), NFS; APFS
  llega después de Minkar 19.6.0.
- **Aceleración por GPU:** un compositor pensado para la GPU, drivers de AMD y el Vulkan de
  Mesa, con un rendimiento igual o mejor que el del sistema de referencia en la misma
  máquina como gate de una versión.
- **Seguridad:** firmas de código y entitlements comprobados por el kernel, y un sandbox
  para las apps.
- **Un entorno Unix actual:** los shells y las herramientas de hoy, de FreeBSD y de sus
  propios proyectos.

## Probarlo

Todavía no hay una imagen para descargar. Compila una y arráncala en QEMU con el
[inicio rápido](Docs/BUILDING.es.md#inicio-rápido).

## Por dónde empezar

- [Docs/README.md](Docs/README.md): la documentación, por lector.
- [Docs/CONTRIBUTING.es.md](Docs/CONTRIBUTING.es.md): cómo contribuir, y las primeras tareas.
- [Docs/Compatibility.es.md](Docs/Compatibility.es.md): qué apps funcionan y qué necesitan.

## Créditos

Minkar es un fork de [ravynOS](https://github.com/ravynsoft/ravynos). Sus colaboradores
hasta el fork están en [CREDITS-ravynOS.txt](CREDITS-ravynOS.txt), y la licencia propia de
ravynOS en [LICENSE-ravynOS](LICENSE-ravynOS). Los colaboradores de Minkar están en
[AUTHORS](AUTHORS).

## Marcas

Ver [TRADEMARKS.md](TRADEMARKS.md).

## Licencia

El código escrito para Minkar va bajo la licencia BSD de 2 cláusulas
([LICENSE-minkar](LICENSE-minkar)). Cada otro archivo mantiene la licencia con la que
llegó; ver [NOTICE](NOTICE) para los componentes de terceros y sus licencias.
