SUBDIR = 
PROG = apply

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/bin
	rsync -cL ${PROG} ${SYSROOT_DIR}/bin/

.include <bsd.prog.mk>
