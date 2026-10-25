# route: FreeBSD's route(8), BSD-3, from git history. Routing sockets
# only (no netlink), no jails, one FIB (XNU has no multiple FIBs).

SUBDIR =
MK_WERROR = no
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -Wno-nullability-completeness -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h -I${ROOT_SOURCE_DIR}/BSD/lib/libutil
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libutil -lutil -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat
PROG=	route
SRCS=	route.c keywords.h
WARNS?=	3
CFLAGS+= -DNS
CFLAGS+= -DINET
CFLAGS+= -DINET6
CFLAGS+= -I. -include route_compat.h -I${.CURDIR}
CLEANFILES+= keywords.h

keywords.h: keywords
	LC_ALL=C awk '!/^#|^$$/ { \
		printf "#define\tK_%s\t%d\n\t{\"%s\", K_%s},\n", \
		    toupper($$1), ++L, $$1, toupper($$1); \
	}' < ${.CURDIR}/keywords > ${.TARGET} || (rm -f ${.TARGET}; false)
CFLAGS+=-DWITHOUT_NETLINK

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/sbin/

.include <bsd.prog.mk>
