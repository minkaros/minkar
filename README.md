# Minkar

Minkar is an open source operating system that aims to provide a
macOS-like experience and compatibility with macOS applications on x86-64
systems (and eventually arm64). It builds on the foundations of Darwin and
FreeBSD, existing open source packages in the same space, and new code to
fill the gaps.

The main design goals are:

- Source compatibility with macOS applications: a Mac application can be
  compiled on Minkar and run. Binary compatibility later.
- Familiar GUI metaphors and UX (file manager, application launcher, a top
  menu bar that reflects the active application) built on the Apple APIs
  applications expect.
- macOS folder layouts (/Library, /System, /Users, /Volumes) and file
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
next is in [Docs/ROADMAP.md](Docs/ROADMAP.md); how many of macOS's
command-line tools Minkar has, in [Docs/Commands.md](Docs/Commands.md). Issues and discussion:
https://github.com/minkaros/minkar

## Credits

Minkar is a fork of [ravynOS](https://github.com/ravynsoft/ravynos).
Its contributors up to the fork are listed in
[CREDITS-ravynOS.txt](CREDITS-ravynOS.txt), and ravynOS's own license is
in [LICENSE-ravynOS](LICENSE-ravynOS). Minkar's contributors are listed in
[AUTHORS](AUTHORS).

## Trademarks

Apple, Mac, macOS and Darwin are trademarks of Apple Inc. Minkar is an
independent project, not affiliated with, sponsored by or endorsed by
Apple, the ravynOS project or The FreeBSD Foundation; their names appear
only to credit the code that comes from them and where compatibility
needs them. See [NOTICE](NOTICE).

## License

Code written for Minkar is under the BSD 2-Clause license
([LICENSE-minkar](LICENSE-minkar)). Each other file keeps the license it
came with; see [NOTICE](NOTICE) for the third-party components and their
licenses.
