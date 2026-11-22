# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# directoryd: users and groups for libinfo's directory module, so that
# sandboxed processes, which may not read /etc, still find them.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
# A Mach-O executable: no .pieo objects and no ELF-style debug files.
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	directoryd
.PATH: ${ROOT_SOURCE_DIR}/Frameworks/OpenDirectory/Core
SRCS =	directoryd.c odapi.c schema.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-incompatible-sysroot -Wno-nullability-completeness -fblocks \
	-I${ROOT_SOURCE_DIR}/Frameworks/OpenDirectory/Core \
	-F${MINKAR_SDKROOT}/System/Library/Frameworks/OpenDirectory.framework/Frameworks \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libpw -include pw_compat.h \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework CoreFoundation \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libpw -lpw \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec
	install -C -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/libexec/${PROG}

.include <bsd.prog.mk>
