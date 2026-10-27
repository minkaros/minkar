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
MAN = src/bzip2.1
MLINKS = src/bzip2.1 bunzip2.1 bzcat.1 bzip2recover.1
MLINKS += src/bzdiff.1 bzcmp.1
MLINKS += src/bzmore.1 bzless.1

all: lib${LIB}.dylib bzip2 bzip2recover bzdiff bzmore

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
