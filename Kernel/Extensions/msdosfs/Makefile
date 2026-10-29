MK_WERROR = no
SUBDIR =
KEXT = msdosfs
KERNEL_PRIVATE = yes
INFO_PLIST = Info-msdosfs_kext.plist
BUNDLE_IDENTIFIER = com.apple.filesystems.msdosfs
BUNDLE_VERSION = 1.10.0
# Minkar: the kext's start and stop register and remove the "msdos" file
# system; without them it loads and does nothing.
MAIN_FUNCTION = msdosfs_module_start
ANTIMAIN_FUNCTION = msdosfs_module_stop

.PATH: ${.CURDIR}/msdosfs.kextproj/msdosfs.kmodproj
SRCS = 	msdosfs_conv.c \
	msdosfs_denode.c \
	msdosfs_fat.c \
	msdosfs_lookup.c \
	msdosfs_vfsops.c \
	msdosfs_vnops.c \
	msdosfs_loader.cpp

CFLAGS = -DPRIVATE -DDEBUG=0 -DKERNEL=1 \
	-Wno-implicit-int-conversion -Wno-sign-conversion \
	-I${.CURDIR}/include \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/san \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/EXTERNAL_HEADERS \
	-I${.CURDIR:H}/IOStorageFamily \
	-include${ROOT_SOURCE_DIR}/Kernel/xnu/bsd/i386/endian.h

.include <minkar.kext.mk>
