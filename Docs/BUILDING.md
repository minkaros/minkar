# Building Minkar from source

How to build Minkar, make a bootable image and boot it in QEMU, with the host it needs,
the build's layers and gates, and the errors people hit. For anyone building the tree.
Last reviewed: 2026-11-21. Spanish copy: [BUILDING.es.md](BUILDING.es.md).

## Quick start

On Ubuntu 24.04 (x86-64, a CPU with AVX), with 16 GB of RAM, about 40 GB of free disk on
a case-sensitive file system, and root:

    git clone https://github.com/minkaros/minkar && cd minkar && mkdir -p ../build
    sudo tools/ci/host-setup.sh packages image
    sudo bmake -m $PWD/BSD/share/mk -j$(nproc) > ../build/world.log 2>&1
    sudo bmake -m $PWD/BSD/share/mk boottest-key
    sudo env INIT= ROOT_PASSWORD=secret ROOT_AUTHORIZED_KEYS=../build/boottest-ssh/id_ed25519.pub \
        bmake -m $PWD/BSD/share/mk image
    sudo env GATE=M2 ROOT_PASSWORD=secret BOOTTEST_SSH_KEY=../build/boottest-ssh/id_ed25519 \
        bmake -m $PWD/BSD/share/mk boottest

The first build compiles the toolchain (LLVM, Swift) and takes hours. `boottest` prints
the QEMU command line to run the image yourself. The rest of this page is reference.

## How the build is laid out

Minkar is built with BSD make (bmake). The build first creates a host toolchain
(Default.xctoolchain: clang/LLVM, Swift, cctools, ld64, xcbuild, mig, ...) and then uses
it, with the Minkar.sdk it produces, to build the rest of the system. The build directory
is `../build` next to the source tree (`ROOT_BINARY_DIR`).

## Host requirements

The reference host is Ubuntu 24.04.5 LTS (x86-64; kernel 6.8, clang 18,
GCC 13, glibc 2.39, QEMU 8.2, Python 3.12) with 24 CPUs and 32 GB of RAM;
the CI uses GitHub's `ubuntu-24.04` runners. (Until 7 Nov 2026 it was
Ubuntu 22.04.) Package names are Debian/Ubuntu ones;
`tools/ci/host-setup.sh packages image` installs all of them.

The host compiler builds only the toolchain (LLVM, cctools, Swift and the
build tools); Minkar itself is built by the toolchain's own clang, so the
host's compiler version does not change the system that comes out.

