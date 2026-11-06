# Atlas icons

Atlas's system icons: folders (places), the Trash and generic documents.
They come from two icon themes by Matthieu James, both under the GNU
General Public License, version 3 (`COPYING`, `AUTHORS`):

- **Faience**, from `faience-icon-theme-master.zip`
  (SHA-256 `cbf7361be70e8b836c1f7ece211c562d2510519815eee555f3434ecbbc21a4a9`),
  with its variants Faience-Azur and Faience-Ocre;
- **Faenza**, for the icons Faience lacks, from faenza-icon-theme commit
  `f361c7ecef7286e8785544567ae8be6fa2a723ec`
  (tarball SHA-256 `5db75bbc9c790fbd1c8247c3598fc4408bdee933d8a4c20201136512e2c76544`).

They are an exception to the rule of no third-party artwork, and stay
under the GPL-3 as separate data files: the SVG sources are in `svg/`, the
rendered PNGs in `png/`. Third-party brand icons are left out.

`pack.txt` lists each icon: its group, the name it is installed under and
its source. `render.sh` copies the sources and renders the PNGs (256 px).

Installed in `/System/Library/Icons/<group>/<name>.png`:

| Group           | What                                       |
|-----------------|--------------------------------------------|
| `places`        | folders, blue: the default                 |
| `places-sand`   | the same folders, sand                     |
| `places-orange` | the same folders, orange                   |
| `system`        | the Trash (empty, full), desktop, document |
| `apps`          | the apps' icons (their bundles hold 512 px copies) |
