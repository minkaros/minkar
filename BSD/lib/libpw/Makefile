# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libpw: the password-file functions FreeBSD's vipw, pwd_mkdb and chpass
# share: pw_util.c from FreeBSD main's lib/libutil (pw_util(3): lock,
# copy, edit master.passwd and run pwd_mkdb) and pw_scan.c from its
# lib/libc/gen (parse a master.passwd line), with the changes for Darwin
# marked Minkar; pw_compat.h, pw_fields.c and flopen.c are ours and give
# what Darwin lacks (the pwd.db keys and the _PWF_ bits of <pwd.h>, those
# bits kept beside struct passwd, which has no pw_fields, and libutil's
# flopen). Static, internal.

SUBDIR =
LIB =	pw
SRCS =	pw_util.c pw_scan.c pw_fields.c flopen.c
MK_WERROR = no
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR} \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h

all: lib${LIB}.a

.include <bsd.lib.mk>
