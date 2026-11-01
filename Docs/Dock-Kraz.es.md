# Plan del Dock y de Kraz

Estado: **aceptado** (31 oct 2026). Fase 5 de [ROADMAP.es.md](ROADMAP.es.md).
Versión en inglés: [Dock-Kraz.md](Dock-Kraz.md).
Verificado contra `devel` en `8dab4060c`.

Este documento es el registro de la decisión y el plan de trabajo del Dock y
de Kraz, el gestor de archivos que reemplaza a Filer. Sigue las mismas reglas
que [WindowServer-Rewrite.es.md](WindowServer-Rewrite.es.md).

## 1. Decisión

- **Kraz se escribe nuevo.** No se conserva nada de Filer.
- **El Dock recibe un núcleo nuevo** y se portan tres partes del actual: las
  claves de preferencias y el formato de los datos de tiles, la ventana del
  fondo de escritorio, y el cálculo de disposición de los tiles como
  referencia.

**Regla general**, como en WindowServer: donde la plataforma de referencia
tiene un diseño conocido (qué proceso hace qué, claves de preferencias, API
públicas), el Dock y Kraz lo siguen. Un atajo solo se permite durante el
desarrollo si la sección 10 lo registra con el hito que lo elimina.

## 2. Por qué: hallazgos en el código actual

### Filer (`CoreServices/Filer/FilerMain.m`, 94 líneas)

Una ventana titulada «Filer - /Applications» sin ninguna vista. Los archivos
que se sueltan sobre ella se copian a la carpeta, borrando antes lo que había
sin preguntar; solo sabe abrir bundles `.app` (`LSOpenCFURLRef`). No hay
navegación, vista de iconos, lista o columnas, papelera ni «Abrir con». No hay
nada que conservar.

### Dock (`CoreServices/Dock/`, unas 1.300 líneas)

1. **Atado al servidor de ventanas anterior.** Activa apps con
   `_windowServerRPC(kWSApplicationActivate)` y structs de
   `WindowServer/Legacy/rpc.h`, y escucha notificaciones que reenvía ese
   servidor (`NSApplicationDidLaunch`, `NSApplicationDidQuit`,
   `WSWindowDidChangeState`). La sección 10.2 del plan de WindowServer elimina
   esos relevos.
2. **No compila.** `main.m` usa `NSAutoreleasePool new`/`drain` con
   `-fobjc-arc`, y el Makefile apunta a rutas antiguas (`gnu/lib/libobjc2`,
   `${OBJTOP}/tmp`). `CoreServices/Makefile` lo lista como no compilado.
3. **Redibujo infinito.** `-[DockView drawRect:]` termina con
   `[self setNeedsDisplay:YES]`, así que se redibuja sin parar y consume CPU.
   Además dibuja con `_frame` en lugar de `bounds`, y el código llama a
   `drawRect:` directamente.
4. **Rehace su ventana en cada cambio** (`relocate` la cierra y crea otra),
   lo que produce parpadeo.
5. **Modelo y vista mezclados.** `DockItem` es un `NSView` que guarda rutas,
   ventanas y banderas.
6. **Restos de Linux:** soporte de `AppDir`/`AppRun`.
7. **Comportamiento que falta:** arrastrar para reordenar, añadir o quitar;
   menús contextuales; magnificación; ocultación automática (la clave se lee
   y nunca se usa); la insignia, el contenido y el menú del `NSDockTile` de
   cada app (no hay IPC); miniaturas reales de las ventanas minimizadas (hoy
   un PNG genérico); pilas; una papelera que funcione (`Trash.app` registra
   «does nothing yet»).

Lo que vale la pena conservar:

- `DockTileData.m` y las claves de preferencias (`persistent-apps`,
  `tilesize`, `autohide`...), que coinciden con las del Dock de la plataforma
  de referencia.
- `DesktopWindow`, la ventana del fondo de escritorio al nivel de escritorio.
  En la plataforma de referencia el Dock dibuja el fondo y el gestor de
  archivos dibuja los iconos del escritorio.
- `fitWindowToItems` (ajustar los tiles a la pantalla en cualquier lado),
  como referencia.

## 3. Quién hace qué (organización de la plataforma de referencia)

