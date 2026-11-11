# Shiira: análisis para Minkar

Estado: análisis, 11 de noviembre de 2026. Aún no se ha importado nada.
Versión en inglés: [Shiira.md](Shiira.md).

Shiira es un navegador web para la API Cocoa, escrito en Objective-C
sobre WebKit por The Shiira Project (Japón, 2004–2009). Este documento
dice qué haría falta para traerlo a Minkar, modernizado, como navegador
propio de Atlas. Firefox sigue siendo la app de referencia para la
cobertura y el navegador de Minkar 19.6.0 (Docs/Atlas.md). Shiira no lo
sustituye.

## 1. Qué hay

| | |
|---|---|
| Última versión | 2.3 (build 090811), agosto de 2009; el proyecto se detuvo ahí |
| Último código publicado | 2.2 (`Shiira2.2src.zip`, espejo de OSDN `ftp.iij.ad.jp/pub/osdn.jp/shiira/26462/`, SHA-256 `531211b9…cd243`) |
| Código de la 2.3 | no publicado como archivo; puede estar en el repositorio del proyecto (preguntar al dueño) |
| Tamaño | 84 000 líneas de Objective-C: Shiira 58 600, HMDTAppKit 21 300, HMDTWebKit 2200, HMDTFoundation 1800 (206 ficheros `.m`) |
| Licencia | BSD de dos cláusulas («Redistribution and use…») en 405 de los 414 ficheros; 9 solo dicen «All rights reserved» |
| Binarios | solo i386 y PowerPC; Minkar solo ejecuta código x86_64, así que hay que compilarlo desde el código |
| Proyecto | Xcode 2.4 (`objectVersion 42`), SDK 10.4u |
| Interfaz | 86 NIB en inglés (17 ventanas), 12 idiomas; `MainMenu.nib` usa el formato antiguo typedstream (`NSIBObjectData`), el resto son archivos con claves |

## 2. Qué usa

Frameworks que enlaza la 2.3: Cocoa, WebKit, CoreData, QuartzCore,
Quartz, OpenGL, Security, Carbon, CoreServices, ApplicationServices,
ExceptionHandling, y dos frameworks de terceros incluidos ya compilados
para i386/PPC: Sparkle (actualizaciones) y Growl (notificaciones).

De los 564 nombres Cocoa que usa el código (clases, constantes,
protocolos), 127 no están hoy en los headers del SDK de Minkar:

- **WebKit y DOM (unos 100):** todavía no existen `WebView`, `WebHistory`,
  `WebArchive`, `WebBackForwardList`, `WebResource` ni las clases DOM
  (`DOMHTMLInputElement`, `DOMHTMLFormElement`, `DOMHTMLAnchorElement`…).
  El WebKit.framework de Minkar (sobre WPE) está previsto después de
  Auto Layout (Docs/ROADMAP.md, puntos W).
- **API privada de WebKit:** `WebIconDatabase` (25 usos), `WebInspector`
  (12, con su propia copia del header privado), `WebHTMLView`,
  `_webFrameForDOMDocument`. El WebKit actual las quitó o las oculta: hay
  que sustituirlas, no ofrecerlas.
- **Transiciones de Core Image:** `CICopyMachineTransition`,
  `CIPageCurlTransition` (efectos de pestañas): decorativas, se pueden
  quitar.
- **AppKit:** algunas constantes antiguas
  (`NSUnifiedTitleAndToolbarWindowMask`, nombres de partes del scroller) y
  categorías propias de Shiira.

Llamadas de Carbon y otras de C: `SetSystemUIMode` (pantalla completa,
5 usos), `SecKeychain*InternetPassword` (contraseñas guardadas),
`FSPathMakeRef`, `LSSetDefaultHandlerForURLScheme` y similares
(navegador predeterminado). API Cocoa antigua de la que depende:
`NSURLDownload` (63 usos, descargas), `NSCalendarDate` (18),
`NSArchiver`/`NSUnarchiver`, `NSAnimation`, bindings con controladores de
listas y árboles, CoreData (marcadores, historial, RSS).

## 3. Qué significa modernizarlo

1. **Compilación.** Sustituir los proyectos de Xcode 2.4 por la
   compilación de Minkar (como las apps de referencia: CMake y Ninja, o
   bmake), x86_64, clang 21. ARC es opcional: con retain/release manual
   compila tal cual.
2. **64 bits.** `int`/`unsigned` donde la API ahora usa `NSInteger` y
   `NSUInteger`, `float` donde usa `CGFloat`; cadenas de formato.
