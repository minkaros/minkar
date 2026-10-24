# id, groups and whoami (FreeBSD's id with Apple's changes). -A prints the
# audit session (getaudit_addr); -F the full name from the passwd entry.
# open_directory.c is not used by id.c.

SUBDIR =
PROG =	id
SRCS =	id.c
MAN =

MK_WERROR = no
CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-DUSE_BSM_AUDIT -Wno-nullability-completeness
LDFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	cp -f ${PROG} ${SYSROOT_DIR}/usr/bin/
	ln -sf id ${SYSROOT_DIR}/usr/bin/groups
	ln -sf id ${SYSROOT_DIR}/usr/bin/whoami

.include <bsd.prog.mk>
