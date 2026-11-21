# Minkar's project structure

Status: **proposed**, 2026-11-20. The decision is the maintainer's (2026-11-20); this
document says how each step would be done, with the numbers of the inventory made the
same day (step R0). Nothing has moved. Spanish copy: [Structure.es.md](Structure.es.md).

## 1. The decision

**The system's core stays in one repository and is not split:** Kernel, Libraries,
Frameworks, WindowServer, the daemons in CoreServices, Dock, Files, loginwindow and
SystemUIServer. They change together, and the gates boot the whole image: a kernel,
framework or session change is only checked when all of them are built into the same
image.

**What does not change with the core leaves the tree**, in this order and at these times:

| Step | What | When | Why then |
| --- | --- | --- | --- |
| R0 | The inventory and this document. Nothing moves | Now (2026-11-20) | It does not touch the build |
| R1 | The toolchain (`Developer/Default.xctoolchain`) to its own repository; the tree uses it as a prebuilt artifact pinned by SHA-256. In the same step, AArch64 joins LLVM's targets | Right after 19.6.0 ships, before KX1 | It has to be built again for arm64 anyway, and the new kernel starts on the cleaned structure |
| R2 | BSD's third-party sources become pinned downloads plus a directory of patches, through `minkar.extlib.mk` | During 19.6.x, one component per commit | A mechanical, reversible change, with no risk for the first release |
| R3 | The CI publishes each built layer (toolchain, kernel and kexts, base sysroot), so a change builds only its own layer | During 19.6.x, after R1 | It needs the toolchain to be an artifact already |
| R4 | Atlas's apps (Terminal, Text Editor, System Monitor, Logs, and A5–A10 as they come) to their own repository, built against the published `Minkar.sdk` | After 19.6.0, once the SDK is published | It needs a published, stable SDK |

**Before 19.6.0 ships, nothing of R1–R4 is done.** Meanwhile arm64 in the CI (ARM0) uses
the clang of the swift.org release the build already downloads, with the header overlay
made from `Kernel/xnu`; AArch64 in the tree's own LLVM comes with R1.

**Rules:**

- One step at a time; each step is a reversible commit, and after each one the gates and
  the batch tests pass.
- No history is rewritten. A new repository gets its history with `git subtree split`;
  in the main repository the directory is removed by an ordinary commit.
- Licenses, copyright lines and `NOTICE` entries travel with the code.
- The maintainer creates the repositories.
- Each R step starts only after the maintainer confirms it.

## 2. What the inventory found (R0)

From the inventory of 2026-11-20 (tracked files only, checked against the upstreams by
blob hash where possible; the report is kept outside the repository):

| Class | Files | Size |
| --- | ---: | ---: |
| Minkar's own | 4,248 (1.7 %) | 60 MB (1.0 %) |
| Third-party, unmodified | 215,871 (88.3 %) | 2,762 MB (45.3 %) |
| Third-party, modified | 1,058 (0.4 %) | 15 MB (0.3 %) |
| Third-party, not verified | 9,370 (3.8 %) | 201 MB (3.3 %) |
| Generated | 13,981 (5.7 %) | 3,058 MB (50.2 %) |
| Total | 244,528 | 6,096 MB |

- **The toolchain is most of the tree:** `Developer/Default.xctoolchain` holds 151,677 files
  (62 %) and 1,716 MB. Its LLVM is swiftlang's `swift-6.4.2-RELEASE` in 147,681 of 147,885
  files; Minkar's changes are 14 files (124 lines), one added file and the build files.
- **Half the bytes are build output committed by mistake:** `BSD/lib/ncurses/obj_s` holds
  10,786 compiler dump files (3.03 GB), from before the fork; the ncurses sources themselves
  are unmodified. They were removed on 2026-11-21 (`723a0f77ba`; the history keeps them).
- **`Developer/Minkar.sdk` holds 2,946 copies** of other tracked files (libc++ and xnu
  headers, manual pages), which the build could produce instead.
- **Coupling:** since the fork, 69 % of commits touch only one of Kernel, Libraries,
  Frameworks, CoreServices, BSD and Developer. CoreServices' commits touch Frameworks too
  55 % of the time; Developer's touch Libraries 40 % of the time; Frameworks' touch
  Developer 2 % of the time. This supports the core as decided: CoreServices and
  Frameworks move together, and the toolchain is the part that changes alone.
- **Times** (existing measurements only): from scratch, on the previous host, the toolchain
  took 52 minutes and everything after it 16 to 19 ([BUILDING.md](BUILDING.md)); in the CI
  the toolchain takes about two hours when its cache misses. A batch run's median is 18
  minutes (86 batches, 17–20 Nov). Gate BC adds 2 to 3 hours under TCG.

## 3. R1 — the toolchain as an artifact

**What moves:** `Developer/Default.xctoolchain` (LLVM and clang, Swift, cctools and ld64,
mig, xar, gmake, unifdef, the CTF tools, rpcgen, plktool, xcbuild, ibtool, iig, codesign).
`tools/toolchain` (its manifest and `manifest.sh`) moves with it.

**Repository:** `minkaros/minkar-toolchain` (decided 2026-11-21).

**How it is published:** the new repository's CI builds the toolchain for the build host
(Ubuntu 24.04, x86-64) and publishes one archive per release, with its SHA-256, tagged
by the toolchain's version (for example `21.1.6-swift6.4.2-1`). Its manifest records the
sources' trees, the tools' versions and the reproducibility check that `manifest.sh check`
does today.

