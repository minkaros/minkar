SUBDIR =
MK_WERROR = no
PROG = grep
MAN = grep.1 
SRCS = file.c grep.c queue.c util.c

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -DWITHOUT_LZMA -DWITHOUT_FASTMATCH \
	-DWITHOUT_NLS -I${ROOT_SOURCE_DIR}/Libraries/zlib \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h \
        -I${ROOT_SOURCE_DIR}/BSD/lib/bzip2/src
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD} -L${SYSROOT_DIR}/usr/lib -lbz2 -lz \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	cp -f ${PROG} ${SYSROOT_DIR}/usr/bin/
	ln -f ${SYSROOT_DIR}/usr/bin/grep ${SYSROOT_DIR}/usr/bin/egrep
	ln -f ${SYSROOT_DIR}/usr/bin/grep ${SYSROOT_DIR}/usr/bin/fgrep
	# grep reads no compressed files itself (FreeBSD main): zgrep and
	# bzgrep are the script that runs zcat or bzcat into it.
	install -m 0555 ${.CURDIR}/zgrep.sh ${SYSROOT_DIR}/usr/bin/zgrep
.for l in zegrep zfgrep bzgrep bzegrep bzfgrep
	ln -f ${SYSROOT_DIR}/usr/bin/zgrep ${SYSROOT_DIR}/usr/bin/${l}
.endfor

.include <bsd.prog.mk>
