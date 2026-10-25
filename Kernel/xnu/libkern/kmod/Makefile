MK_WERROR = no
SUBDIR =
LIB = kmod
SRCS = c_start.c c_stop.c pure.c

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${MINKAR_SDKROOT}/usr/local/include/kernel \
	-I${ROOT_SOURCE_DIR}/Frameworks

LIBDIR = ${MINKAR_SDKROOT}/usr/local/lib/kernel

all: copyfile

copyfile: lib${LIB}.a
	mkdir -p ${LIBDIR}
	cp -f ${.OBJDIR}/lib${LIB}.a ${LIBDIR}

.include <bsd.lib.mk>
