# Plan de reescritura de WindowServer

Estado: **aceptado** (31 oct 2026). Fase 4 de [ROADMAP.es.md](ROADMAP.es.md).
Versión en inglés: [WindowServer-Rewrite.md](WindowServer-Rewrite.md).
Verificado contra `devel` en `21fb25034`.

Este documento es a la vez el registro de la decisión y el plan de trabajo.
Lee «Reglas de este trabajo» antes del primer commit.

## 1. Decisión

Escribir un núcleo nuevo de WindowServer y portar las partes del actual que
vale la pena conservar. No seguir ampliando el núcleo actual, ni tirarlo
entero.

Antes de que exista el núcleo nuevo, arrancar una vez el WindowServer actual,
tal como está, para tener los primeros píxeles en pantalla (hito WS0). Eso
valida de punta a punta la entrada del kernel, el framebuffer y el dibujo de
AppKit, y le da pronto al proyecto un gate gráfico de `boottest`. Después de
WS0, el núcleo actual solo recibe los arreglos que WS0 necesite.

**Regla general (decidida el 31 oct 2026):** donde la plataforma de
referencia tiene un diseño conocido (organización de procesos, usuario,
permisos, movimiento de ventanas), el WindowServer nuevo lo sigue, para no
tener que rehacer nada después por compatibilidad. Un atajo solo se permite
durante el desarrollo si está anotado en la sección 9 con el hito que lo
elimina.

## 2. Por qué: hallazgos en el código actual

El código propio son unas 2.800 líneas (MIT, de ravynOS) en
`CoreServices/WindowServer`, más unas 440.000 líneas de código de entrada de
terceros: libinput, libevdev y mtdev ya no se compilan; libxkbcommon sí, para
los layouts de teclado. Nunca se ha ejecutado en Minkar; su única prueba
cubre el keymap.

1. **La composición no puede cumplir el requisito de rendimiento.**
   `-[WindowServer run]` (`WindowServer.m`) redibuja la pantalla completa en
   cada vuelta de un `poll()` con timeout de 50 ms: sin zonas modificadas,
   sin vsync y sin doble búfer sincronizado. Los clientes dibujan en memoria
   compartida mientras el servidor la lee, sin ninguna barrera, así que los
   frames se rompen (tearing). No hay dónde enchufar IOSurface ni un backend
   de GPU («FIXME: lock this to vsync»).
2. **IPC escrita a mano.** `message.h` define structs fijos de unos 128 KB
   (`unsigned char data[128*1024]`) que se copian enteros en cada mensaje,
   incluido un evento de entrada de 60 bytes. Los envíos usan un timeout de
   50 ms, así que bajo carga se pierden eventos. El propio `message.h` dice
   «refactor this to a .defs file and use mig». Los ID de ventana los elige
   cada cliente y solo son únicos junto con el PID del dueño (`rpc.h`,
   `wsWindowListEntry`).
3. **Seguridad.** La captura de pantalla crea su memoria compartida con
   `shmget(..., IPC_CREAT | 0666)` (`IOFramebufferDisplay.m`): cualquier
   proceso puede leer y escribir la pantalla capturada. Las superficies de
   las ventanas son objetos de memoria compartida POSIX con nombre.
4. **Modelo de ventanas.** Estados copiados de X11/Wayland (`MAXVERT`,
   `MAXHORIZ`); sin orden relativo (`orderWindow:relativeTo:`), ventanas
   hijas, sheets, alfa, sombras ni Spaces. El arrastre de ventanas lo decide
   el servidor a partir de la geometría de la barra de título
   (`sendEventToApp:`).
5. **Entrada.** Sin backend de dispositivos (`-[WSInput run:]` está vacío),
   sin lector de IOHIDSystem, layouts de teclado de X a través de
   libxkbcommon, sin repetición de teclas. `CGEventTapCreate`, `CGEventPost`
   y las demás funciones de taps de `Frameworks/CoreGraphics/CGEvent.m` son
   funciones con valor de retorno y cuerpo vacío (comportamiento indefinido).
6. **Responsabilidades mezcladas.** WindowServer lanza LoginWindow y
   SystemUIServer y prepara el entorno del usuario (`launchShell:`,
   `setUpEnviron:`). El arranque de la sesión corresponde a launchd y a la
   ventana de login.
7. **Core Animation.** `CARenderer` compone en el cliente con OpenGL; no hay
   un árbol de capas que el servidor pueda componer.

Lo que vale la pena conservar:

- `IOFramebufferDisplay.m` (363 líneas): el backend de IOFramebuffer ya
  portado a XNU (conexión con el servidor, consulta del modo, mapeo de la
  memoria de pantalla).
- `WSDisplay`: el modelo de pantalla (modos, gamma, espejo, estado de
  captura).
