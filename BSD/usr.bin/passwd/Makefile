# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# passwd: FreeBSD main's usr.bin/passwd, with macOS's options and wording
# (the lines marked Minkar). It changes the password through PAM's
# "passwd" policy; pam_unix (Libraries/OpenPAM) rewrites /etc/master.passwd.
# Set-user-ID root, as on macOS.

SUBDIR =
MK_WERROR = no
PROG =	passwd
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -lpam

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 4555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/bin/passwd
	install -m 0444 ${.CURDIR}/passwd.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
