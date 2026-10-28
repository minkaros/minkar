SUBDIR =
PROG=	kill
MAN =
MK_WERROR = no
# sig2str(3) and str2sig(3), which Darwin lacks, as sh has them: the same
# kill.c is sh's built-in.
.PATH: ${.CURDIR}/../sh
SRCS=	kill.c shims.c
CFLAGS += -I${.CURDIR}/../sh

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/bin/kill

.include <bsd.prog.mk>
