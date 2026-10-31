SUBDIR = 
MK_WERROR = no

PROG = newfs_hfs
SRCS =  hfs_endian.c makehfs.c newfs_hfs.c
MAN =

PROJ_VER = @(\#)PROGRAM:newfs_hfs  PROJECT:hfs-556~41\n
IOKIT_STORAGE = ${ROOT_SOURCE_DIR}/Kernel/Extensions/IOStorageFamily
LIBUTIL = ${ROOT_SOURCE_DIR}/BSD/lib/libutil
CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -I${.CURDIR} -I${.CURDIR:H}/mount_hfs/include -I${LIBUTIL} \
        -I${ROOT_SOURCE_DIR}/Frameworks -I${IOKIT_STORAGE} \
        '-DPROJ_VER="${PROJ_VER}"' -include${.CURDIR}/hfs_macos_defs.h
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
        -fuse-ld=${LD} -L${ROOT_BINARY_DIR}${LIBUTIL} -lutil \
        -framework CoreFoundation -lc++ \
        ${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/IOKitUser/libIOKit.a


all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin ${SYSROOT_DIR}/usr/share/man/man8
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/sbin/newfs_hfs
	install -m 0444 ${.CURDIR}/newfs_hfs.8 ${SYSROOT_DIR}/usr/share/man/man8/


.include <bsd.prog.mk>
