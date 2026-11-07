# Plan de Atlas

Estado: **aceptado** (5 nov 2026). Fases 5 y 6 de [ROADMAP.es.md](ROADMAP.es.md).
Versión en inglés: [Atlas.md](Atlas.md).

Atlas es el escritorio de Minkar: la sesión (login, barra de menús, Dock,
Files, Preferencias del Sistema) y las aplicaciones que una persona
necesita a diario. Este documento es el único registro de decisiones y
plan de trabajo de todo ello. Recoge el antiguo plan de los menús
(capítulo 3, hitos MB0–MB4) y el del Dock y Files (capítulo 4, D0–D4 y
K0–K4), cuyos hitos conservan sus nombres, y añade las aplicaciones
(capítulo 5, A0–A10). Sigue las mismas reglas que
[WindowServer-Rewrite.es.md](WindowServer-Rewrite.es.md).

## 1. Alcance y reglas

- **Todo es una app o un bundle de Cocoa.** La sesión y las apps se
  escriben sobre la API pública de Cocoa, así que sirven también de prueba
  continua de AppKit y Foundation; lo que una necesite y les falte a los
  frameworks se añade a los frameworks, nunca se rodea en la app.
- **Lenguaje visual propio.** Sin fuentes, sonidos ni arte de terceros,
  con una excepción para los iconos (decidida el 2026-11-05): los temas de
  iconos Faience y Faenza de Matthieu James (GPL-3), como ficheros de datos
  aparte con su licencia, autores y fuentes SVG en `SystemLibrary/Icons`
  (instalados en `/System/Library/Icons`), y sin iconos de marcas.
  Conservan su licencia GPL-3; ningún código se enlaza con ellos y el resto
  de la regla sigue igual (aceptado por el mantenedor: no existe un juego
  de iconos del mismo alcance con licencia BSD o MIT). Las
  carpetas son azules por defecto (Faience-Azur), con arena y naranja como
  alternativas. Los iconos de las apps son PNG de 512×512 en el bundle
  (AppKit aún no lee `.icns`). La barra de menús es oscura (gris carbón,
  texto claro). La fuente de la interfaz es Inter y la de ancho fijo Agave
  (ambas SIL OFL 1.1).
- **Nombres genéricos.** Los únicos nombres propios son Minkar, el
  sistema, y Atlas, su escritorio (decidido el 5 nov 2026). Cada app se
  llama por lo que hace, con las palabras comunes que usan otros
  escritorios libres (Files, Text Editor, System Monitor, Logs), nunca con
  los nombres propios de otros proyectos ni con el nombre de una app de la
  plataforma de referencia que no sea una palabra común. El bundle se
  nombra sin espacios (`TextEditor.app`), su identificador es
  `org.minkar.<Nombre>`, y el nombre en español llega con la traducción de
  la app (Archivos, Editor de texto). Files y Text Editor se llamaban Argos
  y Alcor hasta entonces.
- **Sobre los comandos.** Donde Minkar tiene el comando (`top`, `log`,
  `screencapture`, `diskutil`, `hdiutil`, `system_profiler`, `tar`), la
  app es su interfaz y comparte su código o su salida, de modo que ambos se
  comportan igual.
- **Inglés y español.** Cada app está traducida a los dos; el inglés es el
  idioma de desarrollo, pruebas y capturas.
- **Lugares.** Las piezas de la sesión en `/System/Library/CoreServices`,
  las apps en `/System/Applications`, las utilidades en
  `/System/Applications/Utilities`.

## 2. Estado

| Parte | Capítulo | Hitos | Hechos | Siguiente |
| --- | --- | --- | --- | --- |
| Menús | 3 | MB0–MB4 | MB0 | MB1 (parcial), MB2 |
| Dock | 4 | D0–D4 | D0, D1 | D2 |
| Files | 4 | K0–K4 | K1 (K0 parcial) | K0, K2 |
| Aplicaciones | 5 | A0–A10 | — (Text Editor empezado) | A0, A1 |

Las demás piezas de la sesión (LoginWindow, Preferencias del Sistema, el
lenguaje visual) son puntos de la Fase 5 en [ROADMAP.es.md](ROADMAP.es.md).

## 3. Menús

### 3.1 Decisión

- **Los menús vuelven a cada app.** AppKit dibuja la barra de menús de la app
  y sus menús, y los sigue (tracking) dentro del propio proceso de la app,
  como en la plataforma de referencia. El servidor de ventanas solo reserva el
  sitio de la barra de menús en cada pantalla e indica qué app está al frente.
- **El modelo de menús se queda.** Se conservan `NSMenu` y `NSMenuItem`; el
  código de dibujo y seguimiento de SystemUIServer (`NSMainMenuView`,
  `SubmenuView`, `NSMenuWindow`) pasa a AppKit y se rehace allí.
- **Los status items se reescriben** como ventanas de su propia app.
- **SystemUIServer solo aloja los Menu Extras del sistema.**
- **El primer hito, MB0, es parte de WS4b:** es el trabajo de menús que la
  sección 10.2 asigna al cambio de servidor, y WS4b no se cierra hasta que
  MB0 pase. Lo que MB0 necesita del servidor de ventanas llega en WS4a. El
  resto
  (MB1 a MB4) es de la Fase 5.

**Regla general**, como en WindowServer: donde la plataforma de referencia
tiene un diseño conocido (qué proceso hace qué, API públicas, menús
estándar), los menús lo siguen. Un atajo solo se permite durante el
desarrollo si la sección 3.10 lo registra con el hito que lo elimina.

### 3.2 Por qué: hallazgos en el código actual

1. **Hoy no hay barra de menús.** SystemUIServer, que la dibuja, no se
   compila (`CoreServices/Makefile`: «FIXME: Dock, Filer, DMIHelper,
   SystemUIServer and loginwindow ... are not built yet»).
2. **Menús dibujados en otro proceso.** `-[NSApplication
   sendMenusToWindowServer]` copia todo el menú principal, le quita targets y
   delegados (`_menuEnumerateAndChange:`), lo archiva con `NSKeyedArchiver` y
   lo envía (`CODE_MENU_FOR_APP`) a través del servidor anterior
   (`WindowServer/Legacy/WindowServer.m`) a SystemUIServer, que lo desarchiva
   y lo dibuja (`AppDelegate.m`, `NSMainMenuView.m`, `SubmenuView.m`). Un clic
   vuelve como un tag (`CODE_ITEM_CLICKED`) y la app busca el elemento por
   ese tag. El servidor anterior desaparece en WS4b, y este camino con él.