- La lógica de las ~60 llamadas de Quartz Display Services de
  `WindowServer.m` (de `rpcGetOnlineDisplayList:` a `rpcGetLastMouseDelta:`)
  y de la lista de ventanas (`rpcWindowList:`), con sus clientes
  `CGDirectDisplay.m` y `CGWindowList.m`.
- `WSKeymap.c`, como solución temporal hasta que los layouts pasen a
  `UCKeyTranslate`.
- La activación de apps (`activateApp:`, `switchApp`), como referencia.
- SystemUIServer y LoginWindow como clientes.

## 3. Qué significa compatibilidad aquí

El protocolo entre AppKit y WindowServer en la plataforma de referencia es
privado, y Minkar tiene su propia AppKit y su propio CoreGraphics. Así que el
protocolo lo diseñamos nosotros. La compatibilidad está en la API pública y
en su comportamiento:

| Área | API pública | Hoy |
| --- | --- | --- |
| Quartz Display Services | `CGGetActiveDisplayList`, modos, gamma, captura, configuración | Casi completa; portarla |
| Quartz Window Services | `CGWindowListCopyWindowInfo`, `CGWindowListCreateImage` | Parcial |
| Quartz Event Services | `CGEventTapCreate`, `CGEventPost`, `CGEventSourceRef` | Vacía |
| Streams de pantalla | `CGDisplayStream` | Falta |
| Semántica de ventanas | Niveles de `NSWindow`, orden, ventana key y main, ventanas hijas, sheets, collection behavior, alfa, sombras | Mínima |
| Pantallas | Marcos de `NSScreen`, marco visible, escala | Parcial |
| Cursores | `NSCursor`, cursor rects, ocultar y mostrar, warp | Parcial |
| Core Animation | Árboles de capas que dibuja el servidor | Falta |

Fuera de alcance: copiar la SPI privada del servidor de ventanas de la
plataforma de referencia; Spaces y una vista general en la primera versión
(el modelo les deja sitio); HDR; más de una GPU.

## 4. Arquitectura objetivo

### Proceso

- Un daemon, organizado como en la plataforma de referencia: el ejecutable es
  `/System/Library/PrivateFrameworks/SkyLight.framework/Resources/WindowServer`,
  lanzado por launchd desde `org.minkar.WindowServer.plist`, con su servicio
  Mach en `MachServices`. Solo atiende; no lanza nada. `SkyLight` se usa como
  identificador por compatibilidad, igual que cualquier otro nombre de
  framework.
- La ventana de login y la sesión del usuario las arranca launchd
  (LoginWindow como daemon en la consola; los agentes de la sesión después
  del login). El código de `launchShell:` y `setUpEnviron:` se muda allí.
- Corre como el usuario `_windowserver` (UID 88), como en la plataforma de
  referencia. El job de launchd fija el usuario; el kernel le concede a ese
  usuario los user clients del framebuffer y de HID. Root solo hasta WS4
  (sección 9).

### Módulos (nueva organización en `CoreServices/WindowServer/`)

| Módulo | Responsabilidad |
| --- | --- |
| `IPC/` | `WindowServer.defs` (MIG): subsistemas de conexión, ventana, superficie, pantalla, eventos, cursor y captura. Los mensajes llevan solo sus datos; lo grande viaja como memoria out-of-line o memory entries. |
| `Server/` | `main`, colas de dispatch, gestor de conexiones. Cada conexión de cliente guarda PID, UID y audit token del trailer Mach. |
| `Model/` | `WSConnection`, `WSWindow` (`CGWindowID` global asignado por el servidor, dueño, nivel, orden, marco, alfa, opacidad, sombra, padre e hijas, collection behavior, título, estado de compartición), `WSWindowList` (orden dentro de cada nivel y entre niveles), `WSSpace` (un solo space por ahora). |
| `Surfaces/` | `WSSurface`: el almacén de una ventana. Primero un memory entry de Mach (`mach_make_memory_entry_64`) enviado como puerto, nunca un objeto de memoria compartida con nombre; después un IOSurface (paso 4 del roadmap) detrás de la misma interfaz. Dos búferes por ventana: el cliente dibuja en el de atrás y lo confirma con su zona modificada y un número de secuencia; el servidor toma los búferes confirmados al empezar cada frame. |
| `Compositor/` | `WSCompositor` sobre una interfaz `WSRenderBackend`: crear una textura desde una superficie, dibujar un cuadro con alfa, transformación y recorte, presentar con zona modificada, avisar del vsync. La zona modificada de cada ventana, más los movimientos y cambios de orden, dan la zona modificada de la pantalla; solo esa se compone y se presenta. Backends: primero `WSPixmanBackend` (pixman ya está en el árbol), después `WSVulkanBackend` (frente de GPU, G4). |
| `Displays/` | `IOFramebufferDisplay` y `WSDisplay` portados: modos, gamma, espejo, configuración, captura. Un display link por pantalla: GOP no tiene vsync, así que un temporizador a la frecuencia del modo marca el ritmo de los frames; virtio-gpu y KMS darán cambios de búfer reales después. |
| `Input/` | Lector de IOHIDSystem (`kIOHIDServerConnectType`, la cola compartida `EvGlobals`, un puerto Mach de notificación) → canal de eventos: posición y aceleración del cursor, estado de modificadores, repetición de teclas, keymap (`WSKeymap` por ahora), hit testing, taps de eventos, enrutamiento. Las teclas van al dueño de la ventana key; los eventos de ratón, a la ventana bajo el puntero, o a la que recibió el mouse down hasta el mouse up. Cada cliente tiene una cola acotada; los movimientos del ratón se agrupan; el servidor nunca se bloquea esperando a un cliente. |
| `Cursor/` | El cursor se dibuja al final como capa superpuesta, con su propia zona modificada; cursor por hardware después. |
| `Capture/` | `CGDisplayCreateImage`, `CGWindowListCreateImage`, captura de pantalla (el cliente recibe una superficie, no un segmento legible por todos); `CGDisplayStream` después. |
| `Debug/` | `wsctl`: listar ventanas y conexiones, estadísticas de frames, zonas modificadas, latencia de entrada. |

