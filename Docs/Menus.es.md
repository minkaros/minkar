# Plan de los menús

Estado: **aceptado** (1 nov 2026). Fases 4 y 5 de [ROADMAP.es.md](ROADMAP.es.md).
Versión en inglés: [Menus.md](Menus.md).
Verificado contra `devel` en `4808c1000`.

Este documento es el registro de la decisión y el plan de trabajo de la barra
de menús, los menús, los status items y el menú del sistema. Sigue las mismas
reglas que [WindowServer-Rewrite.es.md](WindowServer-Rewrite.es.md) y detalla
las filas de menús, status items y documentos recientes de su sección 10.2, y
la parte del menú del sistema de su sección 10.3.

## 1. Decisión

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
desarrollo si la sección 10 lo registra con el hito que lo elimina.

## 2. Por qué: hallazgos en el código actual

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

## 3. Quién hace qué (organización de la plataforma de referencia)

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
muestra la de la nueva; las ventanas de los status items no se mueven. Kraz
siempre está en ejecución, así que es la app del frente cuando no hay otra.

Como los menús corren en el proceso de la app, una app colgada no puede abrir
sus menús, igual que en la plataforma de referencia; el servidor de ventanas
muestra el cursor de ocupado sobre ella y Forzar salida (loginwindow) sigue
disponible.

## 4. Comportamiento que hay que seguir

### Barra de menús

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

### Seguimiento

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

### Atajos de teclado

- En el orden documentado: la ventana key y sus vistas
  (`performKeyEquivalent:`), después el menú principal, validando antes cada
  elemento.
- El menú muestra los atajos con los símbolos de los modificadores.
- Los atajos reservados por el sistema los reconoce el servidor de ventanas y
  los envía a su dueño, sin entregarlos como eventos de teclado: Comando-Tab
  al Dock, Comando-Opción-Escape a loginwindow, Control-F2 al AppKit de la app
  del frente, que lleva el foco a la barra de menús.

### Menús estándar

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

### Otros menús

- Menús contextuales: `menuForEvent:`, Control-clic,
  `+popUpContextMenu:withEvent:forView:`,
  `-popUpMenuPositioningItem:atLocation:inView:`.
- Botones emergentes (`NSPopUpButton`) con las mismas ventanas de menú y el
  mismo seguimiento.
- Menús del Dock (`applicationDockMenu:`) a través del servicio de tiles de
  [Dock-Kraz.es.md](Dock-Kraz.es.md) (D2).

### Status items

- `-[NSStatusBar statusItemWithLength:]` con `NSVariableStatusItemLength` y
  `NSSquareStatusItemLength`; `button` (`NSStatusBarButton`), `menu`,
  `length`, `visible`, `autosaveName`, `behavior`.
- Cada elemento es una ventana de su app de nivel `kCGStatusWindowLevel`; el
  servidor de ventanas los coloca de derecha a izquierda, junto con los Menu
  Extras del sistema; Comando-arrastrar los reordena y el orden se conserva.

### Accesibilidad

La barra de menús, los menús y sus elementos exponen sus roles de
accesibilidad, para que las herramientas de asistencia y la API de
accesibilidad puedan leerlos y pulsarlos.

## 5. Arquitectura

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

## 6. Dependencias

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
| Todos | El lenguaje visual Gienah (tipografías, medidas y colores de los menús) | Fase 5, punto 7 |
| Menús del Dock | Servicio de tiles del Dock | [Dock-Kraz.es.md](Dock-Kraz.es.md), D2 |

## 7. Hitos

Cada hito termina con pruebas que pasan, los gates existentes todavía en verde
y el roadmap actualizado con la evidencia. Los hitos se llaman MB*n* para
distinguirlos de los gates M*n*.

- [ ] **MB0 — Menús en la app (parte de WS4b).** Las vistas de menú pasan a
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
  reiniciar. El gate M5 necesita MB1.
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

## 8. Pruebas y mediciones

- Pruebas del modelo (orden de validación, orden de los atajos,
  conservación de tags) como programas de línea de comandos en la suite de
  pruebas.
- `boottest` con QMP: abrir menús, elegir elementos, atajos, menús
  contextuales, status items; comparaciones de `screendump`.
- Tiempo desde el clic en el título de un menú hasta que el menú aparece en
  pantalla, medido igual en la VM y en los equipos físicos.

## 9. Reglas de este trabajo

Las mismas que la sección 8 del plan de WindowServer: inglés en todo lo que se
sube salvo las traducciones `.es.md`; sin nombres comerciales de terceros;
nombres de API y de símbolos como son; código nuevo MIT o BSD-2-Clause; sala
limpia; commits pequeños mostrando antes el diff; evidencia para cada punto
del roadmap.

## 10. Decisiones y atajos de desarrollo

| Pregunta | Decisión (objetivo) | Atajo de desarrollo | Se quita en |
| --- | --- | --- | --- |
| Quién dibuja y sigue los menús de la app | La app (AppKit), en su propio proceso | Ninguno: SystemUIServer dibujaba una copia, y no se compila | MB0 |
| Menú del sistema | Logo de Minkar, los elementos de la plataforma de referencia, acciones a través de loginwindow | Reiniciar, Apagar y Cerrar sesión a través del `loginwindow` mínimo; Reposo, Forzar salida, Bloquear pantalla y Preferencias del Sistema desactivados | MB3 |
| Status items | Ventanas de sus apps, dispuestas por el servidor de ventanas | Solo la API básica | MB1 |
| Menu Extras | Bundles en SystemUIServer | Ninguno hasta que SystemUIServer se compile | MB1 |
| Documentos recientes | Listas compartidas de archivos de LaunchServices | Guardados por app por `NSDocumentController` en sus preferencias | MB2 |
| Menú Servicios | Daemon de servicios y pasteboard | Menú Servicios vacío | MB4 |
| Grupo de estado del sistema | API 19: Menu Extras en SystemUIServer; API 20: una app de centro de control aparte | Ninguno | Minkar 20.0 |
| Aspecto | Lenguaje visual Gienah | Tipografía y colores del sistema, sin estilo propio | Fase 5, punto 7 |

Cada atajo queda marcado en el código con `// SHORTCUT(MBn):` y el hito que lo
elimina.

## 11. Referencias

Solo descripciones públicas del comportamiento observado; nada de código,
cabeceras ni binarios del fabricante (sala limpia).

- «Who owns the menu bar», pkgdemon:
  <https://pkgdemon.github.io/apple-menu-ownership.html>
- La documentación pública de AppKit para `NSMenu`, `NSMenuItem`,
  `NSMenuDelegate`, `NSStatusBar`, `NSStatusItem`, `NSMenuItemValidation` y
  `NSServices`.