3. **Se sobrescriben los tags de la app.** Antes de enviar, `_tagAllMenus`
   pone en cada elemento un ID interno como tag, así que fallan las apps que
   usan `tag`, `itemWithTag:` o `indexOfItemWithTag:`.
4. **La validación y los menús dinámicos no pueden funcionar.**
   SystemUIServer tiene una copia sin targets ni delegados, así que nada puede
   preguntar a la app en el momento en que se abre un menú:
   `validateMenuItem:`, `validateUserInterfaceItem:`, `menuNeedsUpdate:`,
   `menuWillOpen:`, alternativos con Opción, títulos que cambian. Cada cambio
   envía una copia completa nueva (`sendMenusToWindowServer` se llama desde
   seis sitios de `NSApplication.m`).
5. **Límite de tamaño.** El archivo se corta al mensaje de 128 KB
   (`MIN([d length], sizeof(msg.data))`, `data[128*1024]` en `message.h`),
   así que un menú grande llega como un archivo roto.
6. **Los menús contextuales no están implementados.** `+[NSMenu
   popUpContextMenu:withEvent:forView:]` está en `#if 0` con
   `NSUnimplementedMethod()`.
7. **Los status items son código de Windows.** `NSStatusBar.m` y
   `NSStatusItem.m` importan `<windows.h>` y `Win32Window` (iconos de la
   bandeja de otra plataforma); `-[NSApplication _addStatusItem:]` archiva
   el elemento y lo envía a SystemUIServer (`CODE_ADD_STATUS_ITEM`).
8. **Restos.** IDs de menú de D-Bus en `NSMenu` y `NSMenuItem`
   (`DBusItemID`).
9. **El menú del sistema** vive en SystemUIServer (`MenuView.m`): Reposo y
   Forzar salida solo registran «not implemented yet»; Preferencias del
   Sistema registra el mismo mensaje, pero después abre
   `System Preferences.app` con `LSOpenCFURLRef`; Reiniciar, Apagar y Cerrar
   sesión acaban en los códigos de salida que elimina la sección 10.3 del
   plan de WindowServer.

Lo que vale la pena conservar:

- `NSMenu` y `NSMenuItem` como modelo: elementos, submenús, activación
  automática, `update`, `performKeyEquivalent:` con validación, delegado.
- `NSMainMenuView`, `SubmenuView` y `NSMenuWindow`: dibujo, desbordamiento,
  colocación de submenús y `trackForEvent:`, como punto de partida de las
  vistas de menú de AppKit.
- `ClockView` y `ExtrasView` de SystemUIServer, como primeros Menu Extras.

### 3.3 Quién hace qué (organización de la plataforma de referencia)

| Trabajo | Quién |
| --- | --- |
| El sitio de la barra de menús en cada pantalla, su altura, la app del frente, la disposición de la zona de estado | Servidor de ventanas |
| La barra de menús de la app del frente: menú del sistema, menú de la app, menús de la app, dibujados en una ventana de nivel `kCGMainMenuWindowLevel` | La app (AppKit) |
| Abrir y seguir menús, submenús y menús contextuales, en ventanas de nivel `kCGPopUpMenuWindowLevel` | La app (AppKit), en su propio proceso |
| Status items (`NSStatusItem`), cada uno una ventana de nivel `kCGStatusWindowLevel` | La app que los posee |
| Menu Extras del sistema (reloj, volumen, red, pantallas...), como bundles cargables | SystemUIServer |
| Desde la API 20: el grupo de estado del sistema (red, sonido, batería, pantallas...) y sus popovers | Una app de centro de control aparte; SystemUIServer conserva la búsqueda, las notificaciones y los Menu Extras antiguos |
| Lo que hacen los elementos del menú del sistema (reiniciar, apagar, cerrar sesión, reposo, forzar salida) | loginwindow |
| El contenido del menú Servicios | El daemon de servicios (servidor de pasteboard) |

Cada parte de la barra es su propia ventana sin borde, y el servidor de
ventanas las compone en una sola franja. Cuando cambia la app del frente, el
servidor de ventanas retira la ventana de barra de menús de la app anterior y
muestra la de la nueva; las ventanas de los status items no se mueven. Files
siempre está en ejecución, así que es la app del frente cuando no hay otra.

Como los menús corren en el proceso de la app, una app colgada no puede abrir
sus menús, igual que en la plataforma de referencia; el servidor de ventanas
muestra el cursor de ocupado sobre ella y Forzar salida (loginwindow) sigue
disponible.

### 3.4 Comportamiento que hay que seguir

#### Barra de menús

- Una barra de menús por pantalla, cada una con los menús de la app del
  frente; el menú de la app en negrita con el nombre de la app.
- Primero el menú del sistema, con el logo de Minkar (sin imágenes de
  terceros): Acerca de este equipo, Preferencias del Sistema…, Ítems
  recientes, Forzar salida…, Reposo, Reiniciar…, Apagar…, Bloquear pantalla,
  Cerrar la sesión de *usuario*…. Su contenido lo da el sistema y es el mismo
  en todas las apps.
- Elementos estándar del menú de la app: Acerca de *App*, Preferencias…,
  Servicios, Ocultar *App*, Ocultar otros, Mostrar todo, Salir de *App*; la
  app puede añadir los suyos.
- Las apps que no usan AppKit, o que no definen un menú principal, reciben
  igualmente el menú del sistema y un menú de la app con Salir.
- Los menús de la app y, a la derecha, la zona de estado; si falta espacio,
  los menús de la app se recortan por el final, sin tapar nunca la zona de
  estado.
- Oculta en pantalla completa, y visible al llevar el puntero arriba;
  ocultación automática opcional.
- `-[NSScreen visibleFrame]` excluye la barra de menús en cada pantalla que
  la muestra.

#### Seguimiento

- Presionar, arrastrar y soltar elige un elemento; un clic abre un menú que
  se queda abierto hasta otro clic.
- Teclado: flechas, Retorno, Escape, búsqueda por escritura, Control-F2 lleva
  el foco a la barra de menús.
