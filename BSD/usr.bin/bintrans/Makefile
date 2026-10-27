SUBDIR =
PROG=	bintrans
SRCS=	bintrans.c uuencode.c uudecode.c qp.c
MAN =
MK_WERROR = no
CFLAGS += -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat -lresolv

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/bintrans
	ln -f ${SYSROOT_DIR}/usr/bin/bintrans ${SYSROOT_DIR}/usr/bin/uuencode
	ln -f ${SYSROOT_DIR}/usr/bin/bintrans ${SYSROOT_DIR}/usr/bin/b64encode
	ln -f ${SYSROOT_DIR}/usr/bin/bintrans ${SYSROOT_DIR}/usr/bin/uudecode
	ln -f ${SYSROOT_DIR}/usr/bin/bintrans ${SYSROOT_DIR}/usr/bin/b64decode
	ln -f ${SYSROOT_DIR}/usr/bin/bintrans ${SYSROOT_DIR}/usr/bin/base64

.include <bsd.prog.mk>
