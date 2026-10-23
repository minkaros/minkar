# dhcpclient: the DHCP client daemon, started by launchd (architecture plan, Phase 2).

SUBDIR =

PROG =	dhcpclient
SRCS =	dhcpclient.c
MAN =

CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wall -Wextra -Wno-nullability-completeness \
	-I${RAVYN_SDKROOT}/System/Library/Frameworks/System.framework/PrivateHeaders
LDFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec
	cp -f ${PROG} ${SYSROOT_DIR}/usr/libexec/

.include <bsd.prog.mk>
