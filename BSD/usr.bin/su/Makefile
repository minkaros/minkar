# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
#
# su (FreeBSD, BSD-3) from git history: PAM (/etc/pam.d/su), BSM audit
# records, and libbsdcompat's one-class login_cap. Set-user-ID root.

SUBDIR =
MK_WERROR = no
MAN =
PROG =	su
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-DUSE_BSM_AUDIT -Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD = -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat \
	-lpam -lbsm

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -o 0 -g 0 -m 4555 ${PROG} ${SYSROOT_DIR}/usr/bin/

.include <bsd.prog.mk>
