# ------------------------------------------------------------------------
#  MINKAR BUILD SYSTEM - TOP LEVEL
# ------------------------------------------------------------------------

# The Minkar release (what users see) and the API version it is compatible
# with (ProductVersion: what @available, operatingSystemVersion and
# LSMinimumSystemVersion compare against; API 19 is 10.15). The kernel
# stays Darwin 19.6.0 whatever these say.
PROD_VERSION = 19.6.0
PROD_COMPAT_VERSION = 10.15
PROD_FAMILY = Pre Alpha

# Set ONE arch and ONE config
ARCH_CONFIGS = X86_64
KERNEL_CONFIGS = RELEASE

ROOT_SOURCE_DIR = ${.CURDIR}

.if ${MACHINE} == "x86_64" || ${MACHINE} == "amd64"
    BuildArch = X86
    CpuArch = x86_64
.else
    BuildArch = AArch64
    CpuArch = aarch64
.endif
MACHINE_CPUARCH = ${CpuArch}
MK_UNIFIED_OBJDIR = no
MK_AUTO_OBJ = yes

.SYSPATH: ${ROOT_SOURCE_DIR}/BSD/share/mk
.include "./BSD/share/mk/sys.mk"
.include "./BSD/share/mk/src.tools.mk"
.include "./BSD/share/mk/bsd.linker.mk"
.include "./BSD/share/mk/bsd.compiler.mk"

_ROOT_BINARY_DIR = ${ROOT_SOURCE_DIR}/../build
ROOT_BINARY_DIR = ${_ROOT_BINARY_DIR:tA}
SRCTOP = ${ROOT_SOURCE_DIR}
OBJTOP = ${ROOT_BINARY_DIR}
SRCROOT = ${ROOT_SOURCE_DIR}
OBJROOT = ${ROOT_BINARY_DIR}
MAKEOBJDIRPREFIX = ${ROOT_BINARY_DIR}

# ------------------------------------------------------------------------
#  Top level targets
# ------------------------------------------------------------------------

world: version BSD/hier Developer Kernel Libraries/libfirehose_kernel Libraries Frameworks \
	Kernel/Extensions CoreServices BSD SystemLibrary

# ------------------------------------------------------------------------

# Don't warn about Minkar.sdk vs MacOSX.sdk naming
CFLAGS = -Wno-incompatible-sysroot

# If we are building from a different OS, we want to use the host's tools
# to build our toolchain.
.if "${.MAKE.OS}" == "Darwin"
HOST_CC = ${CC}
HOST_CXX = ${CXX}
HOST_AR = ${AR}
.else
# The host compiler must be clang: the toolchain's sources (cctools, the
# reference platform's headers in Default.xctoolchain/include) reject gcc. On Linux,
# /usr/bin/cc is often gcc. Absolute paths: the kernel build puts the
# Minkar toolchain (whose clang targets Darwin) first in PATH.
HOST_CC ?= /usr/bin/clang
HOST_CXX ?= /usr/bin/clang++
HOST_AR ?= /usr/bin/ar
.endif

# If we have a toolchain bundle, default to not building it again
# TOOLCHAIN is initially set to /Library/Developer/... by sys.mk
.if exists(${TOOLCHAIN})
MK_TOOLCHAIN ?= no
.else
MK_TOOLCHAIN ?= yes
.endif

.if ${MK_TOOLCHAIN} == "yes"
TOOLCHAIN = ${ROOT_BINARY_DIR}/Developer/Platforms/Minkar.platform/Developer/Toolchains/Default.xctoolchain
.endif
TOOLS = ${TOOLCHAIN}/usr/bin
DEVEL = ${ROOT_SOURCE_DIR}/Developer

DARWIN_VERSION != head -1 ${ROOT_SOURCE_DIR}/Kernel/xnu/config/MasterVersion

LLVM_VERSION_MAJOR != grep 'set.LLVM_VERSION_MAJOR' Developer/Default.xctoolchain/llvm/cmake/Modules/LLVMVersion.cmake | sed -E 's/^.* ([0-9]+).*$$/\1/'
LLVM_VERSION_MINOR != grep 'set.LLVM_VERSION_MINOR' Developer/Default.xctoolchain/llvm/cmake/Modules/LLVMVersion.cmake | sed -E 's/^.* ([0-9]+).*$$/\1/'
LLVM_VERSION_PATCH != grep 'set.LLVM_VERSION_PATCH' Developer/Default.xctoolchain/llvm/cmake/Modules/LLVMVersion.cmake | sed -E 's/^.* ([0-9]+).*$$/\1/'

