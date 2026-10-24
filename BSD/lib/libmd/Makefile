# libmd for the FreeBSD commands (sort, md5): SHA-1 (lib/libmd/sha1c.c,
# BSD-3) and SHA-2 (sys/crypto/sha2, BSD-2) from FreeBSD's git history,
# and an original MD5 (RFC 1321) and digest helpers (End, File, Data, ...)
# with FreeBSD libmd's interface. FreeBSD's MD4, MD5, RIPEMD-160 and
# helpers are not used: their licenses are not BSD. Static library.

SUBDIR =
MK_WERROR = no
LIB = md
SRCS = md5c.c sha1c.c sha256c.c sha512c.c mdhl.c

CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR} -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h \
	-Wno-nullability-completeness

all: lib${LIB}.a

.include <bsd.lib.mk>
