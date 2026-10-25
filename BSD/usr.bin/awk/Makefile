# awk: the One True AWK (Lucent/AT&T license, LICENSE), from FreeBSD's
# contrib/one-true-awk in git history. awkgram.y needs the host's byacc;
# maketab runs on the host to make proctab.c. nawk is a link.

SUBDIR =
MK_WERROR = no
MAN =
PROG =	awk
SRCS =	awkgram.c b.c lex.c lib.c main.c parse.c proctab.c run.c tran.c

CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.OBJDIR} -I${.CURDIR} \
	-DHAS_ISBLANK -DFOPEN_MAX=64
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD += -lm

HOST_CC ?= cc

awkgram.c awkgram.h: ${.CURDIR}/awkgram.y
	byacc -d -o awkgram.c ${.CURDIR}/awkgram.y

awkgram.tab.h: awkgram.h
	ln -sf awkgram.h ${.TARGET}

maketab: ${.CURDIR}/maketab.c awkgram.tab.h
	${HOST_CC} -I${.OBJDIR} -I${.CURDIR} -o ${.TARGET} ${.CURDIR}/maketab.c

proctab.c: maketab
	./maketab awkgram.tab.h > ${.TARGET}

CLEANFILES += awkgram.c awkgram.h awkgram.tab.h maketab proctab.c

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/
	ln -f ${SYSROOT_DIR}/usr/bin/awk ${SYSROOT_DIR}/usr/bin/nawk

.include <bsd.prog.mk>

${OBJS}: awkgram.h awkgram.tab.h
