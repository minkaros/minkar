# Herramientas de línea de órdenes

Cuántas de las órdenes que escribe un usuario del sistema de referencia tiene Minkar, y cuántas le faltan todavía. Esta página la genera `tools/check/commands-report.py --lang es` a partir de una sysroot compilada; la lista de referencia es `tools/check/reference-commands.txt` (366 órdenes de sus /bin, /sbin, /usr/bin y /usr/sbin, sin las herramientas de desarrollo ni los scripts de Perl y Python).

En inglés: [Commands.md](Commands.md).

Cada orden viene de FreeBSD main cuando FreeBSD la tiene, y se comporta como en el sistema de referencia cuando los dos difieren; cada una tiene su página de manual y sus pruebas.

## En la imagen del sistema

| Directorio | Órdenes |
|---|---:|
| `/bin` | 42 |
| `/sbin` | 47 |
| `/usr/bin` | 339 |
| `/usr/sbin` | 54 |
| **Total** | **482** |

## En el árbol de fuentes

Directorios de órdenes bajo `BSD/`, en el commit `60aa4ff4b1` (donde Minkar partió de ravynOS) y ahora:

| Directorio | Entonces | Ahora |
|---|---:|---:|
| `BSD/bin` | 30 | 35 |
| `BSD/sbin` | 4 | 35 |
| `BSD/usr.bin` | 55 | 192 |
| `BSD/usr.sbin` | 7 | 53 |
| **Total** | **96** | **315** |

## Frente al sistema de referencia

353 de las 366 órdenes de referencia están en la imagen; faltan 13.

| Tipo | En la imagen | Faltan | Qué hace falta |
|---|---:|---:|---|
| BSD y otras herramientas portables | 284 | 0 | Portadas de FreeBSD main (o de su propio proyecto), como las anteriores. |
| Las herramientas propias del sistema de referencia | 38 | 9 | Escritas para Minkar; muchas necesitan el sistema gráfico o frameworks de fases posteriores. |
| Componentes propios de Darwin | 15 | 0 | Conservados del código abierto original (launchd, configd, syslogd...). |
| Herramientas de sistemas de archivos | 6 | 4 | Necesitan antes el sistema de archivos (APFS, exFAT, FAT, NFS, SMB) en el kernel. |
| Licencia por aprobar | 5 | 0 | Su licencia (GPL, Vim, CDDL...) necesita antes la aprobación del proyecto. |
| Órdenes internas del shell como archivos | 5 | 0 | El sistema de referencia tiene /usr/bin/cd y similares: pequeños scripts que ejecutan la orden interna. |

### Faltan

- **Las herramientas propias del sistema de referencia** (9): `fdesctl`, `lsappinfo`, `mdfind`, `mdls`, `mdutil`, `opendiff`, `osascript`, `say`, `tmutil`
- **Herramientas de sistemas de archivos** (4): `fsck_apfs`, `mount_apfs`, `mount_smbfs`, `newfs_apfs`

