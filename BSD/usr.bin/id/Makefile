SUBDIR =
PROG=	id
MAN =
MK_WERROR = no
CFLAGS += -DUSE_BSM_AUDIT -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat -lbsm

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/id
	ln -f ${SYSROOT_DIR}/usr/bin/id ${SYSROOT_DIR}/usr/bin/groups
	ln -f ${SYSROOT_DIR}/usr/bin/id ${SYSROOT_DIR}/usr/bin/whoami

.include <bsd.prog.mk>
