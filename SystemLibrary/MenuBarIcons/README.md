# Menu bar icons

The icons of the menu bar's status items (the volume, network and
displays extras). They are Lucide's icons (24 px grid, stroke 2), from
the lucide repository at commit `e00b2b26350fe4dfb6f5273aa366f94974430903`,
under the ISC License; the ones Lucide took from Feather (here,
`monitor`) are also under the MIT License, as `LICENSE` says. They are an
exception to the rule of no third-party artwork, as the Faience and
Faenza themes are (`../Icons/README.md`), and stay separate data files
under their license.

`pack.txt` lists each icon: the name it is installed under and the
Lucide icon it comes from. `SHA256SUMS` pins the SVGs in `svg/`
(unmodified); `fetch.sh` downloads them again, checks them and renders
`png/` (36 px, 18 points at 2x). Only the icons the menu bar uses are
here.

Installed in `/System/Library/MenuBarIcons/<name>.png`, black on clear:
the status items use them as template images, which the menu bar draws
in its own color. The SVGs, the license and this file go beside them.
