SUBDIR =
PROG=	xinstall
SRCS=	xinstall.c getid.c
MAN =
MK_WERROR = no
CFLAGS += -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h -I${.CURDIR} -I${ROOT_SOURCE_DIR}/BSD/lib/libmd -DWITH_MD5
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libmd -lmd

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/install

.include <bsd.prog.mk>
