# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# unzip: bsdunzip from libarchive 3.8.1 (BSD-2), sources in
# BSD/lib/libarchive/src (unzip/ and libarchive_fe/), linked with that
# libarchive; unzip is a link, as on FreeBSD. macOS's unzip is Info-ZIP's;
# bsdunzip takes its usual options.

SUBDIR =
MK_WERROR = no
MAN =
PROG =	bsdunzip
LA =	${ROOT_SOURCE_DIR}/BSD/lib/libarchive
.PATH: ${LA}/src/unzip ${LA}/src/libarchive_fe
SRCS =	bsdunzip.c cmdline.c la_getline.c err.c passphrase.c

CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-DPLATFORM_CONFIG_H=\"${LA}/config_minkar.h\" \
	-DBSDUNZIP_VERSION_STRING=\"3.8.1\" \
	-I${LA}/src/unzip -I${LA}/src/libarchive -I${LA}/src/libarchive_fe \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${LA} -larchive \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libmd -lmd \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat \
	-lz -L${SYSROOT_DIR}/usr/lib -lbz2 -llzma

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/
	ln -f ${SYSROOT_DIR}/usr/bin/bsdunzip ${SYSROOT_DIR}/usr/bin/unzip
	install -m 0444 ${LA}/src/unzip/bsdunzip.1 ${SYSROOT_DIR}/usr/share/man/man1/
	ln -f ${SYSROOT_DIR}/usr/share/man/man1/bsdunzip.1 ${SYSROOT_DIR}/usr/share/man/man1/unzip.1

.include <bsd.prog.mk>
