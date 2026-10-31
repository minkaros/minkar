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
# Apple headers in Default.xctoolchain/include) reject gcc. On Linux,
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

LLVM_VERSION_MAJOR != grep 'set.LLVM_VERSION_MAJOR' Developer/Default.xctoolchain/llvm/llvm/CMakeLists.txt | sed -E 's/^.* ([0-9]+).*$$/\1/'
LLVM_VERSION_MINOR != grep 'set.LLVM_VERSION_MINOR' Developer/Default.xctoolchain/llvm/llvm/CMakeLists.txt | sed -E 's/^.* ([0-9]+).*$$/\1/'
LLVM_VERSION_PATCH != grep 'set.LLVM_VERSION_PATCH' Developer/Default.xctoolchain/llvm/llvm/CMakeLists.txt | sed -E 's/^.* ([0-9]+).*$$/\1/'

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

.include "./BSD/share/mk/bsd.subdir.mk"
