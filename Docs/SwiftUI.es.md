# SwiftUI en Minkar

Estado: **propuesto**, 21 nov 2026. Hito SU1 del carril SU de [ROADMAP.es.md](ROADMAP.es.md);
la decisión es D7 (20 nov 2026). Todavía no se ha construido nada: SwiftCrossUI se
importa, y SU2 empieza, solo después de publicar Minkar 19.6.0. La referencia es la
versión en inglés, [SwiftUI.md](SwiftUI.md); esta copia se actualiza en el mismo commit.

## 1. Dos partes, y su frontera

| Parte | Qué es | De dónde sale | Licencia |
| --- | --- | --- | --- |
| Capa pública | `SwiftUI.framework`, módulo `SwiftUI`, compilado con library evolution | Escrita por Minkar; su superficie sale solo de los símbolos que importan las apps designadas y de la documentación pública | BSD-2-Clause |
| Motor | Layout, estado y dibujo sobre el AppKit y el Core Animation de Minkar | Adaptado de SwiftCrossUI (MIT) | MIT, conservada en cada archivo adaptado y listada en `NOTICE` |

- **Público:** cada símbolo que puede importar un binario publicado, y nada más: los
  tipos (`View`, `Text`, `VStack`, `ModifiedContent`, `EnvironmentValues`…), sus
  miembros, conformidades, descriptores y accesores de metadatos. La capa pública es su
  dueña, con los nombres y las disposiciones que esperan los binarios.
- **Privado:** el motor. Es un módulo aparte dentro del framework, enlazado
  estáticamente y no exportado, con un nombre de Minkar (por ejemplo `MinkarViewEngine`),
  nunca `SwiftUICore` ni el de otro framework de la plataforma de referencia, salvo que
  los binarios designados importen símbolos de ese framework.
- **Cómo llama la capa al motor:** cada tipo de vista público conforma, internamente,
  los protocolos del motor (su layout y la creación de su widget), y el `App`, la
  `Scene` y la `Window` públicos dirigen el grafo de vistas del motor. SwiftCrossUI pone
  su código de layout dentro de sus propias vistas públicas y lo exige en su protocolo
  `View` público (revisión de SwiftCrossUI, sección 3.2), así que la adaptación lleva
  ese código detrás de protocolos internos; el motor no aparece nunca en una firma
  pública.

## 2. De dónde sale cada símbolo público

1. **Lo que importan los binarios designados** (SU0, `tools/symbols/symbol-inventory.py`):
   el nombre manglado de cada símbolo lleva su firma completa de tipos (módulo,
   anidamiento, parámetros genéricos y restricciones, etiquetas y tipos de los
   argumentos, resultado), y su clase (descriptor de tipo nominal, accesor de
   metadatos, descriptor de método o propiedad, tabla de testigos de protocolo,
   descriptor de conformidad). El inventario los desmangla con el `swift-demangle` del
   host de compilación y los lista por app y por framework.
2. **La documentación pública** para el comportamiento: qué hace un modificador, los
   valores por defecto, las reglas de layout. Sus declaraciones pueden leerse como
   documentación.
3. **Nunca** el SDK del fabricante, sus ficheros `.swiftinterface`, sus headers ni sus
   binarios. `APITool/` y `_SwiftCrossUIPortingKit/Generated/Enums.swift` de
   SwiftCrossUI se generan a partir de la interfaz del SDK del fabricante (revisión,
   sección 2.3), y ni se importan ni se leen.

## 3. ABI

- **Módulo `SwiftUI`, con library evolution** (`-enable-library-evolution`), instalado
  como `/System/Library/Frameworks/SwiftUI.framework/Versions/A/SwiftUI`, el install
  name que cargan los binarios (SU0: las tres apps ligan ahí todos sus símbolos de
  SwiftUI).
- **Lo que muestran los imports:** nombres, firmas y clases de los símbolos, con sus
  firmas genéricas y conformidades; los requisitos de los protocolos resilientes, a
  través de sus descriptores de método y de base (así el orden de los requisitos se
  resuelve por símbolo, no por desplazamiento); descriptores de propiedad; accesores de
  metadatos de tipo.
- **Lo que no muestran:** la disposición de los tipos que el cliente trata como
  resilientes (pasa por las tablas de testigos de valor, así que la elige Minkar), y la
  de los tipos `@frozen` y del código `@inlinable` compilado dentro del cliente, cuya
  disposición el cliente lleva fija.
- **Cómo se comprueban sin salir de la sala limpia:** ejecutando los binarios publicados
  (SU4) y observando dónde fallan, y con la documentación cuando indica una
  representación; cada discrepancia encontrada así pasa a ser un caso de prueba del gate
  de código fuente (SU2). El código máquina de los binarios no se desensambla: el código
  `@inlinable` compilado dentro de ellos es del propio fabricante (pregunta abierta 6).

