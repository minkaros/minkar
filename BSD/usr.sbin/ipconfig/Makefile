# ipconfig: the IP configuration, as macOS's ipconfig (architecture plan, Phase 2).

SUBDIR =

PROG =	ipconfig
SRCS =	ipconfig.c
MAN =

CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wall -Wextra -Wno-nullability-completeness
LDFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/sbin
	cp -f ${PROG} ${SYSROOT_DIR}/usr/sbin/

.include <bsd.prog.mk>
