# OpenBSM's tools (BSD), built for ravynOS with OpenBSM's own sources.

SUBDIR =
MK_WERROR = no
MK_PIE = no
OPENBSM = ${.CURDIR:H:H}
BINDIR ?= /usr/sbin

.PATH: ${.OBJDIR}

CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.OBJDIR} -I${.OBJDIR}/include -I${OPENBSM} \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/liblaunch \
	-Wno-cast-align -Wno-cast-qual -Wno-sign-compare -Wno-shadow
LDFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD ?= -lbsm

MIGFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}

${.OBJDIR}/include/config/config.h: ${OPENBSM}/config/config_ravynos.h
	mkdir -p ${.TARGET:H}
	cp -f ${.ALLSRC} ${.TARGET}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${BINDIR}
	install -m 0555 ${PROG} ${SYSROOT_DIR}${BINDIR}/
