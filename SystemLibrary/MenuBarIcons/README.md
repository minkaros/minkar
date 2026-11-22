# Menu bar icons

The icons of the menu bar's status items (the volume, network and
displays extras). They are Google's Material Symbols, Outlined style,
weight 400, optical size 20 px (drawn for small sizes), from the material-design-icons repository at commit
`10ecc14790612ca8fd164e3f4ffe9c16fc50d5b7`, under the Apache License,
version 2.0 (`LICENSE`). They are an exception to the rule of no
third-party artwork, as the Faience and Faenza themes are
(`../Icons/README.md`), and stay separate data files under their license.

`pack.txt` lists each icon: the name it is installed under and the
symbol it comes from. `SHA256SUMS` pins the SVGs in `svg/` (unmodified);
`fetch.sh` downloads them again, checks them and renders `png/` (36 px,
18 points at 2x). Only the icons the menu bar uses are here.

Installed in `/System/Library/MenuBarIcons/<name>.png`, black on clear:
the status items use them as template images, which the menu bar draws
in its own color. The SVGs, the license and this file go beside them.