- Los submenús se abren tras un breve retardo y siguen abiertos mientras el
  puntero se mueve hacia ellos.
- Los menús largos se desplazan, con flechas en los bordes.
- Mantener Opción muestra los elementos alternativos (`isAlternate`).
- `NSMenuDelegate`: `menuNeedsUpdate:`, `numberOfItemsInMenu:`,
  `menu:updateItem:atIndex:shouldCancel:`, `menuWillOpen:`,
  `menuDidClose:`, `menu:willHighlightItem:`.
- La validación se hace al abrir un menú y antes de ejecutar un atajo: el
  target del elemento, o la cadena de respondedores si el target es nil, a
  través de `validateMenuItem:` o `validateUserInterfaceItem:`.

#### Atajos de teclado

- En el orden documentado: la ventana key y sus vistas
  (`performKeyEquivalent:`), después el menú principal, validando antes cada
  elemento.
- El menú muestra los atajos con los símbolos de los modificadores.
- Los atajos reservados por el sistema los reconoce el servidor de ventanas y
  los envía a su dueño, sin entregarlos como eventos de teclado: Comando-Tab
  al Dock, Comando-Opción-Escape a loginwindow, Control-F2 al AppKit de la app
  del frente, que lleva el foco a la barra de menús.

#### Menús estándar

- **Menú Ventana** (`windowsMenu`): todas las ventanas listadas
  automáticamente, Minimizar, Zoom, Traer todo al frente.
- **Abrir recientes**: `NSDocumentController` con las listas compartidas de
  archivos de LaunchServices (sección 10.2 del plan de WindowServer).
- **Menú Ayuda** (`helpMenu`): un campo de búsqueda que encuentra elementos
  de menú (y libros de ayuda, más adelante).
- **Menú Servicios** (`servicesMenu`): construido por el daemon de servicios
  a partir de las entradas `NSServices` de los `Info.plist` de las apps, y
  enviado a través del pasteboard.
- **Menú Visualización**: «Entrar en pantalla completa» añadido para las
  ventanas que lo permiten.

#### Otros menús

- Menús contextuales: `menuForEvent:`, Control-clic,
  `+popUpContextMenu:withEvent:forView:`,
  `-popUpMenuPositioningItem:atLocation:inView:`.
- Botones emergentes (`NSPopUpButton`) con las mismas ventanas de menú y el
  mismo seguimiento.
- Menús del Dock (`applicationDockMenu:`) a través del servicio de tiles del
  capítulo 4 (D2).

#### Status items

- `-[NSStatusBar statusItemWithLength:]` con `NSVariableStatusItemLength` y
  `NSSquareStatusItemLength`; `button` (`NSStatusBarButton`), `menu`,
  `length`, `visible`, `autosaveName`, `behavior`.
- Cada elemento es una ventana de su app de nivel `kCGStatusWindowLevel`; el
  servidor de ventanas los coloca de derecha a izquierda, junto con los Menu
  Extras del sistema; Comando-arrastrar los reordena y el orden se conserva.

#### Accesibilidad

La barra de menús, los menús y sus elementos exponen sus roles de
accesibilidad, para que las herramientas de asistencia y la API de
accesibilidad puedan leerlos y pulsarlos.

### 3.5 Arquitectura

- **AppKit:**
  - `NSMenuBarWindow`: una por pantalla, de nivel `kCGMainMenuWindowLevel`,
    visible mientras la app está al frente, con un `NSMenuBarView` (a partir
    de `NSMainMenuView`).
  - `NSMenuWindow` y `NSMenuView` (a partir de `SubmenuView`) para menús y
    submenús de nivel `kCGPopUpMenuWindowLevel`.
  - El bucle de seguimiento en la app, en `NSEventTrackingRunLoopMode`, con
    el puntero capturado por el servidor de ventanas mientras haya un menú
    abierto.
  - El menú del sistema construido a partir de una definición compartida en
    un framework del sistema; sus acciones son peticiones a loginwindow
    (sección 10.3 del plan de WindowServer) y a la app Preferencias del
    Sistema.
  - `NSStatusBar` y `NSStatusItem` reescritos para esta plataforma.
  - Se eliminan: `-[NSApplication sendMenusToWindowServer]`, `_tagAllMenus`,
    `_menuEnumerateAndChange:`, `_addStatusItem:`, `addRecentItem:` a través
    del servidor de ventanas, el manejo de `CODE_MENU_FOR_APP`,
    `CODE_ITEM_CLICKED`, `CODE_ADD_STATUS_ITEM`, `CODE_STATUS_ITEM_ADDED` y
    `CODE_ADD_RECENT_ITEM`, y `DBusItemID`.
- **Servidor de ventanas (SkyLight):** reserva el sitio de la barra de menús
  en cada pantalla y publica su altura; avisa a cada app cuando pasa a estar o
  deja de estar al frente; dispone la zona de estado (ventanas de status
  items y Menu Extras) y conserva el orden; gestiona los atajos reservados;
  captura el puntero mientras hay un menú abierto. Es la parte que la sección
  10.2 deja en el servidor de ventanas.
- **SystemUIServer:** anfitrión de los bundles de Menu Extras (`.menu`) en la
  zona de estado; se elimina el código de dibujo de menús y el manejo de los
  `CODE_*`. Se compila con las reglas de compilación actuales.
- **Daemon de servicios:** construye la lista de servicios a partir de
  `NSServices` y ejecuta un servicio a través del pasteboard
  (`NSPerformService`, `NSUpdateDynamicServices`).

### 3.6 Dependencias

| La necesita | Dependencia | Dónde |
| --- | --- | --- |
| MB0 | App del frente, orden de activación, sitio de la barra de menús, disposición de la zona de estado, captura del puntero | Plan de WindowServer, WS4a (sección 10.2) |
| MB0 | `loginwindow` mínimo que atiende peticiones de reiniciar, apagar y cerrar sesión | Plan de WindowServer, sección 10.3 (WS4b) |
| MB0 | Tabla de atajos simbólicos con entrega a sus dueños (Control-F2 a la app del frente) | Plan de WindowServer, WS4a |
| MB1 | SystemUIServer con las reglas de compilación actuales | FIXME de `CoreServices/Makefile` |
| MB2 | Listas compartidas de archivos de LaunchServices, `NSWorkspace` | Fase 3 (LaunchServices); plan de WindowServer 10.2 |
| MB3 | El LoginWindow completo (reposo, forzar salida, bloquear pantalla) | Fase 5, punto 1 |
| MB3 | App Preferencias del Sistema | Fase 5, punto 6 |
| MB4 | Daemon de servicios | Fase 5 |
| Todos | El lenguaje visual Atlas (tipografías, medidas y colores de los menús) | Fase 5, punto 7 |
| Menús del Dock | Servicio de tiles del Dock | Capítulo 4, D2 |