LLVM_VERSION = ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}.${LLVM_VERSION_PATCH}
LLVM_MAJOR = ${LLVM_VERSION_MAJOR}
LLVM_MINOR = ${LLVM_VERSION_MINOR}
LLVM_PATCH = ${LLVM_VERSION_PATCH}

PROD_MAJOR != echo ${PROD_VERSION} | sed 's/^([0-9]+\.\*)/\\1/'
PROD_MINOR != echo ${PROD_VERSION} | sed 's/^[0-9]+\.([0-9]+)\./\\1/'
PROD_PATCH != echo ${PROD_VERSION} | sed 's/([0-9]+)$$/\\1/'

DARWIN_MAJOR != echo ${DARWIN_VERSION} | sed 's/^([0-9]+\.\*)/\\1/'
DARWIN_MINOR != echo ${DARWIN_VERSION} | sed 's/^[0-9]+\.([0-9]+)\./\\1/'
DARWIN_PATCH != echo ${DARWIN_VERSION} | sed 's/([0-9]+)$$/\\1/'

MACOSX_DEPLOYMENT_TARGET = 10.15

# Reproducible builds: __DATE__, __TIME__ and the tools that honour
# SOURCE_DATE_EPOCH use the last commit's date, not the build's
# (tools/toolchain/MANIFEST). root's git needs safe.directory here.
SOURCE_DATE_EPOCH != git -c safe.directory='*' -C ${ROOT_SOURCE_DIR} log -1 --format=%ct 2>/dev/null || true
.if empty(SOURCE_DATE_EPOCH)
SOURCE_DATE_EPOCH = 0
.endif
MACOS_VERSION_MIN = 10.15

RUNTIME_SPEC_PATH = ${ROOT_SOURCE_DIR}/Developer/xcbuild/Specifications

XNU_SOURCE_DIR = ${ROOT_SOURCE_DIR}/Kernel/xnu
KEXT_SOURCE_DIR = ${ROOT_SOURCE_DIR}/Kernel/Extensions
SDK_SOURCE_DIR = ${ROOT_SOURCE_DIR}/Developer/Minkar.sdk
PLATFORM_SOURCE_DIR = ${ROOT_SOURCE_DIR}/Developer/Minkar.platform
MINKAR_SDKROOT = ${ROOT_BINARY_DIR}/Developer/Platforms/Minkar.platform/Developer/SDKs/Minkar.sdk
MINKAR_SDKROOT_MACOSX = ${ROOT_BINARY_DIR}/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
SYSROOT_DIR = ${ROOT_BINARY_DIR}/sysroot

TARGET_TRIPLE = ${CpuArch}-apple-darwin${DARWIN_MAJOR}

# BSD/hier lays out the sysroot (/private, /var, /Library...) before anything
# installs into it.
# Kernel/Extensions needs headers from Libraries (dispatch) and Frameworks
# (CoreFoundation), so it is built after them rather than inside Kernel.
# CoreServices (WindowServer) links against the Frameworks.
SUBDIR ?= BSD/hier .WAIT Developer .WAIT Kernel Libraries Frameworks Kernel/Extensions \
	CoreServices BSD SystemLibrary

# 'bmake ONLY_STAGE=Frameworks Frameworks' builds that stage alone, without
# walking (and re-checking) the stages before it first.
.if defined(ONLY_STAGE)
SUBDIR :=	${ONLY_STAGE}
.endif

.export ROOT_SOURCE_DIR ROOT_BINARY_DIR ARCH_CONFIGS KERNEL_CONFIGS \
	PROD_VERSION PROD_COMPAT_VERSION PROD_FAMILY CFLAGS DEVEL DARWIN_VERSION \
	LLVM_VERSION_MAJOR LLVM_VERSION_MINOR LLVM_VERSION_PATCH \
	LLVM_VERSION PROD_MAJOR PROD_MINOR PROD_PATCH DARWIN_MAJOR \
	DARWIN_MINOR DARWIN_PATCH MACOS_VERSION_MIN MACOSX_DEPLOYMENT_TARGET \
	RUNTIME_SPEC_PATH OPSYS MACHINE BuildArch CpuArch XNU_SOURCE_DIR \
	KEXT_SOURCE_DIR SDK_SOURCE_DIR PLATFORM_SOURCE_DIR MINKAR_SDKROOT \
	MINKAR_SDKROOT_MACOSX SYSROOT_DIR TARGET_TRIPLE MAKEOBJDIRPREFIX \
	TOOLCHAIN TOOLS SRCTOP OBJTOP SRCROOT OBJROOT MK_AUTO_OBJ MK_UNIFIED_OBJDIR \
	HOST_CC HOST_CXX HOST_AR SOURCE_DATE_EPOCH