| Trabajo | Proceso |
| --- | --- |
| Tiles del Dock, indicadores de apps en ejecución, ventanas minimizadas, pilas, tile de la papelera | Dock |
| Fondo de escritorio, una ventana por pantalla en `kCGDesktopWindowLevel` | Dock |
| Selector de apps (Comando-Tab) | Dock, que recibe Comando-Tab de la tabla de atajos simbólicos del servidor de ventanas (hoy dibuja uno el WindowServer anterior en `WSInput.m`; desaparece con `Legacy/` en WS4) |
| Ventanas del navegador de archivos, iconos del escritorio en `kCGDesktopIconWindowLevel`, contenido y vaciado de la papelera, «Abrir con», Obtener información | Kraz |
| Arrancar el Dock y Kraz al iniciar sesión, y de nuevo si terminan | loginwindow (Fase 5; launchd hasta entonces) |

Spaces, una vista general de ventanas y una cuadrícula para lanzar apps
quedan fuera de este plan; el modelo del Dock les deja sitio.

## 4. Arquitectura del Dock

- **Modelo.** `DockModel` con secciones: apps fijas, apps recientes y en
  ejecución, otros elementos fijos (carpetas y pilas), la pila de Descargas,
  la papelera. `DockTile` son datos simples (URL, bundle ID, tipo, banderas,
  insignia, ventanas). Las vistas solo muestran el modelo.
- **Vistas.** Una ventana del Dock por pantalla donde se muestre, dispuesta
  según el modelo; los tiles como capas de Core Animation (rebote,
  magnificación, deslizamiento al ocultarse), así que no se redibuja nada
  mientras nada cambia. El aspecto sigue la guía del lenguaje visual Gienah.
- **Orígenes de datos.** Las notificaciones de `NSWorkspace`
  (`NSWorkspaceDidLaunchApplicationNotification`,
  `NSWorkspaceDidTerminateApplicationNotification`,
  `NSWorkspaceDidActivateApplicationNotification`, ocultar y mostrar) y
  `NSRunningApplication`; iconos a través de LaunchServices y
  `-[NSWorkspace iconForFile:]`; activación con
  `-[NSRunningApplication activateWithOptions:]`.
- **Ventanas minimizadas.** El servidor de ventanas avisa al Dock cuando una
  ventana se minimiza o se restaura; la miniatura sale de la API de captura
  del servidor (el Dock es un cliente del sistema con ese permiso). Restaurar
  pide al servidor que muestre la ventana y activa su app.
- **Servicio de tiles del Dock.** Un servicio Mach, `org.minkar.dock.server`,
  definido en MIG, para lo que las apps envían a través de `NSDockTile`
  (texto de la insignia, captura de la vista de contenido, insignia de la
  app), `-[NSApplication requestUserAttention:]` (rebote) y
  `applicationDockMenu:` (el menú de la app en el Dock).
- **Preferencias.** Dominio `org.minkar.dock`, con las claves y significados
  de la plataforma de referencia: `persistent-apps`, `persistent-others`,
  `tilesize`, `magnification`, `largesize`, `orientation` (`bottom`, `left`,
  `right`; el entero `Location` del código actual pasa a esta clave),
  `autohide`, `autohide-delay`, `autohide-time-modifier`, `mineffect`,
  `show-recents`. Datos de tiles en el formato de la referencia
  (`tile-data`, `file-data`, `_CFURLString`).
- **Fondos de escritorio.** `DesktopWindow` portado, una por pantalla,
  identificada por el ID de pantalla (`NSScreenNumber` en
  `-[NSScreen deviceDescription]`) y no por la clave fija `"NSMainScreen"`.
  `-[NSWorkspace setDesktopImageURL:forScreen:options:error:]` guarda la
  elección y el Dock la sigue.
- **Selector de apps.** La tabla de atajos simbólicos del servidor de
  ventanas (WS4) envía Comando-Tab al Dock en lugar de entregarlo como evento
  de teclado; el Dock muestra el selector y activa la app elegida. El
  selector del servidor anterior (`WSInput.m`) desaparece con `Legacy/` en
  WS4, así que hasta D4 no hay ninguno.

## 5. Arquitectura de Kraz

- **App.** Bundle ID `org.minkar.Kraz`, instalada como
  `/System/Library/CoreServices/Kraz.app`; siempre en marcha en una sesión
  (loginwindow la arranca y la vuelve a arrancar si termina).
- **Ventanas.** Barra de herramientas, barra lateral (favoritos; volúmenes de
  DiskArbitration; ubicaciones), barra de ruta y tres vistas: iconos
  (`NSCollectionView`), lista (`NSOutlineView`) y columnas (`NSBrowser`).
  Historial de navegación, orden, ajustes de vista por carpeta.
- **Abrir.** A través de LaunchServices: app predeterminada por UTI,
  «Abrir con» (`LSCopyApplicationURLsForURL`), apps por bundle.