### 3.7 Hitos

Cada hito termina con pruebas que pasan, los gates existentes todavía en verde
y el roadmap actualizado con la evidencia. Los hitos se llaman MB*n* para
distinguirlos de los gates M*n*.

- [x] **MB0 — Menús en la app (parte de WS4b; hecho el 2 nov 2026, gate M4, `9d510ec6c2`).** Las vistas de menú pasan a
  AppKit; una ventana de barra de menús por pantalla para la app del frente;
  seguimiento con clic, arrastre y teclado; submenús; validación y delegado
  al abrir; atajos en el orden documentado; menús contextuales y botones
  emergentes; los tags de la app se respetan. El menú del sistema en AppKit,
  con Acerca de este equipo, y Reiniciar, Apagar y Cerrar sesión como
  peticiones al `loginwindow` mínimo. Los status items como ventanas de su
  app con la API básica (`statusItemWithLength:`, `button`, `menu`,
  `length`). El camino anterior se elimina de AppKit y de SystemUIServer,
  para que WS4b pueda quitar los códigos de relevo. Pruebas, añadidas al gate
  M4: un clic por QMP abre el menú Archivo y la acción de un elemento se
  ejecuta en la app; un elemento desactivado (por validación) no hace nada;
  un atajo con Comando ejecuta su elemento; un menú contextual se abre con
  Control-clic; una app que usa tags los conserva; al cambiar de app cambia
  la barra de menús; el status item de una app de prueba aparece y abre su
  menú; Cerrar sesión termina la sesión; `screendump`.
- [ ] **MB1 — Menu Extras y la API completa de status items.** SystemUIServer
  compilado y alojando bundles `.menu`: reloj, volumen, pantallas, red,
  notificaciones (Fase 5, punto 3); `visible`, `autosaveName`, `behavior`;
  Comando-arrastrar los reordena y el orden se conserva. Pruebas: dos apps y
  el reloj aparecen en orden y abren sus menús; el orden se mantiene tras
  reiniciar. El gate M5 necesita MB1. *Parcial (4 nov 2026):* un
  SystemUIServer nuevo con el reloj integrado, que sigue al momento el
  formato de 24 horas y la zona horaria (`0fa453aa16`, `61f9c7ae99`); faltan
  los bundles `.menu` y el resto de la API de status items.
- [ ] **MB2 — Menús estándar y detalles.** Menú Ventana, Abrir recientes,
  búsqueda del menú Ayuda sobre los elementos de menú, alternativos con
  Opción, desplazamiento de menús largos, búsqueda por escritura,
  Control-F2, el elemento de pantalla completa del menú Visualización, roles
  de accesibilidad.
- [ ] **MB3 — El menú del sistema completo.** Con el LoginWindow y Preferencias
  del Sistema de la Fase 5: Reposo, ventana de Forzar salida, Bloquear
  pantalla, Preferencias del Sistema…, Ítems recientes. Pruebas: Reiniciar
  pide a las apps que terminen antes de reiniciar la VM; Forzar salida
  termina una app de prueba colgada.
- [ ] **MB4 — Servicios y comportamiento de la barra.** Daemon y menú de
  servicios; ocultación en pantalla completa; ocultación automática.

El gate M6 (Minkar 19.6.0, un escritorio para el uso diario) necesita MB2 y
MB3. MB4 llega en 19.6.x.

### 3.8 Pruebas y mediciones

- Pruebas del modelo (orden de validación, orden de los atajos,
  conservación de tags) como programas de línea de comandos en la suite de
  pruebas.
- `boottest` con QMP: abrir menús, elegir elementos, atajos, menús
  contextuales, status items; comparaciones de `screendump`.
- Tiempo desde el clic en el título de un menú hasta que el menú aparece en
  pantalla, medido igual en la VM y en los equipos físicos.

### 3.9 Reglas de este trabajo

Las mismas que la sección 8 del plan de WindowServer: inglés en todo lo que se
sube salvo las traducciones `.es.md`; sin nombres comerciales de terceros;
nombres de API y de símbolos como son; código nuevo MIT o BSD-2-Clause; sala
limpia; commits pequeños mostrando antes el diff; evidencia para cada punto
del roadmap.

### 3.10 Decisiones y atajos de desarrollo

| Pregunta | Decisión (objetivo) | Atajo de desarrollo | Se quita en |
| --- | --- | --- | --- |
| Quién dibuja y sigue los menús de la app | La app (AppKit), en su propio proceso | Ninguno: SystemUIServer dibujaba una copia, y no se compila | MB0 |
| Menú del sistema | Logo de Minkar, los elementos de la plataforma de referencia, acciones a través de loginwindow | Reiniciar, Apagar y Cerrar sesión a través del `loginwindow` mínimo; Reposo, Forzar salida, Bloquear pantalla y Preferencias del Sistema desactivados | MB3 |
| Confirmar Reiniciar, Apagar y Cerrar sesión | El panel de `loginwindow`, con su cuenta atrás | La app que lo pide muestra una alerta; con Opción pulsada no se muestra (`SHORTCUT(Phase 5)` en `NSMenuBarWindow.m`) | Fase 5 (LoginWindow) |
| Acerca de este equipo | La ventana propia de Atlas | Una alerta sencilla con el nombre y la versión del sistema, el kernel, el procesador, la memoria y la pantalla (`SHORTCUT(Phase 5)` en `NSMenuBarWindow.m`) | Fase 5 |
| Status items | Ventanas de sus apps, dispuestas por el servidor de ventanas | Solo la API básica | MB1 |
| Menu Extras | Bundles en SystemUIServer | Ninguno hasta que SystemUIServer se compile | MB1 |
| Documentos recientes | Listas compartidas de archivos de LaunchServices | Guardados por app por `NSDocumentController` en sus preferencias | MB2 |
| Menú Servicios | Daemon de servicios y pasteboard | Menú Servicios vacío | MB4 |
| Grupo de estado del sistema | API 19: Menu Extras en SystemUIServer; API 20: una app de centro de control aparte | Ninguno | Minkar 20.0 |
| Aspecto | Lenguaje visual Atlas | El aspecto que tenía SystemUIServer: elementos de 22 puntos, tipografía y colores del sistema (`SHORTCUT(Phase 5)` en `NSMenuView.h`) | Fase 5, punto 7 |

