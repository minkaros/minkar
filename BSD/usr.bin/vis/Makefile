SUBDIR =
PROG=	vis
SRCS=	vis.c foldit.c libc_vis.c
MAN =
MK_WERROR = no
CFLAGS += -I${ROOT_SOURCE_DIR}/BSD/usr.bin/vis/libc-vis -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/vis

.include <bsd.prog.mk>