- **Operaciones con archivos.** Copiar, mover, duplicar, renombrar, crear
  carpeta, alias; con progreso, cancelación y deshacer; sobre
  `NSFileManager`, fuera del hilo principal.
- **Papelera.** `~/.Trash` y `/.Trashes/<uid>` en los demás volúmenes, como
  en la plataforma de referencia; «Devolver» conserva la ubicación original
  de cada elemento; vaciar la papelera.
  `-[NSWorkspace recycleURLs:completionHandler:]` pasa por el mismo código.
- **Escritorio.** Una ventana por pantalla en `kCGDesktopIconWindowLevel`
  con el contenido de `~/Desktop` y los volúmenes montados (con expulsar).
- **Actualización en vivo.** Las ventanas de carpetas siguen los cambios
  mediante los eventos del sistema de archivos (ver dependencias).
- **Mostrar en Kraz.** `open -R`,
  `-[NSWorkspace activateFileViewerSelectingURLs:]` y
  `selectFile:inFileViewerRootedAtPath:` abren una ventana de Kraz con el
  elemento seleccionado.
- **Después.** Búsqueda sobre el índice de metadatos (`mdfind`, Fase 5),
  doble clic en un DMG (Fase 6), etiquetas, vistas previas.

## 6. Dependencias

| La necesita | Dependencia | Estado hoy | Dónde |
| --- | --- | --- | --- |
| Dock | Las notificaciones de `NSWorkspace` de apps lanzadas, cerradas y activadas y el `NSRunningApplication` básico | Faltan (solo existe el antiguo `launchedApplications`) | Servicio de sesión de LaunchServices, plan de WindowServer WS4 |
| Dock | El resto de `NSRunningApplication` (icono, fecha de arranque, ocultar y mostrar, `activationPolicy`, terminar) | Falta | AppKit, D0 |
| Dock | Avisos del servidor de ventanas sobre ventanas minimizadas, y captura de ventanas | Faltan | Plan de WindowServer, WS4 |
| Dock | Relevos eliminados, `kWSApplicationActivate` retirado | Planeado | Plan de WindowServer, sección 10.2 (WS4) |
| Dock | `NSDockTile` enviando su estado | Guarda valores, no envía nada | AppKit, D3 |
| Dock | `setDesktopImageURL:forScreen:options:error:` | Por comprobar | AppKit, D1 |
| Kraz | Base de datos de bundles y UTI de LaunchServices, apps predeterminadas, «Abrir con» | Fase 3, punto 4 | Frameworks |
| Kraz | Eventos del sistema de archivos: existe el `/dev/fsevents` de XNU (`vfs_fsevents.c`); faltan un daemon y la API FSEvents | Faltan | CoreServices, antes de K1 |
| Kraz | `NSCollectionView`, `NSOutlineView`, `NSBrowser` funcionando bien | Presentes, en parte con stubs | AppKit, con K1 |
| Kraz | Iconos, papelera y «mostrar» de `NSWorkspace` | Parciales | AppKit, K1–K2 |
| Ambos | loginwindow arrancándolos al iniciar sesión | Fase 5 | loginwindow |
| Ambos | La guía del lenguaje visual Gienah | Falta | Diseño, antes de D2 y K1 |

## 7. Hitos

Cada hito termina con pruebas que pasan, `boottest GATE=M2` (y, cuando
exista, el gate M4) todavía en verde y el roadmap actualizado con la
evidencia.

### Dock

- **D0 — Requisitos previos.** El resto de `NSRunningApplication` (icono,
  fecha de arranque, ocultar y mostrar, `activationPolicy`, terminar) sobre la
  lista que guarda el servicio de sesión de LaunchServices desde WS4, y la API
  del fondo de escritorio. Las notificaciones de apps lanzadas, cerradas y
  activadas y el `NSRunningApplication` básico llegan con WS4. Pruebas: se
  leen el icono y la fecha de arranque de una app de prueba; ocultarla,
  mostrarla y terminarla con `NSRunningApplication` funciona; la elección del
  fondo de escritorio se guarda.
- **D1 — Núcleo nuevo.** `DockModel`, preferencias con las claves de la
  referencia, tiles de apps fijas y en ejecución, lanzar y activar,
  indicadores de ejecución, fondos de escritorio por pantalla. Se va el
  código anterior: `Trash.app`, `Downloads.app`, `AppDir`, las
  notificaciones reenviadas. Pruebas: pruebas del modelo; lanzar una app
  muestra su tile y su indicador, cerrarla lo quita; las preferencias se
  guardan y se leen igual; `screendump` muestra el Dock y el fondo; el Dock
  no consume CPU en reposo.
