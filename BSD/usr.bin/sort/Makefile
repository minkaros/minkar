.include <src.opts.mk>

SUBDIR =
MK_WERROR = no
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -Wno-nullability-completeness -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h -I${ROOT_SOURCE_DIR}/BSD/lib/libutil
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libutil -lutil -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat
PROG=	sort
SRCS=	bwstring.c coll.c file.c mem.c radixsort.c sort.c vsort.c

# MD5 for the random sort, from our libmd.
CFLAGS += -I${ROOT_SOURCE_DIR}/BSD/lib/libmd
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libmd -lmd

all: to-sysroot

# The page: built without SORT_THREADS, so its lines about threads are
# commented out, as FreeBSD does.
sort.1: ${.CURDIR}/sort.1.in
	sed -e 's|%%THREADS%%|.\\"|g' ${.ALLSRC} > ${.TARGET}
CLEANFILES += sort.1

to-sysroot: ${PROG} sort.1
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/
	install -m 0444 sort.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.prog.mk>
