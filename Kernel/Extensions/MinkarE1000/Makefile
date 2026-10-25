# MinkarE1000: driver for Intel 8254x and 82574L Ethernet (QEMU e1000 and
# e1000e, VirtualBox, VMware), on our IONetworkingFamily.
MK_WERROR = no
SUBDIR =
KEXT = MinkarE1000
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = org.minkar.driver.MinkarE1000
BUNDLE_VERSION = 1.0.0

SRCS = MinkarE1000.cpp

CFLAGS = -Wno-implicit-int-conversion -Wno-sign-conversion \
	-I${.CURDIR:H}/IONetworkingFamily \
	-I${.CURDIR:H}/IOPCIFamily \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/bsd

.include <rvn.kext.mk>