Requirements: a case-sensitive file system (see below), at least 16 GB of
RAM for `-j10` (LLVM's links are the peak), about 40 GB of free disk, and
an x86-64 CPU with AVX if the boot test runs under KVM.

* Toolchain build: `clang lld cmake ninja-build python3 python3-venv git
  curl`, `libxml2-dev libpng-dev uuid-dev libssl-dev zlib1g-dev
  libbsd-dev`, `libacl1-dev` (xar), `libc++-dev libc++abi-dev` (plktool),
  `libedit-dev libffi-dev`, `libblocksruntime-dev` (ld64's `Block.h`),
  `libsqlite3-dev` (Swift Build's llbuild),
  and `libicu-dev libkqueue-dev autoconf automake libtool pkg-config` for
  Open CF-lite (its run loop includes `sys/event.h`, which libkqueue
  gives). (The CTF tools no longer link the host's LLVM or libdispatch:
  they build with the toolchain's LLVM sources and a small dispatch shim.)
* libdispatch (swift-corelibs-libdispatch, tag `swift-6.0-RELEASE`,
  Apache-2.0) in `/usr/local`: Open CF-lite's headers include
  `<dispatch/dispatch.h>`, and Ubuntu 24.04 has no `libdispatch-dev`.
  `tools/ci/host-setup.sh packages` builds it before Open CF-lite (cmake
  and ninja); like Open CF-lite it is a build tool only.
* Open CF-lite 635.21.8 (gerickson/opencflite, APSL) in `/usr/local`:
  plktool and xcbuild link a host CoreFoundation. It is a build tool only
  and nothing of it goes into Minkar. `tools/ci/host-setup.sh packages`
  builds it (`./configure --with-tz-includes=<its source>`, since Ubuntu
  has no `tzfile.h`; then `make && make install`).
* System build: `bmake make csh bison flex time`, and `byacc` (Berkeley
  yacc, public domain: libxkbcommon's parser is generated with it, not
  bison), `ruby` (2.5 or later: JavaScriptCore's build generates code
  with it), and `gperf` and `unifdef` (WebCore's build generates its
  lookup tables and WebKit's GLib headers with them)
* Bootable image and boot test: `nasm gdisk dosfstools mtools hfsprogs`,
  `qemu-system-x86 ovmf` (the boot test runs QEMU under TCG unless
  `BOOTTEST_ACCEL=kvm`; its VM is an Ivy Bridge, `-cpu IvyBridge`, since
  Minkar needs AVX, and QEMU emulates AVX under TCG from 7.2 on, so
  Ubuntu 24.04's 8.2 is enough),
  `openssh-client openssl` for gate M2, and the kernel's `hfsplus` module
  (in `linux-modules-extra-*` on cloud kernels).

The build calls `/usr/bin/clang` itself (`HOST_CC`, `HOST_CXX`), so `cc`
may stay gcc.

The source tree must be on a case-sensitive file system (ext4, not a
default HFS+ or APFS volume): some trees have names that differ only in
case. Disk space, measured on 2026-11-21 after the committed ncurses build output and
the 21 unbuilt components were removed: 3.0 GB of tracked sources (232,104 files) and
1.9 GB of git history; on the development host's build tree, 2.7 GB of toolchain, 16 GB
of object directories, 3.5 GB for the host Swift the toolchain builds with, a 1.2 GB
sysroot and a 1.2 GB image. Before that removal the sources took about 7 GB with their
history.

## Building

The build runs as root (the object directories and sysroot are root owned):

    cd /work/minkar
    sudo bmake -m /work/minkar/BSD/share/mk -j$(nproc) > ../build/world.log 2>&1

One stage at a time, in this order: `Developer Kernel Libraries Frameworks
Kernel/Extensions CoreServices BSD Frameworks/WebKit SystemLibrary`. Name the
stage both in `ONLY_STAGE` and as the target:

    sudo env ONLY_STAGE=Frameworks bmake -m /work/minkar/BSD/share/mk -j$(nproc) Frameworks

(`SUBDIR=<stage>` in the environment does the same; never pass it on the
command line, where it would leak into every sub-make.) The disk image is
made from the sysroot afterwards:

    sudo bmake -m /work/minkar/BSD/share/mk image

`-m /work/minkar/BSD/share/mk` is required. Logs use ANSI colors; strip them
before searching: `sed 's/\x1b\[[0-9;]*m//g' ../build/world.log`.

Outputs, under `../build`:

* `Developer/Platforms/Minkar.platform/Developer/Toolchains/Default.xctoolchain`
* `Developer/Platforms/Minkar.platform/Developer/SDKs/Minkar.sdk`
* `sysroot/` - the root file system
* `kernelcache` - the prelinked kernel and extensions

## Expected times

Measured from scratch on 2026-11-21 and 22, at `devel` `de48918522`, on the
build host: Ubuntu 24.04.5 LTS (kernel 6.8), 24 CPUs (one Xeon E5-2697 v2,
12 cores with 2 threads each), 32 GB of RAM, `JOBS=20`, turbo off
(`minkar-heavy`), in a clean worktree with an empty build tree, with
`tools/ci/build-stages.sh`. Three stages stopped on build-order problems
found by this run (fixed in `11a96ec85c`, `0dd9fece36`, `ff19ade12e` and
`de48918522`) and were run again from where they stopped; their times are
the sum of their passes, which is close to one uninterrupted pass since
the second only builds what the first did not. In the CI, the toolchain
takes about two hours when its cache misses, and gate BC 2 to 3 hours
under TCG.

| Stage | Wall time |
| --- | --- |
| `Developer` (toolchain: LLVM and clang, then Swift) | 3 h 12 min the first time (LLVM about 1 h 30 min, Swift about 1 h 40 min); seconds once built |
| `Kernel` | 10 min |
| `Libraries` | 55 s |
| `Frameworks` | 34 min |
| `Kernel/Extensions` | 56 s |
| `CoreServices` | 1 min 30 s |
| `BSD` (WPE WebKit and JavaScriptCore, Mesa, FFmpeg, Ruby, Python, Perl...) | 62 min |
| `Frameworks/WebKit` | 7 s (its engine is built in `BSD`) |
| `SystemLibrary` | 37 s |
| Everything after `Developer` | 1 h 50 min |
| The whole build from scratch | about 5 h |
| `image`, first time (fetches and builds the EFI loader) | 3 min 15 s |
| `image`, after that | 13 s |
| `boottest GATE=M1` | about 20 s |
| `boottest GATE=M2` | about 2 min (passed at 117 s) |

A change to LLVM's sources rebuilds LLVM (about 1 h 30 min) and what
the toolchain builds on it.

Per-layer targets build one layer's stages alone, in order, on the layers
below it already built, and `<layer>-check` checks what it installed
without booting (`layers-check` runs them all; `image-check` boots the
image to gate M2):

| Layer | Stages | `<layer>-check` |
| --- | --- | --- |
| `toolchain` | `BSD/hier`, `Developer` | the toolchain manifest (below) |
| `kernel` | `Kernel`, `Kernel/Extensions` | the kernel is a Mach-O, the kernelcache well formed, every kext a kext bundle |
| `libsystem` | `Libraries` | every library `/usr/lib` loads is in the sysroot |
| `frameworks` | `Frameworks` | the same for the frameworks; their plists parse |
| `userland` | `BSD` | the same for the commands |
| `sysapps` | `CoreServices`, `SystemLibrary` | the same for CoreServices; the system's plists parse |

The checks are `tools/check/sysroot-check.py` (links, plists, kexts,
macho, kernelcache).

The toolchain is pinned by `tools/toolchain/MANIFEST`: each component's
upstream version, the git tree of its sources and the version line its
tool prints. `tools/toolchain/manifest.sh check <toolchain>` verifies
them and that the same input compiles, links and archives to the same
bytes twice; after changing a component, `tools/toolchain/manifest.sh
update` rewrites the trees. The build exports `SOURCE_DATE_EPOCH` (the
last commit's date), so `__DATE__` and `__TIME__` do not make two builds
differ.
`ONLY_STAGE` (above) builds one stage without walking the ones before it.

## Bootable image and boot test

After the build:

    sudo env SUBDIR= bmake -m /work/minkar/BSD/share/mk image
    sudo env SUBDIR= bmake -m /work/minkar/BSD/share/mk boottest           # gate M0
    sudo env SUBDIR= GATE=M1 bmake -m /work/minkar/BSD/share/mk boottest   # gate M1

`image` fetches the EFI loader (ravynsoft/efiloader, pinned in
`Image/Makefile` and patched from `Image/patches/`), builds it with the
toolchain's clang, and writes `../build/Minkar.img`: a GPT disk with an EFI
system partition (loader, kernelcache, boot plist) and an HFS+ root volume
made from the sysroot. `boottest` boots the image in QEMU with OVMF, follows
the serial console and passes when the gate's markers appear; the console is
saved to `../build/boottest-<gate>.log`. Run the image yourself with the
command `boottest` prints.

Gates (see the architecture plan):

* M0 - the kernel prints its first message.
* M1 - the HFS+ root mounts, PID 1 runs and starts a shell, and the shell
  answers a command typed on the serial console.
* M2-login - launchd as PID 1 starts getty on the console, root logs in
  through login and PAM, and the shell answers. It needs launchd as PID 1
  and a root password in the image:

      sudo env SUBDIR= INIT= ROOT_PASSWORD=secret bmake -m /work/minkar/BSD/share/mk image
      sudo env SUBDIR= GATE=M2-login ROOT_PASSWORD=secret bmake -m /work/minkar/BSD/share/mk boottest

  Images have no root password unless ROOT_PASSWORD is given (hashed with
  the host's `openssl passwd -6`). For launchd bring-up, LAUNCHD_DEBUG=1
  makes launchd and launchctl log everything to the console.
* M2 - M2-login, then the network: the e1000e NIC (MinkarE1000 on our
  IONetworkingFamily, with an MSI interrupt) comes up as en0, dhcpclient gets a lease from QEMU
  (10.0.2.15), and root logs in over ssh from the host with a key; the
  remote shell answers, `scutil --dns` shows the name server, and the
  host's name resolves with `dscacheutil` (IPMonitor's resolv.conf,
  mDNSResponder, getaddrinfo). QEMU sends the
  guest's DNS queries to the host's resolver, so the host must resolve its
  own name (systemd-resolved and /etc/hosts do); no Internet is needed. QEMU forwards host port 2222 (BOOTTEST_SSH_PORT)
  to the guest's port 22. `boottest-key` makes the key pair once:

      sudo env SUBDIR= bmake -m /work/minkar/BSD/share/mk boottest-key
      sudo env SUBDIR= INIT= ROOT_PASSWORD=secret \
          ROOT_AUTHORIZED_KEYS=../build/boottest-ssh/id_ed25519.pub \
          bmake -m /work/minkar/BSD/share/mk image
      sudo env SUBDIR= GATE=M2 ROOT_PASSWORD=secret \
          BOOTTEST_SSH_KEY=../build/boottest-ssh/id_ed25519 \
          bmake -m /work/minkar/BSD/share/mk boottest

  ROOT_AUTHORIZED_KEYS installs any public key file as root's
  authorized_keys. sshd keeps OpenSSH's default PermitRootLogin
  prohibit-password: root logs in over ssh with a key, other users with
  their password (PAM). `boottest.py --nic e1000` tests the 82540EM,
  which has no MSI: it uses its INTx line, routed through ACPI _PRT by
  ACPICA in IOACPIFamily.

  configd (the APSL one, with its plugins built in) names the interfaces: its
  InterfaceNamer gives IONetworkStack the unit each one keeps across boots
  (/Library/Preferences/SystemConfiguration/NetworkInterfaces.plist) once
  IOKit is quiet; kextstart tells IOKit at boot that there is no kextd to
  wait for. Without a namer, IONetworkStack names them itself after 30 s.
  dhcpclient serves every Ethernet interface as it appears and publishes
  what it configures to configd (State:/Network/Service/<id>), whose
  IPMonitor sets the default route and resolv.conf and serves the DNS
  configuration (`scutil --dns`, `scutil --nwi`); without configd it does
  those itself. `ipconfig set en0 DHCP|BOOTP|NONE|MANUAL <addr> <mask>|INFORM <addr> <mask>`
  and `ipconfig set en0 AUTOMATIC-V6|LINKLOCAL-V6|NONE-V6|MANUAL-V6 <addr> <prefixlen>`
  change its method, `ipconfig getsummary en0` shows what it has.

* BC - M2 and the window server, then every designated app's published
  binary (lane BC in [ROADMAP.md](ROADMAP.md); the apps of
  `tools/symbols/reference-apps.tsv`). `bc-check` downloads them to
  `../build/bc-apps` by their URL and SHA-256 (kept there; never put in the
  repository or the image), and for each one copies its archive into the
  VM from the apps disk (`../build/bc-apps/bc-apps.img`, a FAT32 image of
  the downloads made with `mtools`, attached read-only; `--no-apps-disk`
  in `BC_ARGS` sends them over ssh instead), where `Tests/BC/bc-run.sh` installs it as a user would (a disk image
  attached and the app copied to /Applications, a zip or a tar expanded)
  and starts it with a probe. Each app's result is how far it got (from
  `install` to `window`); one that gets less far than in
  `Docs/progress/compatibility.tsv` fails the gate. `BC_UPDATE=yes` writes
  the new results there, and `bmake progress` makes
  [Compatibility.md](Compatibility.md) from them. The image needs the
  probe (`bc-tools` before `image`):

      sudo env SUBDIR= bmake -m /work/minkar/BSD/share/mk bc-tools
      sudo env SUBDIR= INIT= ROOT_PASSWORD=secret \
          ROOT_AUTHORIZED_KEYS=../build/boottest-ssh/id_ed25519.pub \
          bmake -m /work/minkar/BSD/share/mk image
      env SUBDIR= BC_ARGS="--password secret --ssh-key ../build/boottest-ssh/id_ed25519 --accel kvm" \
          bmake -m /work/minkar/BSD/share/mk bc-check

Until launchd runs, PID 1 is `minkarinit` (`BSD/sbin/minkarinit`), a small
temporary init that the image installs as `/sbin/launchd`, the only path a
RELEASE kernel tries. Build the image with `INIT=` to boot the sysroot's own
`/sbin/launchd` instead.

## Common errors

* `Could not find bsd.mkopt.mk` (and `bsd.opts.mk`, `bsd.linker.mk`)\
  bmake was run without `-m /work/minkar/BSD/share/mk`.
* `cd: .../Frameworks/Frameworks: No such file or directory`\
  `SUBDIR=` was given on the command line. Pass it through the
  environment (`env SUBDIR=...`) or use `ONLY_STAGE`.
* `SYSROOT_DIR is not set: build BSD/etc from the top of the tree`\
  bmake was run inside a subdirectory. Always run it from the top of the
  tree: a subdirectory alone does not know the sysroot and could install
  into the build host.
* `Permission denied` on `../build`\
  The build runs as root (`sudo`); the object directories and the
  sysroot are root owned.
* `byacc (Berkeley yacc) is required on the build host`\
  Install `byacc` (libxkbcommon's parser, in `CoreServices`).
* Toolchain link errors: `cannot find -lacl`, `-lCoreFoundation`, `-ldispatch`, `-lLLVM` or `-lc++`\
  Host packages are missing, or Open CF-lite is not in `/usr/local`
  (see "Host requirements"). `tools/ci/host-setup.sh packages` installs
  them.
* Undefined symbols when linking a base library in a clean build, although an incremental build works\
  A base library gained a symbol another one uses, and the bootstrap
  stubs do not list it yet. Run `Libraries/bootstrap/regen-stubs.sh` and
  commit the `.tbd` files it changes.
* The whole of LLVM rebuilds although the toolchain is already built\
  The toolchain's tools are older than their sources (a copied or
  restored toolchain, or a fresh checkout): bmake rebuilds mig, and LLVM's
  rule depends on it. `tools/ci/host-setup.sh touch-toolchain` marks a
  restored toolchain up to date.
* A kernel change does not show up in the image\
  The kernelcache is made by `Kernel/Extensions`. After building
  `Kernel`, build `Kernel/Extensions` too.
* An edited AppKit header is not seen by AppKit itself\
  AppKit includes `<AppKit/...-Private.h>` from the framework copy in its
  object directory, refreshed only at link time. Copy the header there
  (`AppKit.framework/Headers`) or relink.
* Everything rebuilds after moving the source tree\
  Object directories are named after the source path
  (`../build/<source path>/...`). Keep the tree where it was built.
* Odd missing files on the reference system or on an HFS+ or APFS disk\
  The file system is case-insensitive; some trees have names that differ
  only in case. Use a case-sensitive volume.
* `mount: unknown filesystem type 'hfsplus'` (image)\
  The host kernel lacks the `hfsplus` module: install
  `linux-modules-extra-$(uname -r)` and `modprobe hfsplus`.
* `boottest: gate M2 needs --password or $BOOTTEST_PASSWORD`, `needs --ssh-key or $BOOTTEST_SSH_KEY`\
  Build the image with `ROOT_PASSWORD` and `ROOT_AUTHORIZED_KEYS`, and
  pass the same password and the private key to `boottest` (see above).
* `boottest: ... the host cannot resolve it either`\
  Gate M2 resolves the host's own name from the guest. Add it to
  `/etc/hosts` (`127.0.1.1 <hostname>`).
* The kernel panics right after the loader when QEMU is run by hand\
  The loader needs `-device virtio-rng-pci` (EFI_RNG_PROTOCOL), and
  use `-cpu IvyBridge` (AVX), not `max` or `host`. `Image/boottest.py` shows a
  working command line.
* `'stdarg.h'`-related errors in ld64 (`va_start` undeclared), or
  `Block.h` not found in ld64's `Snapshot.cpp`\
  An older tree on glibc 2.39 (Ubuntu 24.04), which no longer includes
  `stdarg.h` from other headers, or `libblocksruntime-dev` is missing.
  Update the tree and run `tools/ci/host-setup.sh packages`.
* `dispatch/dispatch.h` or `sys/event.h` not found while building Open
  CF-lite\
  libdispatch is not in `/usr/local` or `libkqueue-dev` is missing:
  `tools/ci/host-setup.sh libdispatch`, then `packages`.
* `-Wint-conversion` or `-Wincompatible-pointer-types` errors in the CTF
  tools\
  clang 15 and later turn these into errors; the tree builds them with
  `-Wno-error=...`. An older tree needs updating.
* `'CoreFoundation/CFBase.h' file not found` in libdispatch, or
  `'Foundation/Foundation.h' file not found` in the Swift runtime, during
  `Kernel`\
  A build from scratch in an older tree: `Kernel` builds `Libraries`
  before the `Frameworks` stage installs CoreFoundation and Foundation.
  The tree now copies CoreFoundation's headers into the SDK first, and
  builds the Swift runtime in `Frameworks`, after Foundation. Update the
  tree.
* Logs are hard to search\
  They carry ANSI colors: `sed 's/\x1b\[[0-9;]*m//g' file.log`.
  `tools/ci/build-stages.sh` keeps one log per stage and prints the first
  error lines of a stage that fails.

## Host tests

Portable parts of the system have host-side tests (clang, ASan/UBSan):

* `make -C Frameworks/OpenGL/tests`
* `python3 -B Developer/Default.xctoolchain/codesign/tests/test_codesign.py`
* `make -C Libraries/Libsystem/libsystem_c/minkar_crypt/tests` (crypt(3) SHA-crypt)
* `cc -DTEST -I Libraries/Libsystem/libsystem_coreservices -o /tmp/sysdir_test Libraries/Libsystem/libsystem_coreservices/sysdir.c && /tmp/sysdir_test`

## Timed builds

`tools/ci/build-stages.sh` builds the stages one at a time, each with its
own log and timing (`../build/logs/<stage>.log`, `../build/logs/times.txt`),
and prints the end of the log of a stage that fails:

    cd /work/minkar
    sudo tools/ci/build-stages.sh
    sudo env STAGES="Frameworks BSD" tools/ci/build-stages.sh

## Continuous integration

`.github/workflows/build-boottest.yml` runs on pushes and pull requests
to `main` and by hand (Actions > build and boottest > Run workflow, on
any branch): the repository is private on a free account, whose Actions
minutes runs of over an hour use up, so `devel`'s batches are verified
locally. Changes go to `devel` first. On a
GitHub-hosted Ubuntu 24.04 runner it:

1. checks, in seconds and before any build, that the progress pages are
   what `bmake progress` writes and that each stage lists what a
   directory needs before it (`tools/check/subdir-order.py`: Frameworks
   builds its `SUBDIR` in order, so a dependency listed later breaks only
   a build from scratch, hours into it);
2. builds the toolchain (`Developer`), or takes it from the cache;
3. builds the rest of the system with `tools/ci/build-stages.sh`, and
   checks the toolchain against `tools/toolchain/MANIFEST`;
4. makes an image with launchd as PID 1, a random root password and a
   fresh ssh key, and runs `boottest GATE=M2`.

The toolchain takes about two hours on a hosted runner, so it is cached.
The cache key is the git tree of `Developer/Default.xctoolchain`: any
change there builds it again. The cache holds the installed toolchain and
its object directories, but not LLVM's build tree; after restoring it,
`tools/ci/host-setup.sh touch-toolchain` makes it newer than the checkout,
or bmake would rebuild mig and, with it, LLVM. Bump `CACHE_VERSION` in the
workflow to throw the cache away.

When a stage or the boot test fails, the end of its log shows up as an
error annotation on the run; the full logs are in the run's artifacts.
