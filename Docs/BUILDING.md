# Building Minkar From Source

Minkar is built with BSD make (bmake). The build first creates a host
toolchain (Default.xctoolchain: clang/LLVM, Apple cctools, ld64, xcbuild,
mig, ...) and then uses it, together with the Minkar.sdk it produces, to
build the rest of the system for the same CPU architecture.

The source tree is `/work/minkar` and the build directory is `../build`
next to it (`ROOT_BINARY_DIR`); adjust the paths below to your checkout.
Building LLVM the first time takes a __long__ time.

## Host requirements

Tested on Ubuntu 22.04 (x86-64) with 10 CPUs and 16 GB of RAM; the CI uses
GitHub's Ubuntu 22.04 runners. Package names are Debian/Ubuntu ones;
`tools/ci/host-setup.sh packages image` installs all of them.

* Toolchain build: `clang lld cmake ninja-build python3 git curl`,
  `libxml2-dev libpng-dev uuid-dev libssl-dev zlib1g-dev libbsd-dev`,
  `libacl1-dev` (xar), `libc++-dev libc++abi-dev` (plktool),
  `llvm-14-dev libedit-dev libffi-dev libdispatch-dev libblocksruntime-dev
  libkqueue-dev libpthread-workqueue-dev` (the CTF tools link the host's
  LLVM 14 and libdispatch), and `libicu-dev autoconf automake libtool
  pkg-config` for Open CF-lite.
* Open CF-lite 635.21.8 (gerickson/opencflite, APSL) in `/usr/local`:
  plktool and xcbuild link a host CoreFoundation. It is a build tool only
  and nothing of it goes into Minkar. `tools/ci/host-setup.sh packages`
  builds it (`./configure --with-tz-includes=<its source>`, since Ubuntu
  has no `tzfile.h`; then `make && make install`).
* System build: `bmake make csh bison flex time`, and `byacc` (Berkeley
  yacc, public domain: libxkbcommon's parser is generated with it, not
  bison)
* Bootable image and boot test: `nasm gdisk dosfstools hfsprogs`,
  `qemu-system-x86 ovmf` (the boot test runs QEMU without KVM, under TCG),
  `openssh-client openssl` for gate M2, and the kernel's `hfsplus` module
  (in `linux-modules-extra-*` on cloud kernels).

The build calls `/usr/bin/clang` itself (`HOST_CC`, `HOST_CXX`), so `cc`
may stay gcc.

The source tree must be on a case-sensitive file system (ext4, not a
default HFS+ or APFS volume): some trees have names that differ only in
case. Disk space: about 7 GB of sources with their git history, 8 GB of
build tree (2 GB of it LLVM's build directory) and a 2 GB image.

## Building

The build runs as root (the object directories and sysroot are root owned):

    cd /work/minkar
    sudo bmake -m /work/minkar/BSD/share/mk -j$(nproc) > ../build/world.log 2>&1

One layer at a time, in this order: `Developer Kernel Libraries Frameworks
Kernel/Extensions CoreServices BSD`. Pass the layer through the environment,
never on the command line (it would leak into every sub-make):

    sudo env SUBDIR=Frameworks bmake -m /work/minkar/BSD/share/mk -j$(nproc)

`-m /work/minkar/BSD/share/mk` is required. Logs use ANSI colors; strip them
before searching: `sed 's/\x1b\[[0-9;]*m//g' ../build/world.log`.

Outputs, under `../build`:

* `Developer/Platforms/Minkar.platform/Developer/Toolchains/Default.xctoolchain`
* `Developer/Platforms/Minkar.platform/Developer/SDKs/Minkar.sdk`
* `sysroot/` - the root file system
* `kernelcache` - the prelinked kernel and extensions

## Expected times

Measured on 2026-10-25 on the reference host: Ubuntu 22.04, 10 CPUs
(Xeon E5-2690 v2), 16 GB of RAM, `-j10`, clean build tree.

| Stage | Wall time |
| --- | --- |
| `Developer` (toolchain, LLVM included) | 52 min the first time; seconds once built |
| `Kernel` | 9-10 min |
| `Libraries` | about 1 min |
| `Frameworks` | 4 min |
| `Kernel/Extensions` | 20 s |
| `CoreServices` | a few seconds |
| `BSD` | 2-2.5 min |
| Everything after `Developer` | 16-19 min |
| `image`, first time (fetches and builds the EFI loader) | 2 min |
| `image`, after that | 6 s |
| `boottest GATE=M1` | about 20 s |
| `boottest GATE=M2` | about 75 s (QEMU under TCG) |

A change to LLVM's sources rebuilds the toolchain (about 47 min).

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

  configd (Apple's, with its plugins built in) names the interfaces: its
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
* Odd missing files on macOS or on an HFS+ or APFS disk\
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
  under TCG use `-cpu Westmere`, not `max`. `Image/boottest.py` shows a
  working command line.
* Logs are hard to search\
  They carry ANSI colors: `sed 's/\x1b\[[0-9;]*m//g' file.log`.
  `tools/ci/build-stages.sh` keeps one log per stage and prints the first
  error lines of a stage that fails.

## Host tests

Portable parts of the system have host-side tests (clang, ASan/UBSan):

* `make -C Frameworks/OpenGL/tests`
* `make -C Frameworks/CoreGraphics/tests`
* `make -C CoreServices/WindowServer/tests` (needs a built sysroot)
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

`.github/workflows/build-boottest.yml` runs on every push and pull
request to `main` and `devel`, and by hand. Changes go to `devel` first. On a
GitHub-hosted Ubuntu 22.04 runner it:

1. builds the toolchain (`Developer`), or takes it from the cache;
2. builds the rest of the system with `tools/ci/build-stages.sh`, and
   checks the toolchain against `tools/toolchain/MANIFEST`;
3. makes an image with launchd as PID 1, a random root password and a
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
