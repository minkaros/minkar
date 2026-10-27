SUBDIR =
PROG=	m4
SRCS=	eval.c expr.c look.c main.c misc.c gnum4.c trace.c parser.y \
	tokenizer.l ohash.c
MAN =
MK_WERROR = no
NO_WMISSING_VARIABLE_DECLARATIONS=
.PATH:	${.CURDIR}/ohash
CFLAGS += -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h \
	-DEXTENDED -I${.CURDIR} -I${.CURDIR}/ohash -I${.OBJDIR}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat -lm

# The parser uses exit() without <stdlib.h>, which byacc's skeleton
# includes and bison's does not.
CFLAGS.parser.c = -include stdlib.h

tokenizer.o: parser.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/

.include <bsd.prog.mk>