### Hilos

- IPC en una cola serie; entrada en una cola de alta prioridad; un hilo de
  dibujo por pantalla, movido por su display link.
- El modelo solo se cambia en la cola del modelo. El hilo de dibujo trabaja
  sobre una copia inmutable tomada al empezar el frame, así que nunca tiene
  un lock mientras dibuja.

### Movimiento de ventanas

Como en la plataforma de referencia, el servidor mueve las ventanas por sí
mismo, así que una ventana sigue al puntero aunque su app esté ocupada:

- Cada ventana le indica al servidor sus zonas de arrastre (la barra de
  título y, con `movableByWindowBackground`, el fondo) y las actualiza cuando
  cambia su diseño. Un mouse down en una zona de arrastre inicia un
  movimiento en el servidor.
- `-[NSWindow performWindowDragWithEvent:]` inicia el mismo movimiento desde
  la app.
- La app recibe el marco nuevo (`NSWindowDidMoveNotification` y las llamadas
  habituales al delegado) durante el movimiento y al terminar.
- El servidor deja de adivinar a partir de la geometría de la barra de
  título.

### Core Animation

La versión 1 compone la superficie de cada ventana; AppKit dibuja sus capas
en ella, en el cliente. La versión 2 permite que un cliente envíe commits de
su árbol de capas al compositor del servidor (necesario para animaciones
fluidas en los gates de rendimiento). `WSSurface` y `WSCompositor` dejan esa
puerta abierta; el diseño irá en su propio ADR después de M4.

### Lado del cliente

- El lado del cliente vive en el mismo framework privado,
  `PrivateFrameworks/SkyLight.framework`, que envuelve la interfaz MIG:
  conexión, ventanas, superficies, eventos. CoreGraphics lo reexporta, como en
  la plataforma de referencia, así que las funciones privadas `CGS*` y `SLS*`
  que llaman apps de terceros (conexión principal, alfa, nivel y orden de
  ventanas...) se encuentran donde esas apps las esperan. Ese subconjunto se
  implementa solo a partir de información pública y de comportamiento
  observado; el protocolo de transporte sigue siendo nuestro (MIG) y no
  pretende coincidir con el de la referencia.
- CoreGraphics (`CGDirectDisplay.m`, `CGWindowList.m`, `CGEvent.m`) y AppKit
  (`NSApplication`, `NSWindow`, `NSScreen`, `NSEvent`, `NSCursor`) lo usan.
  Los headers públicos de AppKit dejan de incluir headers de WindowServer
  (hoy `<AppKit/NSDisplay.h>` arrastra `<WindowServer/message.h>`).

### Seguridad

- Cada petición se comprueba contra el audit token de quien llama.
- Permisos de privacidad como en la plataforma de referencia desde la API 19:
  - **Grabación de pantalla:** `CGDisplayCreateImage`,
    `CGWindowListCreateImage`, los streams de pantalla y los títulos de las
    ventanas de otras apps en `CGWindowListCopyWindowInfo` (sin el permiso,
    se omite `kCGWindowName`). `CGPreflightScreenCaptureAccess` y
    `CGRequestScreenCaptureAccess`.
  - **Monitoreo de entrada:** taps de solo escucha que ven eventos de
    teclado. `CGPreflightListenEventAccess` y `CGRequestListenEventAccess`.
  - **Accesibilidad / envío de eventos:** taps que filtran y `CGEventPost`.
    `CGPreflightPostEventAccess`, `CGRequestPostEventAccess` y
    `AXIsProcessTrusted`.
