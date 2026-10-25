# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
#
# tar: bsdtar from libarchive (BSD-2), sources in BSD/lib/libarchive/src
# (tar/ and libarchive_fe/), linked with that libarchive. tar is a link.

SUBDIR =
MK_WERROR = no
MAN =
PROG =	bsdtar
LA =	${ROOT_SOURCE_DIR}/BSD/lib/libarchive
.PATH: ${LA}/src/tar ${LA}/src/libarchive_fe
SRCS =	bsdtar.c cmdline.c creation_set.c read.c subst.c util.c write.c \
	err.c line_reader.c passphrase.c

CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-DPLATFORM_CONFIG_H=\"${LA}/config_minkar.h\" \
	-DBSDTAR_VERSION_STRING=\"3.8.1\" \
	-I${LA}/src/tar -I${LA}/src/libarchive -I${LA}/src/libarchive_fe \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${LA} -larchive \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libmd -lmd \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat \
	-lz -L${SYSROOT_DIR}/usr/lib -lbz2 -llzma

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/
	ln -f ${SYSROOT_DIR}/usr/bin/bsdtar ${SYSROOT_DIR}/usr/bin/tar

.include <bsd.prog.mk>