version:
	${MAKE} -C ${.CURDIR}/SystemLibrary SystemVersion.plist

# The progress pages (Docs/Progress.md, Docs/Progress.es.md) and their
# SVGs, from Docs/progress/status.toml. CI checks they are current.
progress: .PHONY
	python3 ${.CURDIR}/tools/progress/gen.py ${.CURDIR}/Docs/progress/status.toml \
	    ${.CURDIR}/Docs/progress ${.CURDIR}/Docs/Progress.md ${.CURDIR}/Docs/Progress.es.md

# Bootable disk image and QEMU boot test (Image/Makefile). Not part of
# "world": they need root, network access for the EFI loader, and time.
efiloader image boottest boottest-key: .PHONY
	${MAKE} -C ${.CURDIR}/Image ${.TARGET}

# ------------------------------------------------------------------------
#  Per-layer targets
# ------------------------------------------------------------------------
# 'bmake <layer>' builds a layer's stages alone, in order (as ONLY_STAGE
# does), on the layers below it already built ('world' builds them all).
# 'bmake <layer>-check' checks what the layer installed, without booting;
# 'layers-check' runs them all, and 'image-check' boots the image to gate
# M2 (after 'image').
#
#   layer       stages                       <layer>-check
#   toolchain   BSD/hier Developer           the toolchain manifest
#   kernel      Kernel Kernel/Extensions     the kernel is a Mach-O and the
#                                            kernelcache well formed; every
#                                            kext a kext bundle
#   libsystem   Libraries                    what /usr/lib loads is there
#   frameworks  Frameworks                   what the frameworks load is
#                                            there; their plists parse
#   userland    BSD                          what the commands load is there
#   sysapps     CoreServices SystemLibrary   what CoreServices loads is
#                                            there; the system's plists parse
LAYERS =		toolchain kernel libsystem frameworks userland sysapps
LAYER_toolchain =	BSD/hier Developer
LAYER_kernel =		Kernel Kernel/Extensions
LAYER_libsystem =	Libraries
LAYER_frameworks =	Frameworks
LAYER_userland =	BSD
LAYER_sysapps =		CoreServices SystemLibrary
SYSROOT_CHECK =		python3 -B ${ROOT_SOURCE_DIR}/tools/check/sysroot-check.py \
			--sysroot ${SYSROOT_DIR} --tools ${TOOLS}

.for l in ${LAYERS}
${l}: .PHONY
.for s in ${LAYER_${l}}
	${MAKE} -m ${ROOT_SOURCE_DIR}/BSD/share/mk ONLY_STAGE=${s} ${s}
.endfor
.endfor

toolchain-check: .PHONY
	sh ${ROOT_SOURCE_DIR}/tools/toolchain/manifest.sh check ${TOOLCHAIN}

kernel-check: .PHONY
	${SYSROOT_CHECK} macho /System/Library/Kernels/kernel
	${SYSROOT_CHECK} kernelcache ${ROOT_BINARY_DIR}/kernelcache
	${SYSROOT_CHECK} kexts /System/Library/Extensions

libsystem-check: .PHONY
	${SYSROOT_CHECK} links /usr/lib

frameworks-check: .PHONY
	${SYSROOT_CHECK} links /System/Library/Frameworks
	${SYSROOT_CHECK} plists /System/Library/Frameworks

userland-check: .PHONY
	${SYSROOT_CHECK} links /bin /sbin /usr/bin /usr/sbin /usr/libexec

sysapps-check: .PHONY
	${SYSROOT_CHECK} links /System/Library/CoreServices
	${SYSROOT_CHECK} plists /System/Library/LaunchDaemons \
	    /System/Library/CoreServices /Library /etc

layers-check: .PHONY ${LAYERS:S/$/-check/}

image-check: .PHONY
	${MAKE} -C ${.CURDIR}/Image boottest GATE=M2

# Gate M3: the tools of Tests/M3, built on the host with their tests and
# installed in the sysroot ('m3-tools', then 'image'); 'm3-check' boots
# the image and runs their tests in it.
m3-tools: .PHONY
	${MAKE} -m ${ROOT_SOURCE_DIR}/BSD/share/mk ONLY_STAGE=Tests/M3 Tests/M3