- WindowServer consulta un único servicio de permisos en cada comprobación,
  según la identidad del código del cliente (bundle ID, ruta) y el usuario.
  Ese servicio es un daemon con una base de datos de privacidad y tendrá su
  propio ADR; mientras no exista, responde una política interna (sección 9).
  Los puntos de comprobación y las respuestas de la API son los definitivos
  desde WS3.
- Las superficies solo se comparten mediante puertos entregados a su dueño;
  ninguna memoria legible o escribible por todos.

## 5. Hitos

Cada hito termina con pruebas que pasan, `boottest GATE=M2` todavía en verde
y el roadmap actualizado con la evidencia.

### WS0 — Primeros píxeles con el servidor actual

Depende del trabajo de kernel de la Fase 4: la entrega de eventos de
IOHIDSystem (el bucle de despacho bajo el `NEW_HID` sin definir, las colas de
eventos) y el driver `ApplePS2Mouse`. Son tareas aparte, con sus propios
commits.

- Instalar el job de launchd de WindowServer; arrancar hasta un WindowServer
  que dibuja en `IOGOPFramebuffer`, con una ventana de un cliente de
  demostración.
- Solo arreglos mínimos, y la memoria de captura sin acceso para todos
  (`0666`).
- Nuevo gate de `boottest`, `M4-draw`: un `screendump` de QMP comparado con
  una imagen de referencia (con tolerancia), y una tecla y un clic
  inyectados por QMP que llegan a la ventana de demostración como
  `NSEvent`.

Estado al 31 oct 2026:

- [x] (a) Entrega de eventos de IOHIDSystem (`e2df542919`): la conexión del
  servidor de eventos mapea la cola compartida `EvGlobals` y recibe avisos
  por su puerto Mach; los dispositivos de puntero también llegan. El bucle
  de despacho de `NEW_HID` sigue desactivado: alimenta las colas del
  sistema de eventos, que no se compilan y no son lo que lee WindowServer.
  Prueba `hid-events` 9/0.
- [x] (b) `ApplePS2Mouse` (`2aa9a1da6e`): movimiento, tres botones y rueda.
  Prueba `ps2-mouse` 12/0.
- [x] (c) WindowServer: `IOGOPFramebuffer` da la información de píxeles del
  modo actual (`f10f778c78`); el backend de entrada lee la cola y el
  servidor ya no se cae al arrancar (`ab71ce75a4`); la memoria de captura
  es solo del cliente que captura (`e2731c426f`); la conexión de servidor
  del framebuffer se cierra cuando su cliente desaparece (`9f46a761ac`);
  los nombres de las superficies caben en el límite de XNU (`dc931eeeb0`);
  el job de launchd se instala desactivado, con el cliente de
  demostración WindowServerDemo (`f56c70bb88`). AppKit necesitó tres
  arreglos (`70b62fe8bc`), Onyx2D uno para las fuentes (`6888034ad2`) y
  WindowServer uno para las apps sin bundle (`0e3d06ec35`). Prueba
  `windowserver` 16/0.
- [x] (d) Gate `M4-draw` (`cce7da92bc`): la pantalla coincide con la
  referencia (0 de 786432 píxeles distintos, dos veces seguidas), y un clic
  (tableta USB) y una tecla (teclado PS/2) llegan a la ventana de la
  demostración como `NSEvent`. Suite completa con 3023 pruebas superadas
  antes de (d); M2 pasa.

WS0 está hecho (31 oct 2026).

Atajos tomados (sección 9): root puede abrir la conexión de servidor de
IOHIDSystem, porque todavía ningún módulo de política valida las firmas de
código y ningún proceso tiene entitlements (`SHORTCUT(WS0)` en
`IOHIDUserClient.cpp`); los hilos del servidor actual tienen pilas de 4 MB
para sus mensajes de 128 KB (`SHORTCUT(WS0)` en `main.m`); el job de launchd
ejecuta solo el compositor (`-x`), porque LoginWindow no se compila
(`SHORTCUT(WS0)` en su plist); Inter es la fuente provisional del sistema
hasta que Gienah tenga la suya (`SHORTCUT(WS0)` en
`SystemLibrary/Fonts/Makefile`).

Problemas conocidos para hitos posteriores: las decoraciones de las ventanas
son las imágenes heredadas de ravynOS, con el aspecto de la plataforma de
referencia (Gienah las sustituye); los objetos de memoria compartida POSIX
ignoran `fchown`, así que una superficie es solo del usuario del servidor
(las superficies pasan a ser memory entries en WS2); un `NSEvent` se libera
de más al salir una app sin ventanas; `CGLayer.h` no se puede incluir desde
C.

### WS1 — Esqueleto e interfaz

- `WindowServer.defs` y el lado del cliente en `SkyLight.framework`; el
  servidor nuevo instalado en `SkyLight.framework/Resources`.
