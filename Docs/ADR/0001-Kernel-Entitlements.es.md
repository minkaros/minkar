# ADR 0001: validación de entitlements en el kernel

Estado: aceptado, 2026-11-01. Desarrolla la sección 9.1 de
`Docs/WindowServer-Rewrite.es.md` (opción C, decidida el 2026-10-31). Hace
falta antes de WS4, cuando WindowServer deja de ejecutarse como root.

## Contexto

XNU lee los entitlements de un proceso solo si el proceso está marcado
`CS_SIGNED` (`cs_entitlements_dictionary_copy`, `cs_entitlements_blob_get`
en `bsd/kern/kern_cs.c`). Esa marca la pone el módulo de política de
integridad de código: `ubc_cs_blob_add` da al blob `CS_VALID`, llama a
`mac_vnode_check_signature`, y la política añade `CS_SIGNED` y, para el código
del sistema, `CS_PLATFORM_BINARY` a los flags que devuelve. Minkar no tiene
ese módulo, así que:

- ningún proceso es `CS_SIGNED` y ningún proceso tiene entitlements;
- las comprobaciones de entitlements de IOKit
  (`IOUserClient::copyClientEntitlement`, `IOTaskHasEntitlement`) fallan
  siempre; por eso root puede abrir la conexión de servidor de IOHIDSystem
  (`SHORTCUT(WS0)` en `IOHIDUserClient.cpp`);
- la mayoría de los binarios del sistema no están firmados: solo lo están los
  bundles de aplicación, ad hoc, con `minkar.app.mk` y
  `Developer/Default.xctoolchain/codesign/codesign.py`.

En x86_64 el código de trust cache de XNU es un stub (`trust_cache_init` y las
funciones `pmap_*trust_cache*` de `osfmk/x86_64/pmap.c`). El formato de
`osfmk/kern/trustcache.h` (versión 1: entradas ordenadas por CDHash, cada una
con tipo de hash y flags) sigue sirviendo.

## Decisión

### 1. Todos los binarios del sistema se firman

- `bsd.prog.mk` y `bsd.lib.mk` ganan la variable `ENTITLEMENTS` que ya tiene
  `minkar.app.mk`. Los ejecutables instalados se firman ad hoc con
  `codesign.py`, con el identificador `org.minkar.<nombre>` (el código en
  bundles conserva su `CFBundleIdentifier`) y sus entitlements, si los tiene.
- El código que no se construye con esas reglas (LLVM, sistemas de
  construcción de terceros) lo firma el paso de imagen de abajo, ad hoc y sin
  entitlements.

### 2. La construcción de la imagen escribe una trust cache estática

- Una herramienta nueva, `tools/mktrustcache.py`, se ejecuta en
  `Image/Makefile` antes de crear la imagen de disco. Recorre el sysroot,
  firma todo ejecutable y biblioteca Mach-O sin firma, lee cada
  CodeDirectory, calcula su CDHash (SHA-256 del CodeDirectory, primeros
  20 bytes) y escribe una trust cache de versión 1, ordenada y con un UUID
  nuevo.
- Se instala en `/System/Library/Security/TrustCache` (de root, modo 0644).
  La construcción de la imagen comprueba que todo fichero Mach-O del sysroot
  está en ella.

### 3. Un kext de política MAC marca los binarios del sistema

- Kext nuevo `Kernel/Extensions/MinkarCodeIntegrity`
  (`org.minkar.driver.MinkarCodeIntegrity`), código propio de sala limpia,
  BSD-2. Va en el kernelcache con `AppleSecurityExtension`, así que
  `bootstrapLoadSecurityExtensions` lo carga antes de que arranque BSD y se
  registra antes del primer `exec` (launchd). No se puede descargar.
  `KLDBootstrap::loadSecurityExtensions` (`libsa/bootstrap.cpp`) solo carga
  bundle IDs que empiezan por `com.apple.`; Minkar lo cambia para que acepte
  también `org.minkar.`, así el kext conserva un nombre de Minkar.
