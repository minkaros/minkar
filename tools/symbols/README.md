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
  looked up at run time and are not covered here: see *Selectors* below.
- A symbol that exists but is a stub counts as present. `trivial-functions.py`
  (below) and the noisy-stub log tell them apart.

## Selectors

`selector-inventory.py` covers what the symbol inventory cannot see: the
Objective-C messages. It lists, per app, the selectors its binaries send
(`__objc_selrefs`) that neither the app (any binary in its bundle, embedded
frameworks included, and the methods of the protocols it declares) nor
any Minkar class implements.

    python3 -B tools/symbols/selector-inventory.py --sysroot ../build/sysroot \
        --apps ../build/reference-apps/apps --out ../build/symbols

It reads the Objective-C metadata of each Mach-O file itself (class,
metaclass and category method lists, relative ones included, with chained
fixups): `llvm-otool -ov` leaves out the metaclass, so the class methods,
of exported classes. Output: `selectors.md` (per app: sent, by the app,
system, in Minkar, missing; the missing ones most used first),
`selectors-missing.tsv`, `selectors-used.tsv` and `selectors-minkar.tsv`
(every Minkar method by selector, `-[Class sel]` and `+[Class sel]`).

A selector counts as implemented when any class has a method by that
name, so a message that reaches the wrong class is not caught; a stub
counts as implemented.

## Silent stubs

`trivial-functions.py` lists the exported functions of a sysroot's
libraries and frameworks that do nothing: their code, without the frame
set-up, is only a return (of nothing, zero, a constant or the first
argument).

    python3 -B tools/symbols/trivial-functions.py --sysroot ../build/sysroot \
        --out ../build/symbols/trivial.tsv

Most are stubs that answer without doing the work; some are empty by design
(fork hooks, no-ops). The ones that are stubs become noisy stubs: they call
`MINKAR_STUB()` (C) or `NSUnimplementedMethod()` (Objective-C), from
`<minkar/stub.h>`, and their first call in a process is logged to syslogd
under the subsystem `org.minkar.stubs`:

    syslog | grep org.minkar.stubs

A stub answers as before; only the log line is new.

## Reference apps

`reference-apps.tsv` lists the reference apps of the macOS 10.15 stage
(plan, principle 6): open source Objective-C apps built for macOS 10.15
or earlier, with the URL and sha256 of each download. Swift apps join when
Minkar has a Swift runtime (plan, Phase 3).

    python3 -B tools/symbols/fetch-reference-apps.py --dest ../build/reference-apps
    python3 -B tools/symbols/symbol-inventory.py --sysroot ../build/sysroot \
        --apps ../build/reference-apps/apps --out ../build/symbols

`fetch-reference-apps.py` checks each download's sha256 and unpacks it
without trusting it (no absolute paths, no "..", no links out of the
app). A new version of an app is a new line with its sha256.

The CI makes this report after every build (`tools/ci/symbol-report.sh`):
the per-app table is in the run's summary and the whole report in its
`symbol-coverage` artifact.

