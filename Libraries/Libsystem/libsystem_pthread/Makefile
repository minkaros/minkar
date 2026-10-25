LIB = system_pthread

.include "common.mk"

LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wl,-install_name,/usr/lib/system/libsystem_pthread.dylib \
	-Wl,-current_version,416.0.6 -Wl,-compatibility_version,1.0.0 \
	-shared -dylib -rdynamic -nodefaultlibs -Wl,-search_paths_first \
	-all_load -L${MINKAR_SDKROOT}/usr/lib/system -umbrella System \
	-ldyld -lsystem_platform -lsystem_c -lsystem_kernel -lcompiler_rt \
	-Wl,-dirty_data_list,${.CURDIR}/xcodescripts/pthread.dirty \
	-Wl,-alias_list,${.CURDIR}/xcodescripts/pthread.aliases
ACFLAGS = ${CFLAGS}

SUBDIR = dyld

all: lib${LIB}.dylib
lib${LIB}.dylib: ${.OBJDIR}/lib${LIB}.dylib

${.OBJDIR}/lib${LIB}.dylib: dyld lib${LIB}.a
	${CC} -fuse-ld=${LD} -o ${.TARGET} ${LDFLAGS} ${OBJS}
	mkdir -p ${MINKAR_SDKROOT}/usr/lib/system ${SYSROOT_DIR}/usr/lib/system
	cp ${.TARGET} ${MINKAR_SDKROOT}/usr/lib/system/
	cp ${.TARGET} ${SYSROOT_DIR}/usr/lib/system/
	mkdir -p ${MINKAR_SDKROOT}/usr/local/lib/dyld
	cp -f ${.OBJDIR}/dyld/libpthread.a ${MINKAR_SDKROOT}/usr/local/lib/dyld


.include <bsd.lib.mk>
.include <bsd.subdir.mk>
