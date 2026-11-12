# pixman 0.43.2 (MIT: COPYING), the pixel compositing library: the new
# window server's software compositor (WindowServer plan, WS2), and later
# the bring-up backend of the Core Animation compositor. Installed as
# /usr/lib/libpixman-1.0.dylib, headers in <pixman-1/pixman.h>. The tests,
# demos and the blue-noise generator are not built.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
LIB = pixman-1
.PATH: ${.CURDIR}/pixman

SRCS=	\
    pixman-linear-gradient.c \
    pixman-matrix.c \
    pixman-edge-accessors.c \
    pixman-combine-float.c \
    pixman-gradient-walker.c \
    pixman-noop.c \
    pixman-filter.c \
    pixman-bits-image.c \
    pixman-radial-gradient.c \
    pixman-fast-path.c \
    pixman-trap.c \
    pixman-access.c \
    pixman-combine32.c \
    pixman-region16.c \
    pixman-region32.c \
    pixman-glyph.c \
    pixman-solid-fill.c \
    pixman-conical-gradient.c \
    pixman-timer.c \
    pixman-utils.c \
    pixman-access-accessors.c \
    pixman-implementation.c \
    pixman-image.c \
    pixman.c \
    pixman-general.c \
    pixman-edge.c \
    pixman-arm.c \
    pixman-mips.c \
    pixman-ppc.c \
    pixman-x86.c

.if ${MACHINE_CPUARCH} == "amd64"
SRCS += pixman-sse2.c pixman-ssse3.c pixman-mmx.c
CFLAGS_ARCH = -DUSE_X86_MMX -DUSE_SSE2 -DUSE_SSSE3 -msse2 -mssse3
.endif

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR}/pixman -I${.OBJDIR} ${CFLAGS_ARCH} -O2 \
	-DHAVE_BUILTIN_CLZ -DHAVE_GCC_VECTOR_EXTENSIONS -DHAVE_GETPAGESIZE \
	-DHAVE_GETTIMEOFDAY -DHAVE_MMAP -DHAVE_MPROTECT -DHAVE_POSIX_MEMALIGN \
	-DHAVE_PTHREADS -DHAVE_SIGACTION -DHAVE_SYS_MMAN_H -DHAVE_UNISTD_H \
	-DUSE_GCC_INLINE_ASM -DTOOLCHAIN_SUPPORTS_ATTRIBUTE_CONSTRUCTOR \
	-DTLS=__thread -DPACKAGE=pixman-1 -DASM_LEADING_UNDERSCORE \
	-DSIZEOF_LONG=8 -Wno-nullability-completeness -Wno-unknown-attributes \
	-Wno-expansion-to-defined

LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wl,-install_name,/usr/lib/libpixman-1.0.dylib \
	-Wl,-current_version,43.2 -Wl,-compatibility_version,43.0 -lSystem

INCS = ${.CURDIR}/pixman/pixman.h ${.OBJDIR}/pixman-version.h
CLEANFILES += pixman-version.h

${SRCS:M*.c:S/.c$/.o/}: pixman-version.h

pixman-version.h: pixman/pixman-version.h.in
	sed -e "s/@PIXMAN_VERSION_MAJOR@/0/" \
	    -e "s/@PIXMAN_VERSION_MINOR@/43/" \
	    -e "s/@PIXMAN_VERSION_MICRO@/2/" \
	    ${.ALLSRC} > ${.TARGET}

all: ${.OBJDIR}/libpixman-1.0.dylib

${.OBJDIR}/libpixman-1.0.dylib: lib${LIB}.a
	${CC} -fuse-ld=${LD} -shared -dylib -o ${.TARGET} \
		-Wl,-force_load,${.OBJDIR}/lib${LIB}.a ${LDFLAGS}
.for d in ${MINKAR_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${d}/usr/lib ${d}/usr/include/pixman-1
	rsync -cL ${.TARGET} ${d}/usr/lib/
	ln -sf libpixman-1.0.dylib ${d}/usr/lib/libpixman-1.dylib
.for h in ${INCS}
	rsync -cL ${h} ${d}/usr/include/pixman-1/
.endfor
.endfor

.include <bsd.lib.mk>