- **D2 — Interacción.** Arrastrar para reordenar, añadir y quitar; menús
  contextuales (Mantener en el Dock, Abrir al iniciar sesión, Mostrar en
  Kraz, Salir) y el menú de la propia app; ocultación automática,
  magnificación, orientación.
- **D3 — Servicio de tiles y ventanas minimizadas.**
  `org.minkar.dock.server`: insignias, contenido, rebote de atención;
  ventanas minimizadas con miniaturas (después de WS4).
- **D4 — Pilas, papelera y selector de apps.** Pilas de Descargas y de
  carpetas; tile de la papelera (vacía o llena, soltar para tirar, vaciar a
  través de Kraz); el selector de apps en el Dock, que recibe Comando-Tab de
  la tabla de atajos simbólicos del servidor de ventanas.

### Kraz

- **K0 — Requisitos previos.** Daemon y API de eventos del sistema de
  archivos; abrir y «Abrir con» de LaunchServices; las vistas de AppKit
  comprobadas con apps de prueba.
- **K1 — Navegación.** Ventanas con barra lateral, vistas de iconos, lista y
  columnas, navegación, abrir archivos y apps, Obtener información básico.
  Filer pasa a ser Kraz: bundle ID, `LSRevealInFiler` como alias obsoleto del
  nombre nuevo durante una versión, `open -R`, los tiles predeterminados del
  Dock. Es la parte «Kraz navega» del gate M5.
- **K2 — Operaciones con archivos.** Copiar, mover, renombrar y duplicar con
  progreso y deshacer; arrastrar y soltar entre ventanas y sobre el Dock;
  papelera con «Devolver»; «Abrir con».
- **K3 — Escritorio y sesión.** Iconos del escritorio y volúmenes con
  expulsar; API para mostrar en Kraz; arranque y rearranque por loginwindow.
- **K4 — Trabajo posterior.** Búsqueda sobre el índice de metadatos, doble
  clic en un DMG (Fase 6), etiquetas, vistas previas.

El gate M5 («desde el arranque se llega a una sesión gráfica, se abre una app
desde el Dock, se navega con Kraz y se cambia la resolución desde
Preferencias») necesita D1 y K1, además de loginwindow y Preferencias del
Sistema.

## 8. Pruebas y mediciones

- Pruebas del modelo (secciones y orden de tiles del Dock, ida y vuelta de
  preferencias; operaciones con archivos y registros de la papelera de Kraz)
  como programas de línea de comandos en la suite de pruebas.
- `boottest` con QMP: comparaciones de `screendump` y clics inyectados (abrir
  una app desde el Dock; navegar una carpeta en Kraz).
- CPU en reposo del Dock y de Kraz cercana a cero; tiempo desde un clic en el
  Dock hasta la primera ventana de la app, medido igual en la VM y en los
  equipos físicos.

## 9. Reglas de este trabajo

Las mismas que la sección 8 del plan de WindowServer: inglés en todo lo que
se sube salvo las traducciones `.es.md`; sin nombres comerciales de terceros;
nombres de API y de símbolos como son; código nuevo MIT o BSD-2-Clause; sala
limpia; commits pequeños mostrando antes el diff; evidencia para cada punto
del roadmap; sin líneas `Co-Authored-By`.

## 10. Decisiones y atajos de desarrollo

| Pregunta | Decisión (objetivo) | Atajo de desarrollo | Se quita en |
| --- | --- | --- | --- |
| Preferencias del Dock | Dominio `org.minkar.dock`, claves y formato de datos de tiles de la referencia | — | — |
| Quién dibuja el escritorio | Dock: fondos; Kraz: iconos | Escritorio sin iconos hasta K3 | K3 |
| Selector de apps | Dock, con Comando-Tab de la tabla de atajos del servidor de ventanas | Servidor de ventanas anterior (`WSInput.m`) hasta WS4; después ninguno: se cambia de app con clic en sus ventanas (`SHORTCUT(D4)`) | D4 |
| Ventanas minimizadas | Miniaturas desde la captura del servidor de ventanas | Icono de ventana genérico | D3 (después de WS4) |
| Arranque del Dock y de Kraz | loginwindow, que los rearranca si terminan | Agentes de launchd | Fase 5 (loginwindow) |
| Aspecto | Guía del lenguaje visual Gienah | Controles y colores del sistema, sin estilo propio | Cuando exista la guía |

Cada atajo queda marcado en el código con `// SHORTCUT(Dn):` o
`// SHORTCUT(Kn):` y el hito que lo elimina.
