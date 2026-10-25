# ------------------------------------------------------------------------
#  MINKAR BUILD SYSTEM - Bootable image and boot tests (plan, Phase 0)
# ------------------------------------------------------------------------
#
#   bmake -m BSD/share/mk image       loader + disk image   (needs root)
#   bmake -m BSD/share/mk boottest    boot it in QEMU, gate M0
#   bmake -m BSD/share/mk boottest GATE=M1
#
# Run from the top of the tree after "world"; the top-level Makefile
# forwards these targets here.
#
# The EFI loader is ravynsoft/efiloader (MIT; EDK2 is BSD-2-Clause-Patent),
# fetched at a pinned commit into the build directory and built with the
# Minkar toolchain's clang (EDK2 CLANGDWARF). Set EFILOADER_SRC to use an
# existing checkout.

EFILOADER_REPO ?=	https://github.com/ravynsoft/efiloader.git
EFILOADER_COMMIT ?=	05c194f2a59f582f0ffcf3c68bcd86a982de80c5
# efiloader lacks EDK2's BaseTools/Source/Python/build (its ".gitignore"
# entry "Build/" swallowed it on a case-insensitive disk). Restore it from
# the EDK2 revision the loader's tree matches.
EDK2_RAW ?=		https://raw.githubusercontent.com/tianocore/edk2
EDK2_COMMIT ?=		e0184abff13fccd9df69bb44fa9edbd4062cc99e
EDK2_BUILD_PY =		BuildReport.py __init__.py build.py buildoptions.py

# Our changes on top of the pinned commit (patches/): a fix to send
# upstream, then Minkar's file names.
EFILOADER_PATCHES =	efiloader-0001-macho-16-char-names.patch \
			efiloader-0002-minkar-boot-plist.patch

EFILOADER_SRC ?=	${ROOT_BINARY_DIR}/src/efiloader
EFILOADER_TOOLS =	${ROOT_BINARY_DIR}/src/efiloader-tools
LOADER =		${EFILOADER_SRC}/Build/EmulatorX64/DEBUG_CLANGDWARF/X64/loader.efi

IMAGE ?=		${ROOT_BINARY_DIR}/Minkar.img
IMAGE_SIZE_MB ?=	2048
KERNELCACHE ?=		${ROOT_BINARY_DIR}/kernelcache
BOOT_ARGS ?=		-v debug=0x14e serial=3 keepsyms=1
# PID 1 until launchd runs (gate M1); empty to boot /sbin/launchd itself.
INIT ?=			/sbin/ravyninit
.if !empty(INIT)
INIT_ARGS =		--init ${INIT}
.endif
# ROOT_PASSWORD=...: root's password, for development images (passed to
# mkdiskimage through the environment, not the command line). Without it
# root has no password and nobody can log in on the console.
.if defined(ROOT_PASSWORD)
.export ROOT_PASSWORD
BOOTTEST_PASSWORD ?=	${ROOT_PASSWORD}
.endif
# The password login gates type (GATE=M2-login), by default ROOT_PASSWORD.
.if defined(BOOTTEST_PASSWORD)
.export BOOTTEST_PASSWORD
.endif
# ROOT_AUTHORIZED_KEYS=file: public keys root can log in with over ssh.
# BOOTTEST_SSH_KEY=file: the private key the M2 gate's ssh steps use.
# "bmake boottest-key" makes a pair for the tests in ${BOOTTEST_KEY}.
BOOTTEST_KEY =		${ROOT_BINARY_DIR}/boottest-ssh/id_ed25519
.if defined(ROOT_AUTHORIZED_KEYS)
.export ROOT_AUTHORIZED_KEYS
.endif
.if defined(BOOTTEST_SSH_KEY)
.export BOOTTEST_SSH_KEY
.endif
# TEST_USER=name TEST_USER_PASSWORD=...: a login account for testing (uid
# 501, group staff, admin), for development images.
.if defined(TEST_USER)
.export TEST_USER TEST_USER_PASSWORD
.endif
# SSH_ROOT_LOGIN=yes: root may log in over ssh, also with its password. On
# by default for development images until version 1.0; "no" turns it off.
SSH_ROOT_LOGIN ?=	yes
.if ${SSH_ROOT_LOGIN} == "yes"
INIT_ARGS +=		--ssh-root-login
.endif
# LAUNCHD_DEBUG=1: launchd logs at debug level to the console.
.if !empty(LAUNCHD_DEBUG)
INIT_ARGS +=		--launchd-debug
.endif
GATE ?=			M0
BOOTTEST_TIMEOUT ?=	900