- El núcleo nuevo se compila junto al viejo; el viejo pasa a `Legacy/` y
  sigue siendo el predeterminado.
- Display Services y la lista de ventanas portados a MIG, con un cliente de
  prueba que llama a cada función y comprueba las respuestas.
- La organización del repositorio de la sección 10.1.

WS1 está hecho (31 oct 2026). El servidor nuevo funciona junto al actual,
que sigue siendo el predeterminado; se registra como
`org.minkar.WindowServer.next` hasta WS4 y launchd todavía no lo arranca.
Commits: estructura (`9e48d1f62a`), interfaz, cliente y servidor
(`f9028e286a`), consultas de pantalla (`0e4d7293ff`), modos y
configuraciones (`8dab4060cc`), tablas de transferencia (`37996817e4`),
conexiones, captura, cursor y lista de ventanas (`41e37b7441`), espejo y
fundidos (`030dba37c7`). `skylight-test` llama a cada rutina y comprueba
las respuestas: 115 de 115 pasan en QEMU, igual que los gates M2 y
M4-draw.

Cada cliente pasa un puerto propio al conectarse; al servidor se le avisa
cuando ese puerto muere y libera lo que tenía el proceso: ventanas,
capturas, su cuenta de ocultación del cursor, su fundido, sus cambios de
pantalla solo para la app y las tablas de transferencia que cambió.

Atajos tomados (`SHORTCUT(WS1)`): una configuración de pantalla permanente
dura solo la sesión; las tablas de transferencia y la mezcla del fundido
las guarda el servidor y las aplica el compositor desde WS2; la posición
del cursor solo cambia con warps hasta que el servidor lea la entrada
(WS3).

Quedan para hitos posteriores: los ID de pantalla salen de la entrada del
registro y duran un arranque (se derivarán del EDID); el espejo se
comprueba con una sola pantalla (rechazos y desactivarlo);
`CGDisplayGetDrawingContext` y los flujos de pantalla llegan con la
captura mediante superficies (WS4); las funciones de CoreGraphics siguen
llamando al servidor actual hasta el cambio (WS4).

### WS2 — Ventanas, superficies y compositor por software

- Modelo de ventanas con `CGWindowID` globales, niveles y orden.
- Superficies como memory entries, con dos búferes y commits con zona
  modificada.
- `WSPixmanBackend`, composición por zonas modificadas, display link a la
  frecuencia de refresco, cursor superpuesto.
- `NSWindow` dibujando a través de `SkyLight.framework`.
- Pruebas: dos ventanas de AppKit superpuestas, cambios de orden y
  movimientos comprobados con `screendump`; estadísticas de frames de
  `wsctl` (ningún frame si nada cambia; solo se redibujan las zonas
  modificadas).

WS2 está hecho (1 nov 2026). Commits: pixman como biblioteca del sistema
(`6ca5e31aae`), superficies y compositor (`1aa140cdab`), AppKit y
CoreGraphics a través de SkyLight (`b957c09fac`), gate M4-compose
(`8512fe60ba`). La superficie de una ventana son dos buffers en memoria
que el servidor comparte con su dueño mediante una entrada de memoria
Mach; los commits llevan el daño, y el cliente mantiene su buffer trasero
igual al delantero. Un compositor por pantalla compone con pixman solo el
área dañada, a la frecuencia de refresco del modo y solo cuando hay daño,
con el cursor al final. `wsctl stats` da frames, ticks en reposo, commits,
área dañada y tiempos de frame. AppKit usa el servidor nuevo cuando el
actual no está en marcha. Pruebas: `skylight-test` 128/0; el test local
del compositor (solapamiento, subir, mover, media transparencia, cursor,
ningún frame en reposo); el gate M4-compose (dos ventanas de AppKit,
subir y mover, 0 píxeles distintos). Dos fallos antiguos corregidos por el
camino: los run loops giraban sobre `select` (`61cb01ce77`), y la
liberación de más de `NSEvent` de WS0 (`1508bd4d3c`).

Atajos tomados: AppKit marca dañada toda la ventana en cada flush
(`SHORTCUT(WS2)`); el reloj de pantalla late a la frecuencia de refresco
también en reposo (sin dibujar ningún frame).

### WS3 — Entrada

- Lector de IOHIDSystem, enrutamiento, repetición de teclas, agrupación de
  movimientos, captura del ratón durante el arrastre, ventana key y main,
  movimientos iniciados por el cliente.
- `CGEventTapCreate`, `CGEventTapEnable`, `CGEventPost` y
  `CGGetEventTapList` implementados.
- Pruebas: las teclas y clics inyectados por QMP llegan a la ventana
  correcta; un tap de solo escucha los ve; un tap que filtra puede descartar
  uno; `CGEventPost` llega a la ventana key.

