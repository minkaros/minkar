# zlib 1.3.2 (zlib licence), as macOS has it: /usr/lib/libz.1.dylib
# (libz.dylib links to it) and <zlib.h>, <zconf.h> in /usr/include.
# libz.a stays in the SDK for programs that link it statically.

SUBDIR =
MK_WERROR = no
LIB = z
SRCS = adler32.c crc32.c deflate.c infback.c inffast.c inflate.c inftrees.c \
	trees.c zutil.c compress.c uncompr.c gzclose.c gzlib.c gzread.c gzwrite.c

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR} -DHAVE_HIDDEN -D_LARGEFILE64_SOURCE=1
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wl,-install_name,/usr/lib/libz.1.dylib \
	-Wl,-compatibility_version,1.0.0 -Wl,-current_version,1.3.2

MK_MANCOMPRESS = no
MAN =
INCS = zlib.h zconf.h

all: ${.OBJDIR}/libz.1.dylib

${.OBJDIR}/libz.1.dylib: lib${LIB}.a
	${CC} -fuse-ld=${LD} -shared -dylib -o ${.TARGET} \
		-Wl,-force_load,${.OBJDIR}/lib${LIB}.a ${LDFLAGS}
	mkdir -p ${MINKAR_SDKROOT}/usr/include ${MINKAR_SDKROOT}/usr/share/man/man3
	cp -f ${.OBJDIR}/lib${LIB}.a ${MINKAR_SDKROOT}/usr/lib/
	cp -f ${INCS:S,^,${.CURDIR}/,} ${MINKAR_SDKROOT}/usr/include/
	cp -f ${.CURDIR}/zlib.3 ${MINKAR_SDKROOT}/usr/share/man/man3/
.for d in ${MINKAR_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${d}/usr/lib
	cp -f ${.TARGET} ${d}/usr/lib/
	ln -sf libz.1.dylib ${d}/usr/lib/libz.dylib
.endfor
	mkdir -p ${SYSROOT_DIR}/usr/share/man/man3
	cp -f ${.CURDIR}/zlib.3 ${SYSROOT_DIR}/usr/share/man/man3/

.include <bsd.lib.mk>
