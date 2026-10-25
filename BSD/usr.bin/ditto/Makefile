# ditto: our own (BSD-2): copies through copyfile(3), CPIO and PKZip
# archives through libarchive (BSD/lib/libarchive).

SUBDIR =
MK_WERROR = no
MAN =
PROG =	ditto
SRCS =	ditto.c
LA =	${ROOT_SOURCE_DIR}/BSD/lib/libarchive
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${LA}/src/libarchive
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD = -L${ROOT_BINARY_DIR}${LA} -larchive \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libmd -lmd \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat \
	-lz -L${SYSROOT_DIR}/usr/lib -lbz2 -llzma

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/

.include <bsd.prog.mk>
