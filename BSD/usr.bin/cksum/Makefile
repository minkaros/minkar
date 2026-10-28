SUBDIR =
PROG=	cksum
SRCS=	cksum.c crc.c print.c sum1.c sum2.c crc32.c
MAN =
MK_WERROR = no
CFLAGS += -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/cksum
	ln -f ${SYSROOT_DIR}/usr/bin/cksum ${SYSROOT_DIR}/usr/bin/sum

.include <bsd.prog.mk>