PYTHON ?=		python3 -B

all: image

efiloader: ${LOADER}

${EFILOADER_SRC}:
	git clone ${EFILOADER_REPO} ${.TARGET}
	git -C ${.TARGET} checkout -q ${EFILOADER_COMMIT}

${LOADER}: ${EFILOADER_SRC} ${EFILOADER_PATCHES:S,^,${.CURDIR}/patches/,}
	@test "$$(git -C ${EFILOADER_SRC} rev-parse HEAD)" = "${EFILOADER_COMMIT}" || \
	    { echo "${EFILOADER_SRC} is not at ${EFILOADER_COMMIT}"; exit 1; }
.for p in ${EFILOADER_PATCHES}
	git -C ${EFILOADER_SRC} apply --reverse --check ${.CURDIR}/patches/${p} 2>/dev/null || \
	    git -C ${EFILOADER_SRC} apply ${.CURDIR}/patches/${p}
.endfor
	mkdir -p ${EFILOADER_SRC}/BaseTools/Source/Python/build
.for f in ${EDK2_BUILD_PY}
	test -s ${EFILOADER_SRC}/BaseTools/Source/Python/build/${f} || \
	    curl -sfL -o ${EFILOADER_SRC}/BaseTools/Source/Python/build/${f} \
	    ${EDK2_RAW}/${EDK2_COMMIT}/BaseTools/Source/Python/build/${f}
.endfor
	test -x ${EFILOADER_SRC}/BaseTools/Source/C/bin/GenFw || \
	    MAKEFLAGS="" CC=${HOST_CC} CXX=${HOST_CXX} ${TOOLS}/gmake \
	    -C ${EFILOADER_SRC}/BaseTools/Source/C
	# clang wrapper: the SDK, not the Linux host, provides system headers.
	mkdir -p ${EFILOADER_TOOLS}
	printf '#!/bin/sh\nexec %s/clang --sysroot=%s "$$@"\n' ${TOOLS} \
	    ${MINKAR_SDKROOT} > ${EFILOADER_TOOLS}/clang
	chmod 755 ${EFILOADER_TOOLS}/clang
.for t in llvm-ar ld.lld lld llvm-objcopy
	ln -sf ${TOOLS}/${t} ${EFILOADER_TOOLS}/${t}
.endfor
	cd ${EFILOADER_SRC} && env -u WORKSPACE -u EDK_TOOLS_PATH -u MAKEFLAGS \
	    CLANG_BIN=${EFILOADER_TOOLS}/ \
	    CPATH=${MINKAR_SDKROOT}/System/Library/Frameworks/Kernel.framework/Versions/A/Headers:${MINKAR_SDKROOT}/usr/include \
	    bash -c '. ./edksetup.sh >/dev/null && build -t CLANGDWARF -a X64 \
	        -p EmulatorPkg/EmulatorPkg.dsc \
	        -m MdeModulePkg/Application/Loader/Loader.inf'

image: ${LOADER}
	${PYTHON} ${.CURDIR}/mkdiskimage.py --sysroot ${SYSROOT_DIR} \
	    --kernelcache ${KERNELCACHE} --loader ${LOADER} --output ${IMAGE} \
	    --size-mb ${IMAGE_SIZE_MB} --boot-args "${BOOT_ARGS}" \
	    ${INIT_ARGS}

boottest-key:
	test -f ${BOOTTEST_KEY} || { mkdir -p ${BOOTTEST_KEY:H} && \
	    ssh-keygen -q -t ed25519 -N '' -C minkar-boottest -f ${BOOTTEST_KEY}; }
	@echo "image:    ROOT_AUTHORIZED_KEYS=${BOOTTEST_KEY}.pub"
	@echo "boottest: BOOTTEST_SSH_KEY=${BOOTTEST_KEY}"

boottest:
	${PYTHON} ${.CURDIR}/boottest.py --image ${IMAGE} --gate ${GATE} \
	    --timeout ${BOOTTEST_TIMEOUT} --log ${ROOT_BINARY_DIR}/boottest-${GATE}.log

.PHONY: all efiloader image boottest boottest-key
