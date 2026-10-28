# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# file(1) 5.46 (BSD-2) on libmagic; its database is
# /usr/share/file/magic.mgc (BSD/lib/libmagic).

SUBDIR =
MK_WERROR = no
MAN =
.PATH: ${ROOT_SOURCE_DIR}/BSD/lib/libmagic/src
PROG = file
SRCS = file.c
MAGICLIB = ${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libmagic

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-DHAVE_CONFIG_H -DMAGIC='"/usr/share/file/magic"' \
	-I${MAGICLIB} -I${ROOT_SOURCE_DIR}/BSD/lib/libmagic \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libmagic/src -Wno-nullability-completeness
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD = ${MAGICLIB}/libmagic.a ${MINKAR_SDKROOT}/usr/lib/libz.a

all: to-sysroot

# file(1) and magic(5), from libmagic's doc/ with their sections, version
# and database filled in.
MAGICDOC = ${ROOT_SOURCE_DIR}/BSD/lib/libmagic/doc
MAN_SED = -e 's|__CSECTION__|1|g' -e 's|__FSECTION__|5|g' \
	-e 's|__VERSION__|5.46|g' -e 's|__MAGIC__|/usr/share/file/magic|g'
file.1: ${MAGICDOC}/file.man
	sed ${MAN_SED} ${.ALLSRC} > ${.TARGET}
magic.5: ${MAGICDOC}/magic.man
	sed ${MAN_SED} ${.ALLSRC} > ${.TARGET}
CLEANFILES += file.1 magic.5

to-sysroot: ${PROG} file.1 magic.5
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1 \
	    ${SYSROOT_DIR}/usr/share/man/man5
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/
	install -m 0444 file.1 ${SYSROOT_DIR}/usr/share/man/man1/
	install -m 0444 magic.5 ${SYSROOT_DIR}/usr/share/man/man5/

.include <bsd.prog.mk>
