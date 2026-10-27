SUBDIR =
PROG=	split
MAN =
MK_WERROR = no
CFLAGS += -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h -I${ROOT_SOURCE_DIR}/BSD/lib/libutil
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libutil -lutil

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/split

.include <bsd.prog.mk>