3. **Fuera:**
   - Sparkle: Minkar se actualiza a sí mismo.
   - Growl: pasa a `NSUserNotification`.
   - ExceptionHandling y `atos`: pasan a los informes de fallos del sistema.
   - Carbon: `SetSystemUIMode` pasa a las opciones de presentación y `FSPathMakeRef` a `NSURL`.
   - Las transiciones de Core Image.
4. **Interfaz.** `MainMenu.nib` a NIB con claves con el ibtool de Minkar
   (o probarlo vía `NSIBObjectData`, que AppKit ya tiene); iconos de Atlas
   en lugar de los de Shiira; primero inglés y español (los idiomas de
   Atlas).
5. **Motor web, que es la decisión principal:**
   - **A. Mantener la API antigua `WebView`.** El WebKit.framework de
     Minkar necesitaría `WebView` y además las clases DOM como objetos
     Objective-C sobre WPE, que no tiene esa API: una capa grande que solo
     usarían Shiira y apps antiguas.
   - **B. Pasar Shiira a `WKWebView` (recomendado).** El WebKit.framework
     de Minkar está pensado en torno a `WKWebView`, que es el modelo de
     WPE (varios procesos, llamadas asíncronas). Cada pieza tiene sustituto:
     - el uso del DOM (rellenar formularios, enlaces, imágenes) pasa a
       `evaluateJavaScript:`;
     - `WebIconDatabase` pasa al enlace de icono de la página;
     - `WebInspector` pasa al inspector de WPE;
     - `NSURLDownload` pasa a `WKDownload`/`NSURLSession`.

     Toca sobre todo `SRWebView`, los controladores de la ventana del
     navegador y el código de formularios y contraseñas.
6. **Atlas.**
   - Navegador predeterminado vía LaunchServices.
   - Las descargas se muestran en Files (`LSRevealInFiles`).
   - Las contraseñas van al subconjunto de Security de Minkar.
   - Sus preferencias junto a las de las demás apps.
   - Capturas de pantalla para revisión.

## 4. Nombre y logo

Regla de Minkar: solo Minkar y Atlas son nombres propios; los demás
componentes llevan nombre genérico. Con permiso del dueño, el maintainer
puede hacer de Shiira una excepción:

- **Con permiso** (por escrito, de The Shiira Project o de su autor, que
  cubra nombre y logo): la app se queda con «Shiira» y su icono, y el
  permiso se anota en `TRADEMARKS.md`.
- **Sin permiso:** nombre genérico («Navegador web»,
  `org.minkar.WebBrowser`) e icono de Atlas, conservando los créditos y el
  aviso BSD en la ventana «Acerca de» y en `/usr/share/licenses/shiira`.

Conviene pedir al dueño al mismo tiempo:
- el código de la 2.3 (o el repositorio);
- la licencia de los 9 ficheros sin el texto BSD.

## 5. Esfuerzo y orden

| Fase | Trabajo | Depende de | Estimación |
|---|---|---|---|
| S0 | Importar el código 2.2 (o 2.3) fijado por SHA-256, ficheros de compilación, compilación x86_64 que termine | — | 1 semana |
| S1 | 64 bits, quitar Carbon/Sparkle/Growl, NIB, arranca en Minkar sin contenido web | S0 | 1–2 semanas |
| S2 | `WKWebView` (opción B): páginas, pestañas, historial, marcadores, descargas | WebKit.framework (puntos W) | 3–5 semanas |
| S3 | Integración con Atlas, inglés/español, pruebas en la imagen, capturas | S2 | 1–2 semanas |

S0 y S1 pueden empezar ya. Además ponen a prueba AppKit (bindings,
CoreData, NIB antiguos, barras de herramientas, hojas), como las apps de
referencia. S2 espera a WebKit.framework. Al ser una app Cocoa, Shiira
prueba el WebKit.framework propio de Minkar, cosa que Firefox (con su
propio motor y su propio toolkit) no hace.

## 6. Recomendación

- Seguir con S0–S1 como línea de segunda prioridad, después del trabajo en curso (TextMate, Ruby/Python/Perl, W5).
- Elegir la opción B para el motor.
- Shiira sería el navegador ligero propio de Atlas para 19.6.x o 20.0; Firefox sigue siendo el navegador de 19.6.0.

## Fuentes

- Ficheros de Shiira: espejo de OSDN, `https://ftp.iij.ad.jp/pub/osdn.jp/shiira/`
- https://en.wikipedia.org/wiki/Shiira
- https://osdn.net/projects/shiira/
