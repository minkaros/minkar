SUBDIR =
MK_WERROR = no
LIB = system_trace
SRCS = log.c signpost.c init.c activity.c format.c

SLF = /System/Library/Frameworks
CFLAGS = -mmacos-version-min=${MACOS_VERSION_MIN} --sysroot=${MINKAR_SDKROOT} \
	-ObjC -x objective-c -fblocks \
	-DNUL=\'\0\' -Wno-nullability-completeness \
	-I${MINKAR_SDKROOT}/usr/local/include/kernel \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_SOURCE_DIR}/Frameworks \
	-I${MINKAR_SDKROOT}/usr/include \
	-I${MINKAR_SDKROOT}${SLF}/System.framework/PrivateHeaders


LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wl,-install_name,/usr/lib/system/libsystem_trace.dylib \
	-Wl,-current_version,1147.0.3 -Wl,-compatibility_version,1 \
	-L${MINKAR_SDKROOT}/usr/lib/system -L${MINKAR_SDKROOT}/usr/lib \
	-umbrella System -Wl,-upward-lobjc -lsystem_c -lsystem_platform \
        -ldispatch -ldyld

all: ${.OBJDIR}/lib${LIB}.dylib

${.OBJDIR}/lib${LIB}.dylib: lib${LIB}.a
	${CC} -fuse-ld=${LD} -shared -dylib -o ${.TARGET} \
	  -Wl,-force_load,${.OBJDIR}/lib${LIB}.a ${LDFLAGS}
	mkdir -p ${MINKAR_SDKROOT}/usr/lib/system ${SYSROOT_DIR}/usr/lib/system
	cp -f ${.TARGET} ${MINKAR_SDKROOT}/usr/lib/system
	cp -f ${.TARGET} ${SYSROOT_DIR}/usr/lib/system
	mkdir -p ${MINKAR_SDKROOT}/usr/include/os
	# The public os/log.h is xnu's (os_log, os_log_error...); this
	# library's own log.h only adds to it, for its own build.
	cp -f ${ROOT_SOURCE_DIR}/Kernel/xnu/libkern/os/log.h ${MINKAR_SDKROOT}/usr/include/os
	cp -f ${.CURDIR}/activity.h ${MINKAR_SDKROOT}/usr/include/os

.include <bsd.lib.mk>
