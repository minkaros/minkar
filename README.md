# Minkar

Minkar is an open source operating system that aims to run Cocoa
applications of the reference platform unmodified, with that platform's
conventions, on x86-64 systems (and eventually arm64). It builds on the foundations of Darwin and
FreeBSD, existing open source packages in the same space, and new code to
fill the gaps.

The main design goals are:

- Source compatibility with the reference platform's applications: a Cocoa
  application can be compiled on Minkar and run. Binary compatibility later.
- Familiar GUI metaphors and UX (file manager, application launcher, a top
  menu bar that reflects the active application) built on the Cocoa APIs
  applications expect.
- The reference platform's folder layouts (/Library, /System, /Users,
  /Volumes) and file
  systems (HFS+, APFS), plus other useful ones such as FAT and NTFS.
- Self-contained applications in app bundles, with support for DMG files
  and other archive formats.
- An up-to-date Unix environment with current tools and shells.
- Pleasant to use, secure, stable and fast.

## Building

See [Docs/BUILDING.md](Docs/BUILDING.md). The source map is in
[Docs/SourceMap.md](Docs/SourceMap.md).

## Contributing

See [Docs/CONTRIBUTING.md](Docs/CONTRIBUTING.md) and the
[Code of Conduct](Docs/CODE_OF_CONDUCT.md). What is done and what comes
next is in [Docs/ROADMAP.md](Docs/ROADMAP.md); how many of the reference
platform's command-line tools Minkar has, in [Docs/Commands.md](Docs/Commands.md), and
where each comes from, in [Docs/Userland.md](Docs/Userland.md); the
machines it is tested on, in [Docs/Hardware.md](Docs/Hardware.md). Issues and
discussion:
https://github.com/minkaros/minkar

## Credits

Minkar is a fork of [ravynOS](https://github.com/ravynsoft/ravynos).
Its contributors up to the fork are listed in
[CREDITS-ravynOS.txt](CREDITS-ravynOS.txt), and ravynOS's own license is
in [LICENSE-ravynOS](LICENSE-ravynOS). Minkar's contributors are listed in
[AUTHORS](AUTHORS).

## Trademarks

See [TRADEMARKS.md](TRADEMARKS.md).

## License

Code written for Minkar is under the BSD 2-Clause license
([LICENSE-minkar](LICENSE-minkar)). Each other file keeps the license it
came with; see [NOTICE](NOTICE) for the third-party components and their
licenses.
