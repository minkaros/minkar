# test and [ (FreeBSD, BSD-3). sh has them built in; these are for exec.

SUBDIR =
MK_WERROR = no
MAN =
PROG =	test
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/bin/test
	ln -f ${SYSROOT_DIR}/bin/test ${SYSROOT_DIR}/bin/[

.include <bsd.prog.mk>
