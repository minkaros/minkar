# ravyninit: temporary PID 1 for bring-up (architecture plan, gate M1).
# Image/Makefile installs it as /sbin/launchd until launchd runs.

SUBDIR =

PROG =	ravyninit
SRCS =	ravyninit.c
MAN =

CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wall -Wextra -Wno-nullability-completeness
LDFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin
	cp -f ${PROG} ${SYSROOT_DIR}/sbin/

.include <bsd.prog.mk>
