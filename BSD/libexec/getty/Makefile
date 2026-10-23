# getty: opens a terminal line and runs login(1) on it; launchd starts one
# per console or terminal (architecture plan, Phase 2, console login).

SUBDIR =
MK_WERROR = no

PROG =	getty
SRCS =	main.c init.c subr.c chat.c
MAN =	gettytab.5 ttys.5 getty.8

LIBUTIL = ${ROOT_SOURCE_DIR}/BSD/lib/libutil
CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${LIBUTIL}
LDFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD} -L${ROOT_BINARY_DIR}${LIBUTIL} -lutil

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/libexec ${SYSROOT_DIR}/private/etc
	cp -f ${PROG} ${SYSROOT_DIR}/usr/libexec/
	cp -f ${.CURDIR}/gettytab ${SYSROOT_DIR}/private/etc/

.include <bsd.prog.mk>
