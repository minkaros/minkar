# Symbol inventory

`symbol-inventory.py` lists every symbol the libraries and frameworks in a
Minkar sysroot export (re-exports followed, as dyld does), and checks which
of the symbols a set of macOS apps import from system libraries Minkar has.

    python3 -B tools/symbols/symbol-inventory.py \
        --sysroot ../build/sysroot --out ../build/symbols \
        --apps <app, binary or directory of apps> ...

It needs the toolchain's `llvm-nm` and `llvm-otool` (`--tools`, by default
the ones in `../build`). The apps are only parsed, never run; fat binaries
are read for x86_64.

The report (`--out`):

| File | Contents |
| --- | --- |
| `coverage.md` | per app and per library: imported symbols, how many Minkar exports, the most used missing ones; and the libraries whose install name dyld would not find in the sysroot |
| `coverage.json` | the same numbers, for scripts |
| `exports.tsv` | Minkar's inventory: library install name, symbol |
| `imports.tsv` | app, binary, library, symbol, weak |
| `missing.tsv` | library, symbol, the apps that import it |

What it compares, and what not:

- Only imports from system libraries (`/System/Library/...`, `/usr/lib/...`)
  count. Frameworks and dylibs an app carries in its bundle are its own, but
  their imports from the system count.
- An import matches the Minkar library with the same install name, or the
  one dyld would load from that path in the sysroot (symlinks followed).
  When only the short name matches, it is counted, and the table shows
  Minkar's install name: dyld would not load it under the name the app
  asks for.
- Objective-C classes are symbols (`_OBJC_CLASS_$_NSUUID`); methods are
  looked up at run time and are not covered.
- A symbol that exists but is a stub counts as present. Noisy stubs (plan,
  Phase 2) will let the report tell them apart.
