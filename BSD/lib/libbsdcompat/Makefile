# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libbsdcompat: what the FreeBSD commands built on Minkar need from
# FreeBSD that Darwin lacks: capsicum and casper stand-ins
# (include/sys/capsicum.h, capsicum_helpers.h, libcasper.h, casper/: no
# capability mode), sys/mtio.h (no tape drives), strtonum,
# expand_number, copy_file_range, mkfifoat and mknodat, closefrom, strchrnul,
# reallocarray, setproctitle (a no-op), the pwcache(3) name lookups,
# strverscmp, lpathconf, eaccess, sbuf(9) string buffers, FreeBSD's
# kvm_getprocs() family over sysctl and libproc (include/kvm_darwin.h), a
# one-class login_cap (include/login_cap.h), and some
# FreeBSD names (bsdcompat.h, force-included by
# those commands; libbsdcompat.a). Original code, BSD-2.

SUBDIR =
LIB = bsdcompat
SRCS = strtonum.c pipe2.c getlocalbase.c str2sig.c expand_number.c copy_file_range.c mknodat.c acl.c \
	explicit_bzero.c closefrom.c strings.c login_cap.c pwcache.c strverscmp.c sbuf.c \
	kvm_darwin.c
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR}/include -Wno-nullability-completeness

all: lib${LIB}.a

.include <bsd.lib.mk>