# Gallery (Tests/Gallery): a development app with every AppKit control and
# a report of what AppKit lacks, for test images only: 'bmake gallery'
# before 'bmake image' puts it in /Applications/Developer.
gallery: .PHONY
	${MAKE} -m ${ROOT_SOURCE_DIR}/BSD/share/mk ONLY_STAGE=Tests/Gallery Tests/Gallery

# The template Atlas's apps start from (Developer/Templates/AtlasApp), for
# test images: 'bmake atlas-app-template' before 'bmake image' puts it in
# /Applications/Developer as Sample.app.
atlas-app-template: .PHONY
	${MAKE} -m ${ROOT_SOURCE_DIR}/BSD/share/mk ONLY_STAGE=Developer/Templates/AtlasApp Developer/Templates/AtlasApp

# SwiftFormat's suite alone takes about half an hour in a VM.
m3-check: .PHONY
	${MAKE} -C ${.CURDIR}/Image boottest GATE=M3 BOOTTEST_TIMEOUT=${M3_BOOTTEST_TIMEOUT:U5400}

# Gate W0: WebKit's dependencies (BSD/lib) pass their own tests: each one
# built again with them into /usr/local/tests/w0 ('w0-tests', then
# 'image'); 'w0-check' boots the image and runs them.
w0-tests: .PHONY
	${MAKE} -m ${ROOT_SOURCE_DIR}/BSD/share/mk ONLY_STAGE=Tests/W0 Tests/W0

# Gate W0 runs on a physical machine running Minkar (W0_HOST=root@address,
# W0_SSH_KEY: the key to log in with): in nested VMs GLib's lock tests do
# not finish. w0-check-vm is the same in boottest's VM, for development.
# Every library of Tests/W0 (a directory with a run.sh; httpd is a tool).
W0_LIBS !=	cd ${.CURDIR}/Tests/W0 && ls */run.sh | sed 's|/run.sh||'
w0-check: .PHONY
.if !defined(W0_HOST)
	@echo "w0-check: W0_HOST=root@address (a physical machine running Minkar) is required" >&2; false
.else
	sh ${.CURDIR}/Tests/W0/runner/check-host.sh ${SYSROOT_DIR} ${W0_HOST} "${W0_SSH_KEY:U}" ${W0_LIBS}
.endif

w0-check-vm: .PHONY
	${MAKE} -C ${.CURDIR}/Image boottest GATE=W0 BOOTTEST_TIMEOUT=${W0_BOOTTEST_TIMEOUT:U5400}

# Gate W1: JavaScriptCore passes JSTests' suites (Tests/W1/check-host.sh,
# WebKit's own runner over ssh) on a machine running Minkar: W1_HOST=
# user@address[:port], W1_SSH_KEY the key to log in with, W1_SUITES some
# suites only (es6.yaml stress ...). The results go to W1_OUT.
W1_OUT ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/w1-results
w1-check: .PHONY
.if !defined(W1_HOST)
	@echo "w1-check: W1_HOST=user@address[:port] (a machine running Minkar) is required" >&2; false
.else
	webkit=$$(sh ${.CURDIR}/BSD/lib/libdrm/fetch.sh ${.CURDIR}/BSD/lib/javascriptcore/MANIFEST \
	    ${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache) && \
	sh ${.CURDIR}/Tests/W1/check-host.sh ${SYSROOT_DIR} $$webkit ${W1_HOST} "${W1_SSH_KEY:U}" \
	    ${W1_OUT} ${W1_SUITES:U}
.endif

# Gate W2: WPE WebKit draws Tests/W2/page.html on a machine running Minkar
# and the image matches the page's reference (Tests/W2/check-host.sh):
# W2_HOST=user@address[:port], W2_SSH_KEY the key to log in with. The
# rendering, the reference and their difference go to W2_OUT.
W2_OUT ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/w2-results
w2-check: .PHONY
.if !defined(W2_HOST)
	@echo "w2-check: W2_HOST=user@address[:port] (a machine running Minkar) is required" >&2; false
.else
	sh ${.CURDIR}/Tests/W2/check-host.sh ${TOOLCHAIN}/usr/bin ${MINKAR_SDKROOT} ${W2_HOST} \
	    "${W2_SSH_KEY:U}" ${W2_OUT}
.endif

.include "./BSD/share/mk/bsd.subdir.mk"
