.include <src.opts.mk>

SUBDIR =
MK_WERROR = no
MAN =
CFLAGS += --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -Wno-nullability-completeness -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h -I${ROOT_SOURCE_DIR}/BSD/lib/libutil
LDFLAGS += --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libutil -lutil -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat
PROG=	ping
SRCS=	main.c
CFLAGS+= -DINET
SRCS+=	ping.c utils.c
CFLAGS+= -DINET6 -DKAME_SCOPEID
SRCS+=	ping6.c
LINKS=	${BINDIR}/ping ${BINDIR}/ping6
MLINKS=	ping.8 ping6.8

# ping6 uses MD5 (libmd). No IPsec policies, no Casper.
CFLAGS += -I${ROOT_SOURCE_DIR}/BSD/lib/libmd -D__APPLE_USE_RFC_3542
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libmd -lmd -lm

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin
	install -m 4555 ${PROG} ${SYSROOT_DIR}/sbin/
	ln -f ${SYSROOT_DIR}/sbin/ping ${SYSROOT_DIR}/sbin/ping6

.include <bsd.prog.mk>
