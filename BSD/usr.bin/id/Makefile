# id, groups and whoami (FreeBSD's id with Apple's changes). The -A and -F
# options' OpenDirectory code (open_directory.c) is not built: ravynOS has
# no OpenDirectory yet.

SUBDIR =
PROG =	id
SRCS =	id.c
MAN =

MK_WERROR = no
CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness
LDFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	cp -f ${PROG} ${SYSROOT_DIR}/usr/bin/
	ln -sf id ${SYSROOT_DIR}/usr/bin/groups
	ln -sf id ${SYSROOT_DIR}/usr/bin/whoami

.include <bsd.prog.mk>