WS3 está hecho (1 nov 2026). Commits: entrada desde IOHIDSystem y
reparto (`c3763a06a8`), movimientos por el servidor, taps y eventos
publicados (`03bb7ed773`), los eventos de CoreGraphics (`fa7aae5e38`),
AppKit (`13f99802d5`, `d0c63753c2`), gate M4-compose con entrada
(`98237bf14e`). El servidor lee IOHIDSystem y reparte: los eventos de
ratón a la ventana bajo el puntero, o a la que recibió el clic hasta que
se sueltan todos los botones; las teclas a la ventana clave; un clic hace
clave a su ventana. Cuenta los clics, repite las teclas mantenidas, agrupa
los movimientos en la cola acotada de cada conexión y nunca espera a un
cliente. El servidor mueve una ventana desde un clic en su barra de
título. Funcionan `CGEventTapCreate`, `CGEventTapEnable`, `CGEventPost` y
`CGGetEventTapList`; un tap filtrante que no responde en un segundo se
desactiva. Pruebas: el test local de entrada (movimientos, clics, doble
clic, repetición de teclas, un arrastre fuera de la ventana, un tap de
solo escucha que ve la entrada inyectada), `eventtap-test` 15/0 (una
tecla publicada que llega a la ventana clave, un tap filtrante que
descarta una tecla, un tap desactivado por no responder), el gate
M4-compose (un clic, una tecla y un arrastre de la barra de título a
través de AppKit, 0 píxeles distintos). Encontrados por el camino: un
evento liberado dentro de `nextEventMatchingMask:` (`13f99802d5`),
`CGEventMaskBit` definido como su argumento, y `CGLayer.h` sin poder
incluirse desde C (`b4f8ed015d`, el problema conocido de WS0).

Atajos tomados (`SHORTCUT(WS3)`): la entrada se lee y se reparte en la
cola IPC; la política integrada de la sección 9 decide quién publica y
filtra (root o el usuario de la consola); `CGEventSource` todavía no
pregunta el estado HID al servidor. El mapa de teclado y libxkbcommon
siguen siendo los del servidor actual, enlazados desde allí hasta WS4.

### WS4 — Comportamiento de ventanas y cambio de servidor

- `CGWindowListCopyWindowInfo` con las claves de diccionario de la
  referencia, `CGWindowListCreateImage`, ventanas hijas, sheets, alfa,
  sombras por software, `NSCursor` y cursor rects, captura de pantalla
  mediante superficies.
- El arranque de la sesión sale de WindowServer.
- Sección 10.2: menús y status items dibujados por AppKit de cada app;
  avisos de apps lanzadas, cerradas y activadas a través de LaunchServices y
  `NSWorkspace`; los códigos de relevo eliminados de `message.h`. La parte
  de los menús es el hito MB0 de [Menus.es.md](Menus.es.md); WS4 solo se
  cierra cuando pasa MB0.
- Sección 10.3: la ventana de login gestiona el fin de la sesión, en lo que
  necesita la Fase 4 (atajo hasta la Fase 5).
- El núcleo nuevo pasa a ser el predeterminado; se borra `Legacy/`, y
  también los árboles de libinput, libevdev y mtdev.
- Gate M4 en QEMU: varias apps de AppKit abren ventanas, se arrastran,
  reciben teclado y redibujan sin artefactos.

### WS5 — IOSurface y virtio-gpu

- IOSurface (paso 4 del roadmap) detrás de `WSSurface`.
- Pantalla virtio-gpu 2D (paso 3 del roadmap) con cambios de búfer reales y
  cambio de resolución desde `CGDisplaySetDisplayMode`.
- `CGDisplayStream`.

### WS6 — Composición por GPU

- `WSVulkanBackend` (frente de GPU, G4), elegido por pantalla.
- Gate de rendimiento de composición en el PC de referencia, requisito de
  Minkar 21.0: la frecuencia de refresco de la pantalla sin frames perdidos
  con varias ventanas animadas, y un consumo de CPU de WindowServer no mayor
  que el de la referencia.

Después: árboles de capas compuestos por el servidor (con su propio ADR),
varias pantallas, Spaces, cursor por hardware.

## 6. Mediciones incluidas desde WS2

- Tiempo de frame, frames presentados y descartados, área modificada por
  frame.
- Latencia de entrada a pantalla: marca de tiempo en el evento HID, en la
  entrega, en el commit que lo responde y en la presentación.
- Expuestas con `wsctl stats`, para medir los gates de rendimiento de la
  misma forma en la VM y en los equipos físicos.

## 7. Pruebas

- Pruebas del modelo (orden, niveles, zonas modificadas) como programas de
  línea de comandos dentro de la suite de pruebas existente.
- Integración con `boottest`: `M4-draw` desde WS0 y el gate `M4` desde WS4.
- Comparaciones de `screendump` de QMP con tolerancia por píxel; inyección
  de teclado y ratón por QMP.