## 4. Cómo entra SwiftCrossUI

- **Versión:** la revisada (SU0b) es v0.11.0 (`f673fcd520`, 16 nov 2026); SU2 toma la
  última versión revisada en ese momento, fijada por su SHA-256 a través del manifiesto
  de `minkar.extlib.mk` (url, sha256, versión), con los cambios de Minkar en un
  directorio `patches/`. Su árbol nunca se copia a mano.
- **Se toma** (revisión, sección 6): el layout (`Layout/`, unas 1.000 líneas), el código
  de layout de las pilas y de los modificadores de layout, el grafo de vistas y los
  internos del estado (unas 1.600 líneas), y el backend de AppKit (unas 4.200 líneas),
  adaptado: unas 10.000 líneas en total.
- **No se toma:** su API pública (builders, los `TupleView1`…`TupleView20` generados,
  escenas, `App`), su propio `ObservableObject`/`Published`/`Publisher` (Minkar tiene
  Observation y OpenCombine), el kit de portado ni `APITool/`, los demás backends. Sus
  dependencias se sustituyen por las de Minkar (ImageIO, `Synchronization.Mutex`,
  Observation, OpenCombine).
- **Lo que el AppKit de Minkar necesita primero** (revisión, sección 4.2):
  `CAGradientLayer`, `+[NSColor colorWithName:dynamicProvider:]`,
  `NSImageView.contentTintColor`, las máscaras del anillo de foco en `NSView`, el
  `NSDatePickerCellDelegate` formal, las constantes actuales del selector de fecha, la
  notificación de cambio de apariencia, y los nombres de Swift de la API de AppKit,
  añadidos al SDK (apinotes o `NS_SWIFT_NAME`, a partir de la documentación pública;
  decidido el 21 nov 2026).
- **Hacia el origen:** las correcciones que no son específicas de Minkar se envían a
  SwiftCrossUI.

## 5. Primer objetivo y orden

Según SU0 (20 nov 2026): 3 de las 17 apps designadas enlazan SwiftUI, todas desde
`SwiftUI.framework`; entre las tres importan 1.022 símbolos distintos de SwiftUI, 283 de
ellos compartidos por más de una app.

| App | Símbolos de SwiftUI | Versión mínima | Necesita además |
| --- | ---: | --- | --- |
| iTerm2 | 98 | 13.0 | Charts (fuerte), FoundationModels (débil) |
| IINA | 310 | 11 | un framework privado (PIP) |
| UTM | 971 | 12.0 | Virtualization, vmnet, AppIntents |

1. **iTerm2 primero:** el menor número de símbolos. Sus 98 son sobre todo el núcleo
   común (descriptores de vistas, pilas, marcos, márgenes, valores de entorno, texto y
   cadenas localizadas).
2. **Los 283 símbolos compartidos** después, que acercan a IINA y a UTM.
3. **IINA** y luego **UTM**, cada una con lo demás que enlaza.

Cada paso se mide con la cobertura de SU3: la parte de los símbolos de SwiftUI que
importa la app que Minkar exporta con el mismo nombre y la misma clase.

## 6. Pruebas

- **SU2, gate de código fuente:** apps de prueba SwiftUI pequeñas, propias de Minkar (una
  ventana con pilas y texto; estado y bindings; listas; controles; modificadores),
  compiladas con el Swift del árbol contra `SwiftUI.framework`, ejecutadas en la imagen y
  comprobadas por sus ventanas (su contenido comparado con capturas de referencia hechas
  en Minkar, como hace el gate M4).
- **SU3:** `symbol-inventory.py` informa de la cobertura de SwiftUI de cada app
  designada; el 100 % para la app es el criterio de SU3.
- **SU4:** la app entra en el gate BC: su binario publicado llega a `window`. Necesita la
  subida de `ProductVersion` que la app exija (iTerm2: 13.0).

## 7. Preguntas abiertas

1. El nombre del módulo del motor (`MinkarViewEngine` es provisional).
2. Si las apps de prueba de SwiftUI van en `Tests/` (como Gallery) o en
   `Developer/Templates`.
3. Si la cobertura de SU3 cuenta un stub ruidoso como presente (como hace la cobertura
   de Cocoa) o solo una implementación que funcione.
4. iTerm2 necesita también Charts (enlace fuerte): si Charts tiene su propia entrada en
   el carril o llega con SU4.
5. Si se pide de todos modos a SwiftCrossUI una declaración de procedencia, para que
   conste (la decisión 11 dio la revisión por suficiente).
6. Si la disposición de los tipos `@frozen` puede deducirse de los binarios de las apps
   designadas sin leer código del fabricante incrustado en ellos; si no, esos tipos
   siguen la documentación y los fallos que muestre SU4.
