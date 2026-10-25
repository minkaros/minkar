# Building Minkar From Source

Minkar is built with BSD make (bmake). The build first creates a host
toolchain (Default.xctoolchain: clang/LLVM, Apple cctools, ld64, xcbuild,
mig, ...) and then uses it, together with the ravynOS.sdk it produces, to
build the rest of the system for the same CPU architecture.

The source tree is `/work/minkar` and the build directory is `../build`
next to it (`ROOT_BINARY_DIR`); adjust the paths below to your checkout.
Building LLVM the first time takes a __long__ time.

## Host requirements

Tested on Ubuntu 22.04 (x86-64). Package names are Debian/Ubuntu ones.

* Toolchain build: `clang lld cmake ninja-build python3 git curl`,
  `libxml2-dev libpng-dev uuid-dev libssl-dev zlib1g-dev libbsd-dev`
* System build: `bmake make csh bison flex`, and `byacc` (Berkeley yacc,
  public domain: libxkbcommon's parser is generated with it, not bison)
* Bootable image and boot test: `nasm gdisk dosfstools hfsprogs`,
  `qemu-system-x86 ovmf` (QEMU uses KVM when /dev/kvm exists, TCG otherwise),
  and `openssh-client` for gate M2

Symlink `cc` and `c++` to clang to avoid accidental use of gcc.

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

* `Developer/Platforms/ravynOS.platform/Developer/Toolchains/Default.xctoolchain`
* `Developer/Platforms/ravynOS.platform/Developer/SDKs/ravynOS.sdk`
* `sysroot/` - the root file system
* `kernelcache` - the prelinked kernel and extensions

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
* M2 - M2-login, then the network: the e1000e NIC (RavynE1000 on our
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

Until launchd runs, PID 1 is `ravyninit` (`BSD/sbin/ravyninit`), a small
temporary init that the image installs as `/sbin/launchd`, the only path a
RELEASE kernel tries. Build the image with `INIT=` to boot the sysroot's own
`/sbin/launchd` instead.

## Host tests

Portable parts of the system have host-side tests (clang, ASan/UBSan):

* `make -C Frameworks/OpenGL/tests`
* `make -C Frameworks/CoreGraphics/tests`
* `make -C CoreServices/WindowServer/tests` (needs a built sysroot)
* `python3 -B Developer/Default.xctoolchain/codesign/tests/test_codesign.py`
* `make -C Libraries/Libsystem/libsystem_c/ravyn_crypt/tests` (crypt(3) SHA-crypt)
* `cc -DTEST -I Libraries/Libsystem/libsystem_coreservices -o /tmp/sysdir_test Libraries/Libsystem/libsystem_coreservices/sysdir.c && /tmp/sysdir_test`
