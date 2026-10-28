MK_WERROR = no
LIB = bz2

MAJOR = 1
MINOR = 0
PATCH = 6

SRCS = ./src/blocksort.c \
	./src/bzlib.c \
	./src/compress.c \
	./src/crctable.c \
	./src/decompress.c \
	./src/huffman.c \
	./src/randtable.c

.if "${CpuArch}" == "aarch64"
SRCS +=	./src/arm64/crc32vec.s
.else if "${CpuArch}" == "x86_64"
SRCS +=	./src/x86_64/crc32vec.s
.else
.error Unsupported CPU architecture ${CpuArch}
.endif

MAJMIN = ${MAJOR}.${MINOR}
VERSION = ${MAJOR}.${MINOR}.${PATCH}

CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -I${.CURDIR}/src -Wno-nullability-completeness
ACFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -Wl,-unexported_symbols_list,${.CURDIR}/unexports -fuse-ld=${LD}
PROG_LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -fuse-ld=${LD} -L${.OBJDIR} -lbz2

SUBDIR =
MAN =

all: lib${LIB}.dylib bzip2 bzip2recover bzdiff bzmore man-pages

# bzip2(1) is also bunzip2, bzcat and bzip2recover; bzdiff(1) bzcmp;
# bzmore(1) bzless. (bzgrep is zgrep, with zgrep(1).)
MAN1DIR = ${SYSROOT_DIR}/usr/share/man/man1
man-pages: .PHONY
	mkdir -p ${MAN1DIR}
.for p in bzip2 bzdiff bzmore
	install -m 0444 ${.CURDIR}/src/${p}.1 ${MAN1DIR}/
.endfor
.for l in bunzip2 bzcat bzip2recover
	ln -f ${MAN1DIR}/bzip2.1 ${MAN1DIR}/${l}.1
.endfor
	ln -f ${MAN1DIR}/bzdiff.1 ${MAN1DIR}/bzcmp.1
	ln -f ${MAN1DIR}/bzmore.1 ${MAN1DIR}/bzless.1

bzip2: ${.CURDIR}/src/bzip2.c lib${LIB}.dylib
	mkdir -p ${SYSROOT_DIR}/usr/bin
	${CC} ${CFLAGS} -o ${.TARGET}.o -c ${.CURDIR}/src/bzip2.c
	${CC} ${PROG_LDFLAGS} -o ${SYSROOT_DIR}/usr/bin/${.TARGET} bzip2.o
	ln -f ${SYSROOT_DIR}/usr/bin/bzip2 ${SYSROOT_DIR}/usr/bin/bunzip2
	ln -f ${SYSROOT_DIR}/usr/bin/bzip2 ${SYSROOT_DIR}/usr/bin/bzcat

bzdiff: ${.CURDIR}/src/bzdiff
	cp -f ${.ALLSRC} ${SYSROOT_DIR}/usr/bin/${.TARGET}
	chmod +x ${SYSROOT_DIR}/usr/bin/bzdiff
	ln -sf bzdiff ${SYSROOT_DIR}/usr/bin/bzcmp

bzmore: ${.CURDIR}/src/bzmore
	cp -f ${.ALLSRC} ${SYSROOT_DIR}/usr/bin/${.TARGET}
	chmod +x ${SYSROOT_DIR}/usr/bin/bzmore
	ln -sf bzmore ${SYSROOT_DIR}/usr/bin/bzless

bzip2recover: ${.CURDIR}/src/bzip2recover.c lib${LIB}.dylib
	${CC} ${CFLAGS} -o ${.TARGET}.o -c ${.CURDIR}/src/bzip2recover.c
	${CC} ${PROG_LDFLAGS} -o ${SYSROOT_DIR}/usr/bin/${.TARGET} \
	    ${.CURDIR}/src/bzip2recover.c

# As on macOS: the library is libbz2.1.0.dylib, which programs record;
# libbz2.dylib is a link to it for -lbz2.
lib${LIB}.dylib: lib${LIB}.a
	${CC} ${LDFLAGS} -o ${.TARGET} -shared -Wl,-force_load,lib${LIB}.a \
          -Wl,-compatibility_version,${MAJMIN}.0 -Wl,-current_version,${VERSION} \
          -Wl,-install_name,/usr/lib/libbz2.${MAJMIN}.dylib
	rm -f ${SYSROOT_DIR}/usr/lib/libbz2.${MAJMIN}.dylib
	cp -f ${.TARGET} ${SYSROOT_DIR}/usr/lib/libbz2.${MAJMIN}.dylib
	ln -fs libbz2.${MAJMIN}.dylib ${SYSROOT_DIR}/usr/lib/libbz2.dylib
	ln -fs libbz2.${MAJMIN}.dylib ${SYSROOT_DIR}/usr/lib/libbz2.${VERSION}.dylib

.include <bsd.lib.mk>