Cada atajo queda marcado en el código con `// SHORTCUT(MBn):` y el hito que lo
elimina.

### 3.11 Referencias

Solo descripciones públicas del comportamiento observado; nada de código,
cabeceras ni binarios del fabricante (sala limpia).

- «Who owns the menu bar», pkgdemon:
  <https://pkgdemon.github.io/apple-menu-ownership.html>
- La documentación pública de AppKit para `NSMenu`, `NSMenuItem`,
  `NSMenuDelegate`, `NSStatusBar`, `NSStatusItem`, `NSMenuItemValidation` y
  `NSServices`.

## 4. Dock y Files

### 4.1 Decisión

- **Files se escribe nuevo.** No se conserva nada de Filer.
- **El Dock recibe un núcleo nuevo** y se portan tres partes del actual: las
  claves de preferencias y el formato de los datos de tiles, la ventana del
  fondo de escritorio, y el cálculo de disposición de los tiles como
  referencia.

**Regla general**, como en WindowServer: donde la plataforma de referencia
tiene un diseño conocido (qué proceso hace qué, claves de preferencias, API
públicas), el Dock y Files lo siguen. Un atajo solo se permite durante el
desarrollo si la sección 4.10 lo registra con el hito que lo elimina.

### 4.2 Por qué: hallazgos en el código actual

#### Filer (`CoreServices/Filer/FilerMain.m`, 94 líneas)

Una ventana titulada «Filer - /Applications» sin ninguna vista. Los archivos
que se sueltan sobre ella se copian a la carpeta, borrando antes lo que había
sin preguntar; solo sabe abrir bundles `.app` (`LSOpenCFURLRef`). No hay
navegación, vista de iconos, lista o columnas, papelera ni «Abrir con». No hay
nada que conservar.

#### Dock (`CoreServices/Dock/`, unas 1.300 líneas)

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

### 4.3 Quién hace qué (organización de la plataforma de referencia)

| Trabajo | Proceso |
| --- | --- |
| Tiles del Dock, indicadores de apps en ejecución, ventanas minimizadas, pilas, tile de la papelera | Dock |
| Fondo de escritorio, una ventana por pantalla en `kCGDesktopWindowLevel` | Dock |
| Selector de apps (Comando-Tab) | Dock, que recibe Comando-Tab de la tabla de atajos simbólicos del servidor de ventanas (hoy dibuja uno el WindowServer anterior en `WSInput.m`; desaparece con `Legacy/` en WS4b) |
| Ventanas del navegador de archivos, iconos del escritorio en `kCGDesktopIconWindowLevel`, contenido y vaciado de la papelera, «Abrir con», Obtener información | Files |
| Arrancar el Dock y Files al iniciar sesión, y de nuevo si terminan | loginwindow (Fase 5; launchd hasta entonces) |

Spaces, una vista general de ventanas y una cuadrícula para lanzar apps
quedan fuera de este plan; el modelo del Dock les deja sitio.

### 4.4 Arquitectura del Dock

- **Modelo.** `DockModel` con secciones: apps fijas, apps recientes y en
  ejecución, otros elementos fijos (carpetas y pilas), la pila de Descargas,
  la papelera. `DockTile` son datos simples (URL, bundle ID, tipo, banderas,
  insignia, ventanas). Las vistas solo muestran el modelo.
- **Vistas.** Una ventana del Dock por pantalla donde se muestre, dispuesta
  según el modelo; los tiles como capas de Core Animation (rebote,
  magnificación, deslizamiento al ocultarse), así que no se redibuja nada
  mientras nada cambia. El aspecto sigue la guía del lenguaje visual Atlas.
- **Orígenes de datos.** Las notificaciones de `NSWorkspace`
  (`NSWorkspaceDidLaunchApplicationNotification`,
  `NSWorkspaceDidTerminateApplicationNotification`,
  `NSWorkspaceDidActivateApplicationNotification`, ocultar y mostrar) y
  `NSRunningApplication`; iconos a través de LaunchServices y
  `-[NSWorkspace iconForFile:]`; activación con
  `-[NSRunningApplication activateWithOptions:]`.
- **Ventanas minimizadas.** Como en la plataforma de referencia, son del
  Dock: `miniaturize:` de AppKit se lo pide al Dock, que hace la animación
  (`mineffect`) y guarda el tile. El servidor de ventanas (WS4a) retira la
  ventana y la vuelve a mostrar, avisa al Dock cuando una ventana se
  minimiza o se restaura, y le da la imagen de la ventana con
  `CGWindowListCreateImage` y un solo ID de ventana (el Dock es un cliente
  del sistema con ese permiso). Restaurar pide al servidor que muestre la
  ventana y activa su app.
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
  ventanas (WS4a) envía Comando-Tab al Dock en lugar de entregarlo como evento
  de teclado; el Dock muestra el selector y activa la app elegida. El
  selector del servidor anterior (`WSInput.m`) desaparece con `Legacy/` en
  WS4b, así que hasta D3 no hay ninguno.

### 4.5 Arquitectura de Files

- **App.** Bundle ID `org.minkar.Files`, instalada como
  `/System/Library/CoreServices/Files.app`; siempre en marcha en una sesión
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
- **Mostrar en Files.** `open -R`,
  `-[NSWorkspace activateFileViewerSelectingURLs:]` y
  `selectFile:inFileViewerRootedAtPath:` abren una ventana de Files con el
  elemento seleccionado.
- **Doble clic en un DMG** (Fase 6): monta la imagen y abre su volumen
  (K3, para el gate M6).
- **Después.** Búsqueda sobre el índice de metadatos (`mdfind`, Fase 5),
  etiquetas, vistas previas.

### 4.6 Dependencias

