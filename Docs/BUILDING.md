# Building ravynOS From Source

ravynOS is built with BSD make (bmake). The build first creates a host
toolchain (Default.xctoolchain: clang/LLVM, Apple cctools, ld64, xcbuild,
mig, ...) and then uses it, together with the ravynOS.sdk it produces, to
build the rest of the system for the same CPU architecture.

The source tree is `/work/ravynos` and the build directory is `../build`
next to it (`ROOT_BINARY_DIR`); adjust the paths below to your checkout.
Building LLVM the first time takes a __long__ time.

## Host requirements

Tested on Ubuntu 22.04 (x86-64). Package names are Debian/Ubuntu ones.

* Toolchain build: `clang lld cmake ninja-build python3 git curl`,
  `libxml2-dev libpng-dev uuid-dev libssl-dev zlib1g-dev libbsd-dev`
* System build: `bmake make csh bison flex`, and `byacc` (Berkeley yacc,
  public domain: libxkbcommon's parser is generated with it, not bison)
* Bootable image and boot test: `nasm gdisk dosfstools hfsprogs`,
  `qemu-system-x86 ovmf` (QEMU uses KVM when /dev/kvm exists, TCG otherwise)

Symlink `cc` and `c++` to clang to avoid accidental use of gcc.

## Building

The build runs as root (the object directories and sysroot are root owned):

    cd /work/ravynos
    sudo bmake -m /work/ravynos/BSD/share/mk -j$(nproc) > ../build/world.log 2>&1

One layer at a time, in this order: `Developer Kernel Libraries Frameworks
Kernel/Extensions CoreServices BSD`. Pass the layer through the environment,
never on the command line (it would leak into every sub-make):

    sudo env SUBDIR=Frameworks bmake -m /work/ravynos/BSD/share/mk -j$(nproc)

`-m /work/ravynos/BSD/share/mk` is required. Logs use ANSI colors; strip them
before searching: `sed 's/\x1b\[[0-9;]*m//g' ../build/world.log`.

Outputs, under `../build`:

* `Developer/Platforms/ravynOS.platform/Developer/Toolchains/Default.xctoolchain`
* `Developer/Platforms/ravynOS.platform/Developer/SDKs/ravynOS.sdk`
* `sysroot/` - the root file system
* `kernelcache` - the prelinked kernel and extensions

## Bootable image and boot test

After the build:

    sudo env SUBDIR= bmake -m /work/ravynos/BSD/share/mk image
    sudo env SUBDIR= bmake -m /work/ravynos/BSD/share/mk boottest           # gate M0
    sudo env SUBDIR= GATE=M1 bmake -m /work/ravynos/BSD/share/mk boottest   # gate M1

`image` fetches the EFI loader (ravynsoft/efiloader, pinned in
`Image/Makefile` and patched from `Image/patches/`), builds it with the
toolchain's clang, and writes `../build/ravynOS.img`: a GPT disk with an EFI
system partition (loader, kernelcache, boot plist) and an HFS+ root volume
made from the sysroot. `boottest` boots the image in QEMU with OVMF, follows
the serial console and passes when the gate's markers appear; the console is
saved to `../build/boottest-<gate>.log`. Run the image yourself with the
command `boottest` prints.

## Host tests

Portable parts of the system have host-side tests (clang, ASan/UBSan):

* `make -C Frameworks/OpenGL/tests`
* `make -C Frameworks/CoreGraphics/tests`
* `make -C CoreServices/WindowServer/tests` (needs a built sysroot)
* `python3 -B Developer/Default.xctoolchain/codesign/tests/test_codesign.py`
