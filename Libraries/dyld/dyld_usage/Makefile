MK_WERROR = no
PROG = dyld_usage
.PATH: ${.CURDIR:H}
SRCS =  src/dyld_usage.cpp
MAN = ${.CURDIR:H}/doc/man/man1/dyld_usage.1

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -I${ROOT_SOURCE_DIR}/BSD/lib/libutil \
        -I${ROOT_SOURCE_DIR}/PrivateFrameworks/ktrace \
        -I${.CURDIR:H}/dyld3

LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -fuse-ld=${LD} -L${ROOT_SOURCE_DIR}/BSD/lib/libutil/libutil.a -lc++

.include <bsd.prog.mk>

