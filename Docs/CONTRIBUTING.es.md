# Contribuir a Minkar

Cómo ayudar a Minkar: por dónde empezar, las reglas del código y de la documentación, y
cómo llega el trabajo a `devel`. Para quien va a enviar su primer cambio. Última
revisión: 21 nov 2026. Es una copia de [CONTRIBUTING.md](CONTRIBUTING.md) y se actualiza
en el mismo commit.

## Por dónde empezar

1. **Elige una app que todavía no funcione.** [Compatibility.es.md](Compatibility.es.md)
   lista cada app designada, hasta dónde llega su binario publicado (`no-load`,
   `at-start`, `no-window`…) y el motivo, a menudo la biblioteca o el símbolo que falta.
   Ese motivo es la tarea.
2. **Lee el registro de stubs de un framework.** Cada método o función sin implementar
   registra su primera llamada en el subsistema `org.minkar.stubs`. Ejecuta la app en la
   imagen y después `log show --predicate 'subsystem == "org.minkar.stubs"'`: las
   llamadas que salen son lo que la app necesita, en el orden en que lo usó.
3. **Pasa las pruebas antes de proponer un cambio.** Compila la etapa que tocaste
   (`bmake ONLY_STAGE=<etapa> <etapa>`), después `bmake layers-check`, el gate M2
   (`boottest GATE=M2`) para cualquier cosa del sistema base, el gate M4 para el servidor
   de ventanas o AppKit, el gate BC (`bmake bc-check`) para un cambio pensado para una
   app, y `bmake progress` sin diferencias. [BUILDING.es.md](BUILDING.es.md) explica cómo.

Otras formas de ayudar: probar en tu hardware ([Hardware.md](Hardware.md)), informar de
fallos y mejorar la documentación.

## Código de conducta

Esperamos que todo el que participe en el proyecto siga el
[Contributor Covenant](CODE_OF_CONDUCT.md).

## Qué hace falta

El trabajo pendiente está en [ROADMAP.es.md](ROADMAP.es.md) y en las
[issues abiertas](https://github.com/minkaros/minkar/issues). Lo que más se necesita: C y
C++, Objective-C, Swift, ensamblador x86-64, el toolchain y el enlazador, programación del
kernel Darwin y de kexts, y gráficos.

## Comunidad

Únete a las [discusiones](https://github.com/minkaros/minkar/discussions) para conocer a
otros usuarios y desarrolladores.

## Informar de problemas

Si has encontrado un problema de seguridad, _no_ abras una issue en GitHub: escribe a
minkaros@outlook.com.

Para todo lo demás, mira primero las [issues abiertas](https://github.com/minkaros/minkar/issues)
por si ya existe. Si existe, puedes añadir un comentario con más información o un «+1»
para subir su prioridad. Si no, abre una issue nueva con todo el detalle que puedas sobre
cómo reproducir el problema.

## Reglas básicas

No envíes preguntas generales ni peticiones de ayuda al gestor de issues: para eso están
las [discusiones](https://github.com/minkaros/minkar/discussions).

Sigue el código de conducta.

El código nuevo va bajo la licencia BSD de 2 cláusulas.

Si _alguna vez_ has visto el código propietario de la plataforma de referencia (no el que
tiene licencia APSL u otra libre), *no* puedes contribuir código a Minkar. Lo sentimos.
Aun así, puedes ayudar de otras formas.

## Idioma

Todo lo que se sube está en inglés: código, comentarios, mensajes de commit y textos de la
interfaz. La interfaz se traduce con paquetes `*.lproj` (el inglés es el idioma de
desarrollo y la base de las pruebas y las capturas); una traducción nunca sustituye los
textos en inglés del código.

La documentación se escribe en inglés. Solo se traducen los
documentos que llevan a alguien al proyecto: `README.md`, `Docs/ROADMAP.md`,
`Docs/BUILDING.md`, `Docs/CONTRIBUTING.md` y `Docs/Compatibility.md`, cada uno con una
copia en español al lado como `<nombre>.es.md`. Una traducción es una copia del documento
en inglés y se actualiza en el mismo commit.

## Ramas

El trabajo va a `devel`. `devel` pasa a `main` cuando se cierra un hito o, como muy tarde,
una vez por semana, y solo por avance directo (`git push origin devel:main`, sin commits
de fusión), para que la CI, que corre en `main`, compruebe lotes pequeños.

## Atajos de desarrollo

Una solución temporal tomada durante el desarrollo se marca en el código con el hito que
la retira:

    // SHORTCUT(<removed by>): <what and why>; taken in <milestone>.

Por ejemplo `SHORTCUT(WS4a)`, `SHORTCUT(Phase 5)` o `SHORTCUT(D3)`. Cada atajo tiene
además una fila en la tabla de atajos de su documento
([WindowServer.md](WindowServer.md#development-shortcuts-still-open), secciones 3.5 y 4.5
de [Atlas.md](Atlas.md)). Un hito solo se cierra cuando
`git grep -n 'SHORTCUT(<hito>)'` no encuentra nada.

## Páginas generadas

`bmake progress` escribe todas las partes generadas de la documentación; solo se genera
lo que hay entre los marcadores de cada página:

- el resumen y los SVG de [Progress.md](Progress.md), y el estado del README, desde
  [progress/status.toml](progress/status.toml);
- la matriz y la tabla de frameworks de [Compatibility.es.md](Compatibility.es.md), desde
  `progress/compatibility.tsv` y `progress/frameworks.tsv`;
- las tablas de órdenes de [Userland.md](Userland.md), desde `progress/commands.tsv`.

El commit que cierra un hito actualiza también `status.toml` y ejecuta `bmake progress`;
el tablero de trabajo conserva los seis elementos hechos más recientes. La CI falla si las
páginas subidas no son lo que escribe `bmake progress`.

`commit` y `updated` en `status.toml` nombran el árbol que resumen las páginas: el último
commit fuera de `Docs/` y su fecha. `bmake progress` avisa si `commit` es más antiguo.

## Escribir documentación

Para que la documentación siga siendo corta y esté al día:

- Un documento describe cómo es algo, o qué falta. No cuenta cómo se llegó ahí: eso está
  en los commits.
- Cada documento empieza con una o dos frases que dicen qué es y para quién, y la fecha de
  su última revisión.
- Un hito es una línea: su nombre, su estado y, si está hecho, su commit; como mucho 200
  caracteres, sin historia.
- Ningún párrafo pasa de 600 caracteres en los documentos para recién llegados (README, el
  índice, ROADMAP, CONTRIBUTING, el inicio rápido de BUILDING).
- Listas para las enumeraciones; prosa corta para explicar un porqué.
- Lo que ya muestra una página generada (Progress.md, Compatibility.md) no se repite a
  mano en otra.
- Cómo funciona algo hoy va al documento de esa parte; una decisión va a un ADR si es de
  arquitectura y, si no, a una línea de [Decisions.md](Decisions.md).
- Se escribe «API N», «la plataforma de referencia», «PC 1» y «PC 2»; sin nombres
  comerciales de la plataforma de referencia.
- [README.md](README.md) lista todos los documentos; uno nuevo añade allí su línea.

¡Que te diviertas y hagas grandes cosas!