| La necesita | Dependencia | Estado hoy | Dónde |
| --- | --- | --- | --- |
| Dock | Las notificaciones de `NSWorkspace` de apps lanzadas, cerradas y activadas y el `NSRunningApplication` básico | Faltan (solo existe el antiguo `launchedApplications`) | Servicio de sesión de LaunchServices, plan de WindowServer WS4a |
| Dock | El resto de `NSRunningApplication` (icono, fecha de arranque, ocultar y mostrar, `activationPolicy`, terminar) | Falta | AppKit, D0 |
| Dock | La parte del servidor de ventanas al minimizar (retirar y mostrar, avisos al Dock) y la captura de una sola ventana | Faltan | Plan de WindowServer, WS4a |
| Dock | Relevos eliminados, `kWSApplicationActivate` retirado | Planeado | Plan de WindowServer, sección 10.2 (WS4b) |
| Dock | `NSDockTile` enviando su estado | Guarda valores, no envía nada | AppKit, D3 |
| Dock | `setDesktopImageURL:forScreen:options:error:` | Por comprobar | AppKit, D1 |
| Files | Base de datos de bundles y UTI de LaunchServices, apps predeterminadas, «Abrir con» | Fase 3, punto 4 | Frameworks |
| Files | Eventos del sistema de archivos: existe el `/dev/fsevents` de XNU (`vfs_fsevents.c`); faltan un daemon y la API FSEvents | Faltan | CoreServices, antes de K1 |
| Files | `NSCollectionView`, `NSOutlineView`, `NSBrowser` funcionando bien | Presentes, en parte con stubs | AppKit, con K1 |
| Files | Iconos, papelera y «mostrar» de `NSWorkspace` | Parciales | AppKit, K1–K2 |
| Ambos | loginwindow arrancándolos al iniciar sesión | Fase 5 | loginwindow |
| Ambos | La guía del lenguaje visual Atlas | Falta | Diseño, antes de D2 y K1 |

### 4.7 Hitos

Cada hito termina con pruebas que pasan, `boottest GATE=M2` (y, cuando
exista, el gate M4) todavía en verde y el roadmap actualizado con la
evidencia.

#### Dock

- [x] **D0 — Requisitos previos.** El resto de `NSRunningApplication` (icono,
  fecha de arranque, ocultar y mostrar, `activationPolicy`, terminar) sobre la
  lista que guarda el servicio de sesión de LaunchServices desde WS4a, y la API
  del fondo de escritorio. Las notificaciones de apps lanzadas, cerradas y
  activadas y el `NSRunningApplication` básico llegan con WS4a. Pruebas: se
  leen el icono y la fecha de arranque de una app de prueba; ocultarla,
  mostrarla y terminarla con `NSRunningApplication` funciona; la elección del
  fondo de escritorio se guarda. *Hecho (4 nov 2026):* icono, fecha de
  arranque, `activationPolicy`, terminar y la API del fondo de escritorio
  (`cf62bba724`, `41cf45da95`); ocultar y mostrar a través de
  launchservicesd (`606d680705`); prueba local `desktop` (34 comprobaciones).
- [x] **D1 — Núcleo nuevo.** `DockModel`, preferencias con las claves de la
  referencia, tiles de apps fijas y en ejecución, lanzar y activar,
  indicadores de ejecución, fondos de escritorio por pantalla. Se va el
  código anterior: `Trash.app`, `Downloads.app`, `AppDir`, las
  notificaciones reenviadas. Pruebas: pruebas del modelo; lanzar una app
  muestra su tile y su indicador, cerrarla lo quita; las preferencias se
  guardan y se leen igual; `screendump` muestra el Dock y el fondo; el Dock
  no consume CPU en reposo. *Hecho (4 nov 2026):* `8cfe7115db`,
  `f6c2d254be`; prueba local `desktop` (27 comprobaciones).
- [ ] **D2 — Interacción.** Arrastrar para reordenar, añadir y quitar; menús
  contextuales (Mantener en el Dock, Abrir al iniciar sesión, Mostrar en
  Files, Salir) y el menú de la propia app; ocultación automática,
  magnificación, orientación; la animación de minimizar y restaurar
  (`mineffect`) para `miniaturize:`. *Parcial (4 nov 2026, `5c083e0b97`):*
  arrastrar, los menús de los tiles, ocultación automática, magnificación y
  orientación; Mostrar en Files (`7e7ea6dfbb`); faltan Abrir al iniciar
  sesión y la animación de minimizar.
- [ ] **D3 — Servicio de tiles, ventanas minimizadas y selector de apps.**
  `org.minkar.dock.server`: insignias, contenido, rebote de atención;
  ventanas minimizadas con miniaturas (después de WS4a); el selector de apps
  en el Dock, que recibe Comando-Tab de la tabla de atajos simbólicos del
  servidor de ventanas.
- [ ] **D4 — Pilas y papelera.** Pilas de Descargas y de carpetas; tile de la
  papelera (vacía o llena, soltar para tirar, vaciar a través de Files).
  *Parcial (6 nov 2026):* el tile de la papelera, el último tras un
  separador propio: su icono sigue a `~/.Trash` con una vigilancia vnode
  (sin sondeo), un clic la abre, su menú la abre o la vacía (preguntando
  antes) y un tile de app soltado encima sale del Dock; probado en la
  prueba de imagen `desktop`. Falta: pilas, soltar archivos, Devolver y
  las `.Trashes` de otros volúmenes.

#### Files

- [ ] **K0 — Requisitos previos.** Daemon y API de eventos del sistema de
  archivos; abrir y «Abrir con» de LaunchServices; las vistas de AppKit
  comprobadas con apps de prueba. *Parcial (4 nov 2026):* fseventsd y
  FSEvents (`a2a333d45f`, prueba `fsevents`), abrir con LaunchServices y las
  apps de un archivo (`0331f57894`, prueba `launchservices`); faltan las
  vistas de AppKit (Files dibuja las suyas).
- [x] **K1 — Navegación.** Ventanas con barra lateral, vistas de iconos, lista y
  columnas, navegación, abrir archivos y apps, Obtener información básico.
  Filer pasa a ser Files: bundle ID, `LSRevealInFiler` como alias obsoleto del
  nombre nuevo durante una versión, `open -R`, los tiles predeterminados del
  Dock. Es la parte «Files navega» del gate M5. *Hecho (4 nov 2026):*
  `896749b380`, `c9f602b43c`, `45c2c1ad9e`, `788fe67376`, `906f12eae4`;
  pruebas locales `argos` (16) y `launchservices` (24).
