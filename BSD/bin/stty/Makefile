# stty (FreeBSD's, BSD-3).

SUBDIR =
MK_WERROR = no
MAN =
PROG=	stty
SRCS=	cchar.c gfmt.c key.c modes.c print.c stty.c util.c
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 ${PROG} ${SYSROOT_DIR}/bin/
	install -m 0444 ${.CURDIR}/stty.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
