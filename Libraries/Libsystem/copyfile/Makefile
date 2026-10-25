LIB = copyfile
SUBDIR =
SRCS = copyfile.c xattr_flags.c
MK_WERROR = no

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -D"VERSION_INFO_PREFIX=__attribute__((visibility(\"hidden\")))" \
        -DNO_QUARANTINE # until quarantine.h implemented
CFLAGS += -I${.CURDIR}/../liblaunch -I${MINKAR_SDKROOT}/usr/include \
        -I${MINKAR_SDKROOT}/usr/local/include/kernel \
        -I${.CURDIR}/../libsystem_info/membership.subproj

# same version as removefile
LDFLAGS = -Wl,-syslibroot,${MINKAR_SDKROOT}/usr/lib -L${MINKAR_SDKROOT}/usr/lib \
        -L${MINKAR_SDKROOT}/usr/lib/system -Wl,-current_version,48 \
        -Wl,-install_name,/usr/lib/system/libcopyfile.dylib -umbrella System \
        -Wl,-compatibility_version,1 -mmacos-version-min=${MACOS_VERSION_MIN} \
        -lsystem_info -lsystem_asl -lsystem_kernel -lsystem_c -lsystem_malloc \
        -ldispatch -lsystem_blocks -lxpc -ldyld -fuse-ld=${LD}

all: ${.OBJDIR}/lib${LIB}.dylib

${.OBJDIR}/lib${LIB}.dylib: lib${LIB}.a
	${CC} -shared -Wl,-dylib -o ${.TARGET} ${OBJS} ${LDFLAGS}
	mkdir -p ${SYSROOT_DIR}/usr/lib/system ${MINKAR_SDKROOT}/usr/lib/system
	cp -f ${.TARGET} ${MINKAR_SDKROOT}/usr/lib/system/
	cp -f ${.TARGET} ${SYSROOT_DIR}/usr/lib/system/
	cd ${.CURDIR} && env PLATFORM=MacOSX DSTROOT=${MINKAR_SDKROOT} \
          bash ${.CURDIR}/xcodescripts/install_files.sh

.include <bsd.lib.mk>