- [ ] **K2 — Operaciones con archivos.** Copiar, mover, renombrar y duplicar con
  progreso y deshacer; arrastrar y soltar entre ventanas y sobre el Dock;
  papelera con «Devolver»; «Abrir con».
- [ ] **K3 — Escritorio y sesión.** Iconos del escritorio y volúmenes con
  expulsar; API para mostrar en Files; arranque y rearranque por loginwindow;
  doble clic en un DMG (Fase 6, paso 3 de DMG).
- [ ] **K4 — Trabajo posterior.** Búsqueda sobre el índice de metadatos,
  etiquetas, vistas previas.

El gate M5 («desde el arranque se llega a una sesión gráfica, se abre una app
desde el Dock, se navega con Files y se cambia la resolución desde
Preferencias») necesita D1 y K1, además de loginwindow y Preferencias del
Sistema.

El gate M6 (Minkar 19.6.0, un escritorio para el uso diario) necesita D2 y
D3, y K2 y K3. D4 y K4 llegan en 19.6.x.

### 4.8 Pruebas y mediciones

- Pruebas del modelo (secciones y orden de tiles del Dock, ida y vuelta de
  preferencias; operaciones con archivos y registros de la papelera de Files)
  como programas de línea de comandos en la suite de pruebas.
- `boottest` con QMP: comparaciones de `screendump` y clics inyectados (abrir
  una app desde el Dock; navegar una carpeta en Files).
- CPU en reposo del Dock y de Files cercana a cero; tiempo desde un clic en el
  Dock hasta la primera ventana de la app, medido igual en la VM y en los
  equipos físicos.

### 4.9 Reglas de este trabajo

Las mismas que la sección 8 del plan de WindowServer: inglés en todo lo que
se sube salvo las traducciones `.es.md`; sin nombres comerciales de terceros;
nombres de API y de símbolos como son; código nuevo MIT o BSD-2-Clause; sala
limpia; commits pequeños mostrando antes el diff; evidencia para cada punto
del roadmap; sin líneas `Co-Authored-By`.

### 4.10 Decisiones y atajos de desarrollo

| Pregunta | Decisión (objetivo) | Atajo de desarrollo | Se quita en |
| --- | --- | --- | --- |
| Preferencias del Dock | Dominio `org.minkar.dock`, claves y formato de datos de tiles de la referencia | — | — |
| Quién dibuja el escritorio | Dock: fondos; Files: iconos | Escritorio sin iconos hasta K3 | K3 |
| Selector de apps | Dock, con Comando-Tab de la tabla de atajos del servidor de ventanas | Servidor de ventanas anterior (`WSInput.m`) hasta WS4b; después ninguno: se cambia de app con clic en sus ventanas (`SHORTCUT(D3)`) | D3 |
| Ventanas minimizadas | Miniaturas desde la captura del servidor de ventanas | Icono de ventana genérico | D3 (después de WS4a) |
| Arranque del Dock y de Files | loginwindow, que los rearranca si terminan | Agentes de launchd | Fase 5 (loginwindow) |
| Aspecto | Guía del lenguaje visual Atlas | Controles y colores del sistema, sin estilo propio | Cuando exista la guía |

Cada atajo queda marcado en el código con `// SHORTCUT(Dn):` o
`// SHORTCUT(Kn):` y el hito que lo elimina.

## 5. Aplicaciones

### 5.1 Decisión

- **Las apps que necesita un escritorio de uso diario, propias y en
  Cocoa.** Un primer conjunto es obligatorio para Minkar 19.6.0
  (esenciales, 5.2); un segundo llega en 19.6.x; el navegador y un IDE son
  trabajo a largo plazo.
- **Cada app tiene su hito** (A*n*) con pruebas en la imagen, como las
  piezas de la sesión, y sus dependencias listadas (5.4).
- **Primero la base común** (A0), para que todas las apps tengan la misma
  ventana Acerca de, preferencias, ayuda, traducción y manejo de
  documentos.

### 5.2 Las apps

Los nombres, confirmados el 5 nov 2026 (en inglés, y en español):

| Hito | App | Qué hace | Sobre |
| --- | --- | --- | --- |
| A0 | (base común) | La plantilla de app: ventana Acerca de, ventana de preferencias, menú Ayuda, español e inglés, `NSDocument` | AppKit |
| A1 | Terminal (Terminal) | Shells en ventanas y pestañas: un emulador VT100/xterm sobre un pty, historial, selección y copia, Agave | pty, `NSTextView` o una vista de rejilla de texto, CoreText |
| A2 | Text Editor (Editor de texto) | Documentos de texto plano y RTF, buscar y reemplazar, codificaciones, finales de línea, impresión | `NSDocument`, Cocoa Text System |
| A3 | System Monitor (Monitor del sistema) | Procesos (CPU, memoria, energía, disco, red), salir y forzar salida, muestreo; gráficas | libproc, `sysctl`, los datos de `top` |
| A4 | Logs (Registros) | Los registros del sistema y del usuario, en vivo y pasados, con búsquedas; informes de cuelgues y pánicos | `logd`, `log`, `os_log` |
| A5 | Screenshot (Captura de pantalla) | La pantalla, una ventana o una selección, a un fichero o al portapapeles; temporizador; los atajos del sistema | `screencapture`, `CGWindowList` |
| A6a | Image Viewer (Visor de imágenes) | Imágenes: zoom, giro, presentación, exportar | ImageIO (por escribir) |
| A6b | Document Viewer (Visor de documentos) | Documentos PDF: páginas, zoom, búsqueda, exportar | PDFKit (por escribir) |
| A7 | Disks (Discos) | Discos, particiones y volúmenes: montar, expulsar, borrar, verificar, imágenes de disco | DiskArbitration, `diskutil`, `hdiutil` |
| A8 | System Information (Información del sistema) | Informe de hardware y software, como `system_profiler` | `system_profiler`, IOKit |
| A9 | Calculator (Calculadora) | Modos básico, científico y de programador | AppKit |
| A10 | Archive Manager (Gestor de archivadores) | Abre y crea archivos zip, tar y xz desde Files | `tar` (libarchive), `ditto` |

Después de 19.6.0 (19.6.x):

