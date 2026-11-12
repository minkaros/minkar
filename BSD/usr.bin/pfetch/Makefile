SUBDIR = 
MK_WERROR = no
PROG = pfetch
SRCS =
MAN =

all: ${PROG}

${PROG}: ${.CURDIR}/${PROG}
	rsync -cL ${.CURDIR}/${PROG} ${SYSROOT_DIR}/usr/bin/${PROG}
	chmod +x ${SYSROOT_DIR}/usr/bin/${PROG}


