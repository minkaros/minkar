# SwiftUI in Minkar

Status: **proposed**, 2026-11-21. Milestone SU1 of lane SU in [ROADMAP.md](ROADMAP.md);
the decision is D7 (2026-11-20). Nothing of it is built yet: SwiftCrossUI is imported,
and SU2 starts, only after Minkar 19.6.0 ships.

## 1. Two parts, and their boundary

| Part | What it is | Where it comes from | License |
| --- | --- | --- | --- |
| Public layer | `SwiftUI.framework`, module `SwiftUI`, built with library evolution | Written by Minkar, its surface from the designated apps' imported symbols and the public documentation only | BSD-2-Clause |
| Engine | Layout, state and drawing on Minkar's AppKit and Core Animation | Adapted from SwiftCrossUI (MIT) | MIT, kept in every adapted file, listed in `NOTICE` |

- **Public:** every symbol a published binary can import, and nothing else: the types
  (`View`, `Text`, `VStack`, `ModifiedContent`, `EnvironmentValues`…), their members,
  conformances, descriptors and metadata accessors. The public layer owns them, with
  the names and layouts the binaries expect.
- **Private:** the engine. It is a separate module inside the framework, linked
  statically and not exported, named for Minkar (for example `MinkarViewEngine`), never
  `SwiftUICore` or after another framework of the reference platform unless the
  designated binaries import symbols from such a framework.
- **How the layer calls the engine:** each public view type conforms, internally, to
  the engine's protocols (its layout and its widget creation), and the public `App`,
  `Scene` and `Window` drive the engine's view graph. SwiftCrossUI puts its layout code
  inside its own public view structs and requires it in its public `View` protocol
  (SwiftCrossUI review, section 3.2), so the adaptation moves that code behind
  internal protocols; the engine never appears in a public signature.

## 2. Where each public symbol comes from

1. **The designated binaries' imports** (SU0, `tools/symbols/symbol-inventory.py`):
   each symbol's mangled name carries its full type signature (module, nesting, generic
   parameters and constraints, argument labels and types, result), and its kind
   (nominal type descriptor, metadata accessor, method or property descriptor, protocol
   witness table, conformance descriptor). The inventory demangles them with the build
   host's `swift-demangle` and lists them per app and framework.
2. **The public documentation** for behavior: what a modifier does, defaults, layout
   rules. Its declarations may be read as documentation.
3. **Never** the vendor SDK, its `.swiftinterface` files, its headers or its binaries.
   SwiftCrossUI's `APITool/` and `_SwiftCrossUIPortingKit/Generated/Enums.swift` are made
   from the vendor SDK's interface (review, section 2.3) and are not imported or read.

## 3. ABI

- **Module `SwiftUI`, library evolution on** (`-enable-library-evolution`), installed as
  `/System/Library/Frameworks/SwiftUI.framework/Versions/A/SwiftUI`, the install name
  the binaries load (SU0: all three apps bind every SwiftUI symbol there).
- **What the imports show:** names, signatures and kinds of symbols, including generic
  signatures and conformances; resilient protocols' requirements through their method
  and base descriptors (so the order of requirements is resolved by symbol, not offset);
  property descriptors; type metadata accessors.
- **What they do not show:** the layout of types the client treats as resilient (it
  goes through value witness tables, so Minkar chooses it), and of `@frozen` types and
  `@inlinable` code compiled into the client, whose layout the client hard-codes.
- **How those are checked inside the clean room:** by running the published binaries
  (SU4) and observing where they fail, and from the documentation where it states a
  representation; a mismatch found this way becomes a test case in the source gate
  (SU2). The binaries' machine code is not disassembled: the `@inlinable` code compiled
  into them is the vendor's own (open question 6).

## 4. How SwiftCrossUI comes in

- **Version:** v0.11.0 (`f673fcd520`, 2026-11-16) is the version reviewed (SU0b); SU2
  takes the latest reviewed release then, pinned by its SHA-256 through
  `minkar.extlib.mk`'s manifest (url, sha256, version), with Minkar's changes as a
  `patches/` directory. Its tree is never copied by hand.
- **Taken** (review, section 6): the layout (`Layout/`, about 1,000 lines), the stacks'
  and layout modifiers' layout code, the view graph and the state internals (about
  1,600 lines), and the AppKit backend (about 4,200 lines), adapted: about 10,000 lines
  in all.
- **Not taken:** its public API (builders, generated `TupleView1`…`TupleView20`, scenes,
  `App`), its own `ObservableObject`/`Published`/`Publisher` (Minkar has Observation and
  OpenCombine), the porting kit and `APITool/`, the other backends. Its dependencies are
  replaced by Minkar's own (ImageIO, `Synchronization.Mutex`, Observation, OpenCombine).
- **What Minkar's AppKit needs first** (review, section 4.2): `CAGradientLayer`,
  `+[NSColor colorWithName:dynamicProvider:]`, `NSImageView.contentTintColor`, focus
  ring masks on `NSView`, the formal `NSDatePickerCellDelegate`, the current date picker
  constants, the appearance change notification, and the Swift names of AppKit's API,
  added to the SDK (apinotes or `NS_SWIFT_NAME`, from the public documentation; decided
  2026-11-21).
- **Upstream:** fixes that are not specific to Minkar go to SwiftCrossUI.

## 5. First target and order

From SU0 (2026-11-20): 3 of the 17 designated apps link SwiftUI, all from
`SwiftUI.framework`; together they import 1,022 distinct SwiftUI symbols, 283 of them
shared by more than one app.

| App | SwiftUI symbols | Minimum version | Also needs |
| --- | ---: | --- | --- |
| iTerm2 | 98 | 13.0 | Charts (strong), FoundationModels (weak) |
| IINA | 310 | 11 | a private framework (PIP) |
| UTM | 971 | 12.0 | Virtualization, vmnet, AppIntents |

1. **iTerm2 first:** the fewest symbols. Its 98 are mostly the shared core (view
   descriptors, stacks, frames, padding, environment values, text and localized
   strings).
2. **The 283 shared symbols** next, which bring IINA and UTM closer.
3. **IINA**, then **UTM**, each with what else it links.

Each step is measured by SU3's coverage: the share of the app's imported SwiftUI symbols
Minkar exports with the same name and kind.

## 6. Tests

- **SU2, source gate:** Minkar's own small SwiftUI test apps (a window with stacks and
  text; state and bindings; lists; controls; modifiers), built with the tree's Swift
  against `SwiftUI.framework`, run in the image and checked by their windows (contents
  compared with reference screenshots taken from Minkar, as gate M4 does).
- **SU3:** `symbol-inventory.py` reports each designated app's SwiftUI coverage; 100 %
  for the app is SU3's criterion.
- **SU4:** the app joins gate BC: its published binary reaches `window`. It needs the
  `ProductVersion` rise the app requires (iTerm2: 13.0).

## 7. Open questions

1. The engine module's name (`MinkarViewEngine` is a placeholder).
2. Whether the SwiftUI test apps live in `Tests/` (like Gallery) or in `Developer/Templates`.
3. Whether SU3's coverage counts a noisy stub as present (as the Cocoa coverage does),
   or only a working implementation.
4. iTerm2 also needs Charts (a strong link): whether Charts gets its own lane entry or
   comes with SU4.
5. Whether to ask SwiftCrossUI for a provenance statement anyway, for the record
   (decision 11 found the review sufficient).
6. Whether the layout of `@frozen` types can be learned from the designated apps'
   binaries at all without reading vendor code inlined into them; if not, those types
   follow the documentation and the failures SU4 shows.
