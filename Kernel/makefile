# ------------------------------------------------------------------------
#  MINKAR BUILD SYSTEM - Kernel and Extensions
# ------------------------------------------------------------------------

AVAILABILITY_PL = ${MINKAR_SDKROOT}/usr/local/libexec/availability.pl
AVAILABILITY_PL_PATH = ${MINKAR_SDKROOT}/usr/local/libexec/availability.pl
SEED_DEFINES = -DRC_ENABLE_XNU_PRODUCT_INFO_FILTER
PATH := ${TOOLCHAIN}/usr/bin:${PATH}

.if "${KERNEL_CONFIGS}" == "DEBUG"
DEBUG = .debug
.else
DEBUG =
.endif

.if "${.MAKE.OS}" ==  "Linux"
HOST_CODESIGN = /bin/true
IIG = /bin/true
HOST_LD ?= /usr/bin/ld
DKPATH = ${.OBJDIR}/xnu/${KERNEL_CONFIGS}_${ARCH_CONFIGS}/iokit/DriverKit

# The DriverKit implementations (iig output, shipped in the tree since iig
# does not run here) too: xnu copies them while compiling them, a race
# with its own jobs. xnu's build starts after this (installhdrs needs it).
DKIMPL = ${.OBJDIR}/xnu/${KERNEL_CONFIGS}_${ARCH_CONFIGS}/iokit/${KERNEL_CONFIGS}/DriverKit