**How the tree uses it:** a pinned manifest in the main repository (url, sha256, version),
fetched by the same mechanism as the other pinned downloads
(`BSD/lib/libdrm/fetch.sh`), unpacked into `../build/Developer/Platforms/Minkar.platform`,
and checked with `manifest.sh check` against the published manifest. `bmake` builds
the stages after `Developer` exactly as now.

**The other way:** `TOOLCHAIN_SOURCE=<a checkout of minkar-toolchain>` builds it from its
sources, as the tree does today, for a change to the toolchain itself or a host the
artifact does not cover.

**plktool and the build tools that belong to the system:** `plktool` (the kernelcache
builder) and `codesign` are tools of the system's build, not of the compiler: they could
stay in the main repository (`tools/` or `Developer/`) and be built with the host's
compiler. They stay (decided 2026-11-21).

**AArch64:** in the same step, `LLVM_TARGETS_TO_BUILD` becomes `X86;AArch64`, with the
default target triple still x86-64. clang, lld and the Swift compiler then target arm64,
and ARM0 moves to the tree's own toolchain.

**How it is undone:** restore the directory with an ordinary commit from the history and
point the build back at it.

**What validates it:** the full build from the artifact, `bmake layers-check`, gates M2
and M3, gate BC, the batch tests with `LOTE_SCOPE=base`, and `manifest.sh check`.

## 4. R2 — BSD's third-party sources as pinned downloads

**What moves:** each third-party component under `BSD` (and `GNU`) becomes a manifest
(url, sha256, version) plus a `patches/` directory, built through `minkar.extlib.mk`, one
component per commit. 33 components are already pinned downloads (31 by URL and SHA-256,
WebKit and JavaScriptCore by git commit).

**Order, easiest first** (from the inventory):

1. The five that `minkar.extlib.mk` already builds from an in-tree copy (`EXTLIB_SOURCE`):
   fontconfig, freetype, libxml2, libjpeg, libpng. The rule exists; only the source changes.
2. Unmodified components with their own build system (66), for example vim, openssl, bind,
   git, sudo, zsh, nano, libtiff, ksh, tcpdump, rsync, tcsh, htop, liburcu, libuv, dash and
   ncurses (whose committed build output goes with it).
3. Lightly patched ones (122), for example heimdal (3 files), curl (3 files), pixman,
   dtrace, mDNSResponder, bash.
4. Heavily modified or unverified ones (65), for example configd, launchd, the hfs and nfs
   tools, Info-ZIP, SQLite, libedit, libarchive, pfctl.

**What does not fit:** the FreeBSD commands built with bmake's own rules (most of
`BSD/bin`, `sbin` and `usr.bin`) cannot become downloads through `minkar.extlib.mk` as it
is: it builds projects with their own build system. They stay in the tree (decided
2026-11-21).

**Unreferenced:** 21 components no Makefile named (among them `GNU/`'s bash 5.2 and
dmidecode, and `BSD/lib/libutf8`) were removed on 2026-11-21 (`1d3e679f11`).

**How it is undone:** revert the component's commit. **What validates it:** the stage's
build, the component's tests, and the batch tests the files reach.

## 5. R3 — the CI publishes the built layers

**What changes:** the CI publishes, per commit on `main`, the built toolchain (from R1),
the kernel with its kexts, and the base sysroot, each with its SHA-256. A contributor who
changes one layer fetches the others and builds only theirs.

**How it is pinned:** by the commit that built each layer; the build checks that the
fetched layer matches the sources it claims (the tree hash of its directories).

**How it is undone:** the build falls back to building everything, as today.

**What validates it:** a build from the published layers gives the same image as a full
build (compared file by file), and the gates pass on it.

## 6. R4 — Atlas's apps to their own repository

**What moves:** the user apps in `CoreServices`: Terminal, TextEditor, SystemMonitor,
Logs and ForceQuit, and A5–A10 as they come. SystemPreferences is a candidate too, but its
Mouse and Keyboard panes use the private SkyLight interface, and its panes share
notification keys and defaults names with the Dock and SystemUIServer: it stays in the
core (decided 2026-11-21).

**What stays:** the Dock, Files, loginwindow, SystemUIServer and the twelve daemons.

**What the apps use outside the public SDK:** five of the six candidates use only public
headers. In the core, Files uses a private AppKit selector and Minkar's `LSRevealInFiles`,
and the Dock and loginwindow use SkyLight; they stay, so this does not block R4.

**How it is built:** against the published `Minkar.sdk`, with the same `xcbuild` and
project files; the image takes the apps' published builds.

**How it is undone:** bring the directories back with an ordinary commit.

**What validates it:** the apps' own tests in the image, gate M5's desktop, and the
batch tests.

## 7. What this does not achieve

- **The history does not shrink.** Moving a directory out leaves every past version of it
  in the main repository's history, including the 3 GB of build output under ncurses;
  only a rewrite would remove them, and history is not rewritten.
- It does not split the core: the gates still boot one image built from one repository.

## 8. Decided on 2026-11-21

The maintainer answered the open questions of the proposed version:

1. The toolchain's repository is `minkaros/minkar-toolchain`, and its artifact is
   published as that repository's GitHub Releases.
2. The artifact covers only Ubuntu 24.04 on x86-64 for now.
3. `plktool`, `codesign` and the other tools of the system's build stay in the main
   repository, built with the host's compiler.
4. The FreeBSD commands built with bmake stay in the tree; R2 converts only the
   projects with their own build system.
5. The build output under `BSD/lib/ncurses/obj_s` was removed at once (`723a0f77ba`).
6. SystemPreferences stays in the core; R4 takes the other apps.
7. `Developer/Minkar.sdk`'s copies are generated by the build from R1 on.
8. The 21 components no Makefile built were checked one by one and removed
   (`1d3e679f11`).
