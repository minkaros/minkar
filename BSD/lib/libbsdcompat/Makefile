# libbsdcompat: what the FreeBSD commands built on ravynOS need from
# FreeBSD that Darwin lacks: capsicum and casper stand-ins
# (include/sys/capsicum.h, capsicum_helpers.h, libcasper.h, casper/: no
# capability mode), strtonum, expand_number, copy_file_range, mkfifoat
# and mknodat, and some FreeBSD names (bsdcompat.h, force-included by
# those commands; libbsdcompat.a). Original code, BSD-2.

SUBDIR =
LIB = bsdcompat
SRCS = strtonum.c expand_number.c copy_file_range.c mknodat.c acl.c
CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR}/include -Wno-nullability-completeness

all: lib${LIB}.a

.include <bsd.lib.mk>
