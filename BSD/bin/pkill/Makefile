SUBDIR =
PROG=	pkill
MAN =
MK_WERROR = no
CFLAGS += -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

# macOS has both in /usr/bin.
to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/pkill
	ln -f ${SYSROOT_DIR}/usr/bin/pkill ${SYSROOT_DIR}/usr/bin/pgrep

.include <bsd.prog.mk>
