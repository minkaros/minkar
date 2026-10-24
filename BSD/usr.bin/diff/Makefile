.include <src.opts.mk>

# diff: FreeBSD's (ISC; diffreg.c BSD-4 Caldera), from git history.

SUBDIR =
MK_WERROR = no
MAN =
CFLAGS += --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -Wno-nullability-completeness -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h -I${ROOT_SOURCE_DIR}/BSD/lib/libutil
LDFLAGS += --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libutil -lutil -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat
PROG=	diff
SRCS=	diff.c diffdir.c diffreg.c xmalloc.c pr.c diffreg_new.c
# libdiff (Neels Hofmeyr, ISC: libdiff/LICENCE) from FreeBSD's
# contrib/libdiff, built in; its compat/ for recallocarray and
# reallocarray (Darwin exports only reallocarray$DARWIN_EXTSN).
.PATH: ${.CURDIR}/libdiff/lib ${.CURDIR}/libdiff/compat
SRCS+=	diff_atomize_text.c diff_main.c diff_myers.c diff_patience.c \
	diff_output.c diff_output_plain.c diff_output_unidiff.c \
	diff_output_edscript.c recallocarray.c reallocarray.c
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/libdiff/compat/include -I${.CURDIR}/libdiff/include \
	-include sys/param.h -include unistd.h -include time.h	# FreeBSD's headers bring these
LDADD += -lm

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/

.include <bsd.prog.mk>