## 8. Reglas de este trabajo

- Todo lo que se sube al repo va en inglés, salvo las traducciones `.es.md`
  de la documentación. Sin nombres comerciales de terceros en código,
  comentarios, documentación ni mensajes de commit; los nombres de API y de
  símbolos se quedan como son.
- Código nuevo bajo MIT o BSD-2-Clause. Sala limpia: nada de código de las
  fuentes cerradas de la plataforma de referencia; solo documentación
  pública y headers de la API pública.
- Commits pequeños, un tema cada uno; mostrar el diff antes del commit.
- Los cambios de kernel (IOHIDSystem, `ApplePS2Mouse`, IOSurface,
  virtio-gpu) van en commits separados de los de WindowServer.
- Marcar un punto del roadmap como hecho solo con evidencia (commit, prueba,
  salida de `boottest`) y citarla.
- Mantener `boottest GATE=M2` en verde después de cada hito; mantener el
  servidor viejo funcionando hasta el cambio de WS4.

## 9. Decisiones y atajos de desarrollo

Decidido el 31 oct 2026, según la regla general de la sección 1:

| Pregunta | Decisión (objetivo) | Atajo de desarrollo | Se quita en |
| --- | --- | --- | --- |
| Usuario con el que corre WindowServer | `_windowserver` (UID 88), fijado por el job de launchd | Root | WS4 |
| Dónde vive | `SkyLight.framework/Resources/WindowServer`; el cliente en `SkyLight.framework`, reexportado por CoreGraphics | `CoreServices/WindowServer.app` para WS0 (servidor actual) | WS1 |
| Capturar pantalla y enviar eventos | Permisos de grabación de pantalla, monitoreo de entrada y accesibilidad, mediante un servicio de permisos | Política interna: se permite a root y a los procesos del usuario de consola; el resto, denegado | ADR del servicio de privacidad (antes de M5) |
| Mover ventanas | Movimientos en el servidor a partir de las zonas de arrastre que declara el cliente, más `performWindowDragWithEvent:` | Solo la zona de la barra de título | WS4 |
| Terminar la sesión | La ventana de login pide a las apps que terminen, cierra la sesión y después reinicia o apaga | Un `loginwindow` mínimo que solo cierra la sesión y llama a `reboot(2)` | Fase 5 (LoginWindow) |
| Validación de entitlements en el kernel | Una trust cache estática de los binarios del sistema y un kext de política MAC propio (sección 9.1) | Root puede abrir la conexión de servidor de IOHIDSystem (`SHORTCUT(WS0)` en `IOHIDUserClient.cpp`) | Antes de WS4 |

### 9.1 Validación de entitlements en el kernel

XNU solo respeta los entitlements de un proceso cuando su firma de código está
marcada como válida (`CS_VALID`, comprobado en `csblob_get_entitlements`), y
esa marca la pone un módulo de política de integridad de código. El módulo de
la plataforma de referencia es de código cerrado, así que hoy ningún proceso de
Minkar tiene entitlements. Decidido el 31 oct 2026:

- **Trust cache.** El build calcula el CDHash de cada binario del sistema
  firmado y escribe una trust cache estática (con el formato de
  `kern/trustcache.h`) que viaja en la imagen de arranque.
- **Kext de política.** Un módulo de política MAC propio marca como
  `CS_VALID` y como binario de plataforma todo binario cuyo CDHash está en la
  trust cache.
- **Dos clases de entitlements, como en la plataforma de referencia.** Los
  restringidos (los privados y los de acceso a hardware, como
  `com.apple.hid.system.server-access`) solo se respetan en binarios de
  plataforma. Los no restringidos (hardened runtime, sandbox) se respetan con
  cualquier firma válida, incluida la ad hoc. Los binarios fuera de la trust
  cache se siguen ejecutando, salvo si llevan un entitlement restringido:
  entonces el exec se rechaza, como en la plataforma de referencia (ADR 0001).
- **Nombres.** Los nombres de los entitlements se quedan como los comprueban
  el kernel y los frameworks.
- **Después.** Firmas con certificado, verificadas con corecrypto, para lo que
  se firme fuera del build y para las apps de terceros (Fase 7); el kext de
  política deja sitio para esa comprobación. Un daemon de validación en espacio
  de usuario solo si lo necesitan provisioning profiles o comprobaciones caras.
- **Hito.** Primero su propio ADR (`Docs/ADR/0001-Kernel-Entitlements.es.md`,
  aceptado el 2026-11-01); después la implementación, antes de WS4, con
  pruebas: un binario del sistema que está en la trust cache obtiene un
  entitlement restringido; una copia refirmada fuera de la caché se rechaza; un binario
  firmado ad hoc conserva sus entitlements no restringidos. Entonces se quita
  `SHORTCUT(WS0)` de `IOHIDUserClient.cpp`.

