SUBDIR =
PROG_CXX=	rs
MAN =
MK_WERROR = no
CFLAGS += -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
CXXFLAGS += -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: rs
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 rs ${SYSROOT_DIR}/usr/bin/rs

.include <bsd.prog.mk>