| App | Qué hace | Espera a |
| --- | --- | --- |
| Fonts (Tipografías) | Instala, muestra y desactiva fuentes | Gestión de fuentes de CoreText |
| Passwords and Keys (Contraseñas y claves) | Contraseñas y certificados | Security (Fase 3, punto 3) |
| Color Picker (Selector de color) | El color bajo el puntero, en varios espacios | ColorSync (Little CMS) |
| Script Editor (Editor de scripts) | Escribe y ejecuta scripts | Open Scripting Architecture (`osascript`) |
| Help (Ayuda) | Los libros de ayuda de las apps | el menú Ayuda (MB2) |
| Characters (Caracteres) | Busca e inserta cualquier carácter | Datos de Unicode (ICU) |

A largo plazo, fuera de la puerta de 19.6.0: un navegador web (Ladybird,
BSD-2, ya con su propia interfaz en AppKit) y un IDE (CodeEdit depende de
SwiftUI, fuera de la API 19–21).

### 5.3 Hitos

- [ ] **A0 — Base común de las apps.** Ventana Acerca de (la de Atlas, con
  icono, versión y copyright), menús estándar de Preferencias y Ayuda, los
  dos idiomas, `NSDocument` con autoguardado y versiones en el sistema de
  archivos, la plantilla de la que parten A1–A10. Pruebas: la ventana
  Acerca de de cada app, un menú traducido, un documento guardado y
  reabierto.
- [ ] **A1 — Terminal.** Secuencias VT100 y xterm (colores, movimiento del
  cursor, pantalla alternativa), UTF-8, cambio de tamaño (`SIGWINCH`),
  historial, selección, copiar y pegar, Agave y su tamaño, varias ventanas
  y pestañas, `open -a Terminal`. Pruebas: las pantallas básicas de
  `vttest`, una sesión de `vim` dirigida por QMP, comparaciones con
  `screendump`.
- [ ] **A2 — Text Editor para uso diario.** Empezado el 4 nov 2026 (texto plano,
  abrir y guardar, `open -t`). Falta: RTF, buscar y reemplazar,
  codificaciones y finales de línea, impresión, la regla. Pruebas: un
  fichero de ida y vuelta en cada codificación, buscar y reemplazar, un
  documento RTF reabierto.
- [ ] **A3 — Monitor del sistema.** Lista de procesos con orden y búsqueda,
  pestañas de CPU, memoria, disco y red con gráficas, información de
  proceso, salir y forzar salida. Pruebas: un proceso de prueba aparece
  con su PID y se termina desde la app.
- [ ] **A4 — Logs (Registros).** `log show` y `log stream` en una ventana, filtros
  por proceso, subsistema y nivel, informes de cuelgues de `/Library/Logs`.
  Pruebas: un mensaje `os_log` de un programa de prueba aparece con su
  subsistema.
- [ ] **A5 — Captura.** Pantalla completa, ventana y selección,
  Comando-Mayúsculas-3 y Comando-Mayúsculas-4 por la tabla de atajos
  simbólicos, ficheros en el escritorio con la fecha como nombre, el
  portapapeles. Pruebas: el atajo produce un PNG que coincide con
  `screendump`.
- [ ] **A6a — Image Viewer (Visor de imágenes).** Imágenes por ImageIO
  (PNG, JPEG, TIFF, GIF, WebP), escrito para él. Pruebas: cada formato se
  abre y coincide con una imagen de referencia.
- [ ] **A6b — Document Viewer (Visor de documentos).** PDF por PDFKit,
  escrito para él: páginas, zoom, búsqueda, selección y copia. Pruebas:
  una página PDF se compara con una imagen de referencia; se encuentra una
  palabra.
- [ ] **A7 — Discos.** Volúmenes y particiones, montar y expulsar, borrar
  (HFS+, FAT, exFAT), verificar, crear y conectar imágenes de disco.
  Pruebas: se crea una imagen, se borra, se monta y se expulsa desde la
  app.
- [ ] **A8 — Información del sistema.** El informe de `system_profiler`,
  navegable. Pruebas: el resumen de hardware coincide con
  `system_profiler`.
- [ ] **A9 — Calculadora.** Modos básico, científico y de programador,
  cinta de papel, entrada por teclado. Pruebas: una serie de cálculos con
  teclas por QMP.
- [ ] **A10 — Archive Manager (Gestor de archivadores).** Expandir archivos zip, tar, gzip y xz
  desde Files (doble clic) y comprimir una selección. Pruebas: un archivo
  creado y expandido desde Files.

### 5.4 Dependencias

| Lo necesita | Dependencia | Dónde |
| --- | --- | --- |
| A1 | Dibujo rápido de texto en rejilla; eventos de teclado con todos los modificadores | AppKit, CoreText (Fase 5, punto 7) |
| A2 | Lectura y escritura de RTF, impresión | AppKit (`NSAttributedString`, `NSPrintOperation`) |
| A3 | CPU, memoria, disco y red por proceso | libproc y `sysctl` (Fase 2) |
| A4 | Leer el almacén de registros desde un programa | `logd` (hecho el 3 nov 2026) |
| A5 | Imágenes de ventanas; los atajos del sistema | `CGWindowListCreateImage`; los atajos simbólicos del WindowServer |
| A6a, A6b | ImageIO y PDFKit | por escribir (pila gráfica de la Fase 4) |
| A7 | Borrar y verificar desde un programa | DiskArbitration, `newfs_*`, `fsck_*` |
| Todas | Iconos | los diseños del mantenedor (1) |

### 5.5 Pruebas

Como en la sesión: cada hito añade a la suite local pruebas en la imagen
(el comportamiento del programa a través de sus ficheros, su portapapeles
y `screendump`) e interacción por QMP, y la puerta M6 abre las apps de la
lista de 19.6.0.

## 6. Puertas

- **M5** (Fase 5, sin cambios): desde el arranque se llega a una sesión
  gráfica, se abre una app desde el Dock, se navega con Files y se cambia
  la resolución desde Preferencias, con el lenguaje visual que necesita
  K1; MB1 forma parte de ella.
- **M6 y Minkar 19.6.0** (un escritorio de uso diario): MB2 y MB3, D2 y
  D3, K2 y K3, y las apps esenciales A0–A10; el navegador no es
  obligatorio.
- **19.6.x:** MB4, D4, K4 y las apps de la segunda tabla de 5.2.
