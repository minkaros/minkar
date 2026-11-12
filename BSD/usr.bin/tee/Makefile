SUBDIR =
PROG=	tee

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	rsync -cL ${PROG} ${SYSROOT_DIR}/usr/bin


.include <bsd.prog.mk>
