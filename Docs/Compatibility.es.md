# Minkar — Matriz de compatibilidad

Generada por `bmake progress` desde [progress/compatibility.tsv](progress/compatibility.tsv), que escribe el gate BC (`bmake bc-check BC_UPDATE=yes`; carril BC en [ROADMAP.es.md](ROADMAP.es.md)). El binario publicado de cada app designada, descargado de su proyecto, se instala en la imagen como lo haría un usuario y se arranca; el resultado es hasta dónde llegó. La referencia es la versión en inglés, [Compatibility.md](Compatibility.md).

_Comprobado: 2026-11-20; ProductVersion 10.15; the gate took 19 minutes under KVM._

4 de 17 apps muestran una ventana.

## El conjunto 19 (versión mínima 10.15 o menor; BC4)

| App | Versión | Mínima | Arquitecturas | Resultado | Menús | Motivo |
| --- | --- | --- | --- | --- | --- | --- |
| Cyberduck | 9.6.0 | 10.13 | arm64, x86_64 | `at-start` | no | exit status 1, after its run loop started;  |
| Firefox | 160.0 | 10.15 | arm64, x86_64 | `window` | sí | installed after hdiutil convert (an lzma-compressed image (ULMO) is not attached yet); a window at 0,22, 1280x702 |
| HexFiend | 2.18.1 | 10.14.6 | arm64, x86_64 | `window` | sí | a window at 167,275, 367x442 |
| KeepingYouAwake | 1.6.8 | 10.13 | arm64, x86_64 | `at-start` | no | exit status 139; 2026-11-20 22:05:11.583 KeepingYouAwake[272:10fafe7c0] unexpected character 76 'v' at 7 |
| Platypus | 5.5.0 | 10.13 | arm64, x86_64 | `at-start` | no | exit status 134; 12  libdyld.dylib                       0x000000010720ba61 start + 1 |
| Skim | 1.7.19 | 10.13 | arm64, x86_64 | `window` | sí | a window at 334,104, 480x382 |
| Sloth | 3.6 | 10.13 | arm64, x86_64 | `at-start` | no | exit status 139; 2026-11-20 22:05:16.697 Sloth[327:10d49f7c0] Illegal NSOutlineView data source (<SlothController: 0x7fc715c6eba0>). Must implement outlineView:child:ofItem: |
| TextMate | 2.0.23 | 10.12 | arm64, x86_64 | `window` | sí | a window at 426,162, 427x422 |
| Thunderbird | 160.0 | 10.15 | arm64, x86_64 | `no-load` | no | installed after hdiutil convert (an lzma-compressed image (ULMO) is not attached yet); dlopen(/Applications/Thunderbird.app/Contents/MacOS/XUL, 265): Symbol not found: _kABModificationDateProperty Referenced from: /Applications/Thunderbird.app/Contents/MacOS/XUL  |
| VLC | 3.0.24 | 10.7.5 | arm64, x86_64 | `at-start` | no | exit status 1; cannot be run by non-trusted users first). |

## Las apps designadas siguientes (BC5 en adelante)

| App | Versión | Mínima | Arquitecturas | Resultado | Menús | Motivo |
| --- | --- | --- | --- | --- | --- | --- |
| IINA | 1.5.1 | 11 | arm64, x86_64 | `min-version` | no | requires 11, Minkar reports 10.15; run anyway: no-load: dyld: Library not loaded: /System/Library/PrivateFrameworks/PIP.framework/Versions/A/PIP Referenced from: /Applications/IINA.app/Contents/MacOS/IINA Reason: image not found  |
| iTerm2 | 3.7.8 | 13.0 | arm64, x86_64 | `min-version` | no | requires 13.0, Minkar reports 10.15; run anyway: signature: MinkarCodeIntegrity: refused /Applications/iTerm.app/Contents/MacOS/iTerm2: restricted entitlement keychain-access-groups outside the trust cache |
| KeePassXC | 2.8.0 | 12.0 | x86_64 | `min-version` | no | requires 12.0, Minkar reports 10.15; run anyway: signature: MinkarCodeIntegrity: refused /Applications/KeePassXC.app/Contents/MacOS/KeePassXC: restricted entitlement keychain-access-groups outside the trust cache |
| LibreOffice | 26.8.3 | 11.0.0 | x86_64 | `min-version` | no | requires 11.0.0, Minkar reports 10.15; run anyway: no-load: dyld: Library not loaded: /System/Library/Frameworks/Kerberos.framework/Versions/A/Kerberos Referenced from: /Applications/LibreOffice.app/Contents/Frameworks/libmergedlo.dylib Reason: image not found  |
| Transmission | 4.1.4 | 12.0 | arm64, x86_64 | `min-version` | no | requires 12.0, Minkar reports 10.15; run anyway: no-load: dyld: Library not loaded: /System/Library/Frameworks/QuickLookUI.framework/Versions/A/QuickLookUI Referenced from: /Applications/Transmission.app/Contents/MacOS/Transmission Reason: image not found  |
| UTM | 5.0.9 | 12.0 | arm64, x86_64 | `min-version` | no | requires 12.0, Minkar reports 10.15; run anyway: signature: MinkarCodeIntegrity: refused /Applications/UTM.app/Contents/MacOS/UTM: restricted entitlement com.apple.vm.device-access outside the trust cache |
| VSCodium | 1.143.17232 | 12.0 | x86_64 | `min-version` | no | requires 12.0, Minkar reports 10.15; run anyway: install: hdiutil attach: hdiutil: attach failed - no mountable file systems |

Resultados, del que menos llega al que más:

- `install`: no se pudo instalar el archivo descargado
- `signature`: el kernel rechazó su firma
- `no-load`: dyld no pudo cargarla (falta una biblioteca o un símbolo)
- `before-main`: terminó antes de que corrieran los inicializadores de sus bibliotecas
- `at-start`: terminó después, antes de mostrar una ventana
- `no-window`: corre, pero no mostró ninguna ventana
- `min-version`: su versión mínima es mayor que la que reporta Minkar y no mostró ventana (el motivo dice hasta dónde llegó)
- `not-run`: el gate no llegó a ella
- `window`: muestra una ventana
