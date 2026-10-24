# traceroute: our own IPv4 traceroute (BSD-2). setuid root for the raw
# ICMP socket; it drops root right after opening it.

SUBDIR =
MK_WERROR = no
MAN =
PROG =	traceroute
SRCS =	traceroute.c
CFLAGS += --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h
LDFLAGS += --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin
	install -m 4555 ${PROG} ${SYSROOT_DIR}/usr/sbin/

.include <bsd.prog.mk>
