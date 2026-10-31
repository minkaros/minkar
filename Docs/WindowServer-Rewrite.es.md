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

### WS1 — Esqueleto e interfaz

- `WindowServer.defs` y el lado del cliente en `SkyLight.framework`; el
  servidor nuevo instalado en `SkyLight.framework/Resources`.
- El núcleo nuevo se compila junto al viejo; el viejo pasa a `Legacy/` y
  sigue siendo el predeterminado.
- Display Services y la lista de ventanas portados a MIG, con un cliente de
  prueba que llama a cada función y comprueba las respuestas.

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

### WS3 — Entrada

- Lector de IOHIDSystem, enrutamiento, repetición de teclas, agrupación de
  movimientos, captura del ratón durante el arrastre, ventana key y main,
  movimientos iniciados por el cliente.
- `CGEventTapCreate`, `CGEventTapEnable`, `CGEventPost` y
  `CGGetEventTapList` implementados.
- Pruebas: las teclas y clics inyectados por QMP llegan a la ventana
  correcta; un tap de solo escucha los ve; un tap que filtra puede descartar
  uno; `CGEventPost` llega a la ventana key.

### WS4 — Comportamiento de ventanas y cambio de servidor

- `CGWindowListCopyWindowInfo` con las claves de diccionario de la
  referencia, `CGWindowListCreateImage`, ventanas hijas, sheets, alfa,
  sombras por software, `NSCursor` y cursor rects, captura de pantalla
  mediante superficies.
- El arranque de la sesión sale de WindowServer.
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

Cada atajo queda marcado en el código con `// SHORTCUT(WSn):` y el hito que
lo elimina, para listarlos todos con `git grep`.
