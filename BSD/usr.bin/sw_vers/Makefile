SUBDIR =
PROG = sw_vers
SRCS = sw_vers.c
MAN =
MK_WERROR = no

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${ROOT_SOURCE_DIR}/Frameworks \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern

LDFLAGS = -framework CoreFoundation -Wl,-platform_version -Wl,macos \
	  -Wl,${MACOS_VERSION_MIN} -Wl,${MACOS_VERSION_MIN} \
	  -Wl,-syslibroot,${MINKAR_SDKROOT}/usr/lib

all: ${PROG} copyprog

${.OBJDIR}/${PROG}:
	mkdir -p ${.TARGET:H}

${PROG}: ${.OBJDIR}/${PROG}

install: ${PROG}

copyprog: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	cp -f ${.OBJDIR}/${PROG} ${SYSROOT_DIR}/usr/bin/
	install -m 0444 ${.CURDIR}/sw_vers.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