- Implementa solo `mpo_vnode_check_signature`:
  - CDHash en la trust cache: añade `CS_SIGNED | CS_PLATFORM_BINARY` (el
    kernel pone entonces `csb_platform_binary`).
  - Cualquier otra firma válida, ad hoc incluida: añade `CS_SIGNED`, sin
    `CS_PLATFORM_BINARY`.
  - Entitlements restringidos y no restringidos: ver la sección 4.
- La trust cache se lee una vez, en la primera llamada (para entonces el
  volumen raíz ya está montado), con `vnode_open` y `vn_rdwr`; se comprueba
  (versión 1, ordenada, tamaño acorde a `num_entries`) y se queda en memoria.
  La búsqueda es binaria. Si el fichero falta o está mal, el kext lo registra
  una vez y ningún binario es de plataforma; el sistema arranca igual.
- Se deja sitio para firmas con certificado verificadas con corecrypto
  (Fase 7): hasta entonces una firma con blob CMS se trata como ad hoc.

### 4. Entitlements restringidos

- No restringidos: los nombres que empiezan por `com.apple.security.`
  (sandbox, excepciones del hardened runtime, `get-task-allow`, acceso a
  ficheros, red y dispositivos). Cualquier firma válida puede llevarlos.
- Restringidos: todos los demás, como `com.apple.hid.system.server-access` y
  los privados. Solo los binarios de plataforma pueden llevarlos.
- Una firma fuera de la trust cache que lleva un entitlement restringido se
  rechaza, como en la plataforma de referencia: la política devuelve un error
  con motivo, el proceso no arranca y la consola nombra el entitlement
  rechazado. Así ningún lector de entitlements, del kernel ni del espacio de
  usuario (`SecTask` lee el blob en bruto con `csops`), puede verlo, y los
  lectores de XNU no cambian.

### 5. Pruebas

Un programa de prueba, `Tests/codesign/cstest`, lo ejecutan una prueba local
de imagen y un paso nuevo de boottest:

1. La copia que está en la trust cache informa `CS_SIGNED | CS_PLATFORM_BINARY`
   con `csops(CS_OPS_STATUS)` y, como usuario no root, abre la conexión de
   servidor de IOHIDSystem (entitlement restringido).
2. El mismo binario copiado y vuelto a firmar (CDHash nuevo, fuera de la
   cache) se rechaza en el `exec`.
3. Un binario firmado ad hoc con `com.apple.security.get-task-allow` lo
   conserva (`csops(CS_OPS_ENTITLEMENTS_BLOB)`).
4. Un binario sin firmar sigue ejecutándose y no es `CS_SIGNED`.

Cuando pasan las pruebas se elimina `SHORTCUT(WS0)` de `IOHIDUserClient.cpp`
y el job de WindowServer (WS4) lleva `com.apple.hid.system.server-access`.

## Alternativas consideradas

- Ejecutar ese binario y ocultar sus entitlements restringidos: requiere un
  cambio en XNU para que tanto el diccionario de entitlements como el blob en
  bruto que devuelve `csops` omitan los nombres restringidos en código que no
  es de plataforma.
- Un bundle ID `com.apple.` para el kext: sin cambiar el kernel, pero el
  código propio de Minkar llevaría un nombre ajeno.
- Entregar la trust cache con el cargador EFI, como datos de arranque: sin
  fichero en el volumen raíz, pero requiere cambiar el cargador y un lector
  de datos de arranque en el kernel. Posible más adelante, con un volumen de
  sistema sellado.

## Fuera de alcance

- Validación de bibliotecas, aplicación del hardened runtime y el sandbox.
- Proteger el fichero de la trust cache más allá de que sea de root (un
  volumen de sistema sellado es un tema posterior).
- Firmas con certificado y perfiles de aprovisionamiento (Fase 7).