${DKPATH}: .NOTMAIN
	mkdir -p ${DKPATH}
	cp -Rf ${.CURDIR}/xnu/iokit/DriverKit/*.h ${DKPATH}/

# Every time (a directory target would not run again once it exists).
dkimpl: .PHONY .NOTMAIN
	mkdir -p ${DKIMPL}
	cp -f ${.CURDIR}/xnu/iokit/DriverKit/*.iig.cpp ${DKIMPL}/
installhdrs: ${DKPATH} dkimpl
TARGETS = ${DKPATH}
.endif
TARGETS += xnu_all

# xnu's gmake must not see bmake's flags: they are cleared for it alone.
# It runs its commands with bash, as on macOS: its makefiles use bash
# (MakeInc.def computes the kernel's link addresses with $[ ]), and /bin/sh
# is dash on Debian and Ubuntu. With dash the addresses came out as 0 and
# the kernel did not link on the CI.
XNU_SHELL ?= /bin/bash
# (Clearing them for the whole file also took the jobs from the sub-makes,
# so all of Libraries, ICU included, was built one job at a time.)

PLATFORM_DIR=${ROOT_BINARY_DIR}/Developer/Platforms/Minkar.platform

# The SUBDIRs (added to this list by bsd.subdir.mk) after xnu, as before.
all: ${TARGETS} .WAIT
installhdrs: .PHONY
	touch ${ROOT_BINARY_DIR}${.CURDIR}/xnu/.mach_kernel.timestamp
	touch ${ROOT_BINARY_DIR}${.CURDIR}/xnu/.symbolset.timestamp
	MAKEFLAGS= PATH="${PATH}" ${GMAKE} -C ${.CURDIR}/xnu SHELL=${XNU_SHELL} LOGCOLORS=y PLATFORM=MacOSX BUILD_LTO=0 \
		BUILD_PROFILE=0 BUILD_WERROR=0 DO_CTFMERGE=0 \
		SDKVERSION=${MACOS_VERSION_MIN} SRCROOT=${.CURDIR}/xnu \
		OBJROOT=${.OBJDIR}/xnu DSTROOT=${MINKAR_SDKROOT} \
		MIGCOM=${MIGCOM} MIG=${MIG} IIG=${IIG} MIGCC=${CC} \
		CC=${CC} CXX=${CXX} HOST_OS_VERSION=${MACOS_VERSION_MIN} \
		UNIFDEF=${UNIFDEF} CTFCONVERT=${CTFCONVERT} \
		CTFMERGE=${CTFMERGE} CTFINSERT=${CTFINSERT} \
		HOST_SDKROOT=${MINKAR_SDKROOT} VERBOSE=yes \
		AVAILABILITY_PL=${AVAILABILITY_PL} SDKROOT=macosx \
		AVAILABILITY_PL_PATH=${AVAILABILITY_PL_PATH} XCRUN=${XCRUN} \
		KERNEL_CONFIGS=${KERNEL_CONFIGS} ARCH_CONFIGS=${ARCH_CONFIGS} \
		DEVELOPER_DIR=${ROOT_BINARY_DIR}/Developer HOST_FLEX=flex \
		SYMROOT=${ROOT_BINARY_DIR} HOST_CC=${HOST_CC} HOST_BISON=bison \
		HOST_CXX=${HOST_CXX} HOST_LD=${HOST_LD} \
		HOST_CODESIGN_ALLOCATE=/bin/true installhdrs

libfirehose_kernel: .PHONY
	${MAKE} -C ${ROOT_SOURCE_DIR}/Libraries/libfirehose_kernel all

compiler_rt: .PHONY
	${MAKE} -C ${ROOT_SOURCE_DIR}/Libraries all

# In this order, also with -j: Libraries needs the kernel headers, and
# libfirehose_kernel archives Libraries' libplatform, libkernel and libc++.
# The kernel carries its CTF (__CTF), the types dtrace reads: the
# toolchain's ctfconvert, ctfmerge and ctf_insert make it from the dSYM.
KERNEL_CTF =	${.OBJDIR}/xnu/${KERNEL_CONFIGS}_${ARCH_CONFIGS}/kernel${DEBUG}.ctf
xnu_all: installhdrs .WAIT compiler_rt .WAIT libfirehose_kernel
	MAKEFLAGS= PATH="${PATH}" ${GMAKE} -C ${.CURDIR}/xnu SHELL=${XNU_SHELL} LOGCOLORS=y \
		PLATFORM=MacOSX BUILD_LTO=0 BUILD_WERROR=0 DO_CTFMERGE=1 \
		SDKVERSION=${MACOS_VERSION_MIN} SRCROOT=${.CURDIR}/xnu \
		OBJROOT=${.OBJDIR}/xnu DSTROOT=${MINKAR_SDKROOT} \
		SYMROOT=${.OBJDIR}/xnu/sym BUILD_STATIC_LINK=1 \
		MIGCOM=${MIGCOM} MIG=${MIG} IIG=${IIG} CC=${CC} CXX=${CXX} \
		UNIFDEF=${UNIFDEF} CTFCONVERT=${CTFCONVERT} LD="${CC} --ld-path=${LD}" \
		CTFMERGE=${CTFMERGE} CTFINSERT=${CTFINSERT} AR=${AR} \
		NM=${NM} NMEDIT=${NMEDIT} DSYMUTIL=${DSYMUTIL} LIPO=${LIPO} \
	    STRIP=${STRIPBIN} AS=${AS} AVAILABILITY_PL=${AVAILABILITY_PL} \
		MACHINE=${MACHINE} XCRUN=${XCRUN} SDKROOT=macosx \
		HOST_OS_VERSION=${MACOS_VERSION_MIN} VERBOSE=yes \
		AVAILABILITY_PL_PATH=${AVAILABILITY_PL_PATH} \
		DEVELOPER_DIR=${ROOT_BINARY_DIR}/Developer \
		LIBTOOL=${TOOLCHAIN}/usr/bin/libtool VERBOSE=yes \
		KERNEL_CONFIGS=${KERNEL_CONFIGS} ARCH_CONFIGS=${ARCH_CONFIGS} \
		HOST_CC=${HOST_CC} HOST_CXX=${HOST_CXX} HOST_LD=${HOST_LD} all install
.for target_dir in ${MINKAR_SDKROOT}/System/Library/Kernels \
  ${SYSROOT_DIR}/System/Library/Kernels
	mkdir -p ${target_dir} ${target_dir:H}/Extensions
	cp -Rf ${.OBJDIR}/xnu/sym/kernel${DEBUG}.dSYM ${target_dir}/
	cp -f ${.OBJDIR}/xnu/sym/kernel${DEBUG} ${target_dir}/
	# The installed kernel (with all its symbols) gets its CTF from the
	# types xnu converted, merged again against its own symbol table (the
	# CTF maps symbols to types by their index); sym/kernel, which the
	# kernel cache takes, stays as linked.
	if [ -s ${KERNEL_CTF} ]; then rm -f ${target_dir}/kernel${DEBUG}.ctfdata && \
	    ${CTFMERGE} -l xnu -o ${target_dir}/kernel${DEBUG} \
	    -Z ${target_dir}/kernel${DEBUG}.ctfdata ${KERNEL_CTF} && \
	    ${CTFINSERT} ${target_dir}/kernel${DEBUG} -arch x86_64 \
	    ${target_dir}/kernel${DEBUG}.ctfdata -o ${target_dir}/kernel${DEBUG} && \
	    rm -f ${target_dir}/kernel${DEBUG}.ctfdata; fi
	cp -Rf ${.OBJDIR}/xnu/sym/System.kext ${target_dir:H}/Extensions/
.endfor

# NOTE! This kxld build is for the host toolchain. The target kxld is built
# later in IOKitUser
SUBDIR = xnu/libkern/kmod xnu/libkern/kxld
# IOKitUser (IOKit.framework) links CoreFoundation: Frameworks builds it.
# Extensions is built from the top level, after Frameworks.

.include <bsd.subdir.mk>
