SUBDIR =
MK_WERROR = no

PROG = fsck_hfs
SRCS =  cache.c \
        fsck_debug.c \
        fsck_hfs.c \
        fsck_hfs_strings.c \
        fsck_messages.c \
        fsck_strings.c \
        utilities.c
MAN = fsck_hfs.8

PROJ_VER = @(\#)PROGRAM:fsck_hfs  PROJECT:hfs-556~41\n
LIBUTIL = ${ROOT_SOURCE_DIR}/BSD/lib/libutil
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -I${.CURDIR} -I${.CURDIR:H}/mount_hfs/include -I${LIBUTIL} \
        '-DPROJ_VER="${PROJ_VER}"'
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -fuse-ld=${LD} -L${ROOT_BINARY_DIR}${LIBUTIL} -lutil \
        -L${.OBJDIR}/dfalib -ldfa -framework CoreFoundation -framework IOKit

SUBDIR = dfalib

all: all_subdir_BSD/sbin/fsck_hfs/dfalib to-sysroot
# The link happens in ${PROG}.full (debug files on), which needs libdfa.
${PROG} ${PROG}.full: all_subdir_BSD/sbin/fsck_hfs/dfalib

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin
	cp -f ${PROG} ${SYSROOT_DIR}/sbin/

.include <bsd.subdir.mk>
.include <bsd.prog.mk>