Cada atajo queda marcado en el código con `// SHORTCUT(WSn):` y el hito que
lo elimina, para listarlos todos con `git grep`.

## 10. Reestructuración alrededor de WindowServer

La reescritura cambia también lo que rodea a WindowServer: lo que vive en su
directorio, los mensajes que reenvía entre apps y quién gestiona la sesión.
Cada parte sigue la organización de la plataforma de referencia.

### 10.1 Organización del repositorio (WS1)

| Hoy | Después |
| --- | --- |
| `CoreServices/WindowServer/` contiene el servidor y LoginWindow, SystemUIServer y LoadingWindow | El servidor nuevo en `PrivateFrameworks/SkyLight/` (el servidor se instala en `SkyLight.framework/Resources`, la biblioteca del cliente en el framework); el servidor actual en `CoreServices/WindowServer/Legacy/` hasta WS4 |
| LoginWindow, SystemUIServer y LoadingWindow dentro del directorio del servidor | Apps propias en `CoreServices/`: `loginwindow.app` y `SystemUIServer.app`, junto a `Dock.app` y el gestor de archivos; LoadingWindow pasa a formar parte de loginwindow |
| Árboles de libinput, libevdev y mtdev | Se borran en WS4 |

### 10.2 Mensajes que WindowServer reenvía hoy (WS4)

`message.h` lleva mensajes que no son trabajo de un servidor de ventanas;
los usan AppKit (`NSApplication.m`) y SystemUIServer (`AppDelegate.m`). Cada
uno pasa a donde lo tiene la plataforma de referencia:

| Hoy, a través de WindowServer | Después |
| --- | --- |
| La app envía su menú serializado a SystemUIServer (`CODE_MENU_FOR_APP`), que lo dibuja y devuelve los clics (`CODE_ITEM_CLICKED`) | AppKit de cada app dibuja su propia barra de menús y sus menús, en ventanas de nivel `kCGMainMenuWindowLevel` y de los niveles de menú, mientras es la app del frente |
| Status items enviados a SystemUIServer (`CODE_ADD_STATUS_ITEM`, `CODE_STATUS_ITEM_ADDED`) | Cada `NSStatusItem` es una ventana de su propia app, de nivel `kCGStatusWindowLevel`; WindowServer solo ordena la zona de estado. SystemUIServer aloja los Menu Extras del sistema |
| App lanzada o cerrada, reenviado al Dock y a SystemUIServer (`CODE_APP_LAUNCHED`, `CODE_APP_EXITED`) | LaunchServices y las notificaciones de `NSWorkspace` (`NSWorkspaceDidLaunchApplicationNotification`, `NSWorkspaceDidTerminateApplicationNotification`, `NSWorkspaceDidActivateApplicationNotification`) |
| Elementos recientes (`CODE_ADD_RECENT_ITEM`) | Las listas de archivos compartidas de LaunchServices, a través de `NSDocumentController` |

Las filas de menús, status items y elementos recientes se planifican en
detalle en [Menus.es.md](Menus.es.md).

WindowServer se queda solo con lo suyo: la app del frente, el orden de
activación, las ventanas key y main, y el lugar de la barra de menús en la
pantalla. Después de WS4 se eliminan los códigos de relevo de `message.h`, y
el propio `message.h` desaparece con el servidor anterior.

### 10.3 Ciclo de vida de la sesión (WS4, se completa en la Fase 5)

Hoy reiniciar, apagar y cerrar sesión son códigos de salida: SystemUIServer
termina con `EXIT_RESTART`, `EXIT_SHUTDOWN` o `EXIT_LOGOUT` (`common.h`,
`SystemUIServer/MenuView.m`), y WindowServer reacciona a ellos; apagar solo
registra «should shut down!».

Como en la plataforma de referencia, la ventana de login gestiona la sesión:

- launchd arranca `loginwindow` en la consola; muestra la ventana de login
  y, después del login, arranca la sesión del usuario (su dominio de launchd
  y sus `LaunchAgents`).
- Reiniciar, apagar y cerrar sesión son peticiones a `loginwindow` (la referencia las envía como eventos de scripting; aquí primero un mensaje Mach). Pide a cada app
  que termine (`applicationShouldTerminate:`), espera, cierra la sesión y
  después llama a `reboot(2)` con las opciones que corresponden.
- WindowServer no recibe más aviso que la desconexión de sus clientes.

La Fase 4 solo necesita el `loginwindow` mínimo de la sección 9 (fin de la
sesión y `reboot(2)`), marcado `// SHORTCUT(WS4):`; la ventana de login
completa es de la Fase 5.

El menú del sistema que envía estas peticiones se planifica en
[Menus.es.md](Menus.es.md) (MB0 y MB3).
