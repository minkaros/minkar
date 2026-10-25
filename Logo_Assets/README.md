# Minkar artwork

Official Minkar icons and logos (package version 1.0), all exported from
one master artwork.

| Directory | Contents |
|---|---|
| `Minkar/PNG/` | System icon, 16 to 1024 px, with alpha |
| `Minkar/macOS/Minkar.icns` | Native multi-resolution icon |
| `Minkar/MenuBar/` | Monochrome mark for the menu bar: black for light themes, white for dark themes; 18, 36, 64 and 128 px |
| `Minkar/Boot/` | Boot logo (black and white, 256/512/1024 px), the progress bar parts (track and fill) and 1920x1080 boot screens |
| `Minkar/Vector/` | SVG of the monochrome mark (not of the glossy icon) |
| `Minkar/Source/` | Original master artwork |

The boot screens show the intended look; the progress bar must follow the
actual boot progress. Rebuild an `.icns` on macOS with
`iconutil --convert icns --output Minkar.icns Minkar.iconset`.

Filer, System Preferences and the Installer use the system icon until they
get icons of their own.
