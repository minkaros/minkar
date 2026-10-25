SUBDIR = 
LIB = system_platform
SEED_DEFS = -DLONG_BIT=64

DYLIB_LDFLAGS = --sysroot=${MINKAR_SDKROOT} -shared -dylib \
	-Wl,-syslibroot,${MINKAR_SDKROOT}/usr/lib \
        -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD} \
        -Wl,-install_name,/usr/lib/system/libsystem_platform.dylib \
        -Wl,-current_version,220 -Wl,-compatibility_version,1.0.0 \
        -Wl,-search_paths_first -umbrella System \
        -Wl,-force_load,lib${LIB}.a \
        -L${MINKAR_SDKROOT}/usr/lib/system -L${MINKAR_SDKROOT}/usr/lib \
        -lsystem_kernel -ldyld \
        -Wl,-alias_list,${.CURDIR:H}/xcodeconfig/libplatform.aliases

.PATH: ${.CURDIR}/..

all: lib${LIB}.dylib

lib${LIB}.dylib: lib${LIB}.a
	${CC} -o ${.TARGET} ${DYLIB_LDFLAGS}
	mkdir -p ${MINKAR_SDKROOT}/usr/lib/system
	cp -f lib${LIB}.dylib ${MINKAR_SDKROOT}/usr/lib/system/
	cp -f lib${LIB}.dylib ${SYSROOT_DIR}/usr/lib/system/

.include "common.mk"

CFLAGS := ${CFLAGS:S/${.CURDIR}/${.CURDIR:H}/}
ACFLAGS = ${CFLAGS}

.include <bsd.lib.mk>
