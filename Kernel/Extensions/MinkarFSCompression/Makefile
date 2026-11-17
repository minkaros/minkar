MK_WERROR = no
SUBDIR =
KEXT = MinkarFSCompression
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = org.minkar.driver.MinkarFSCompression
BUNDLE_VERSION = 1.0.0

SRCS = MinkarFSCompression.cpp MinkarFSCompressionTypes.c

# The decompressors: libkern's zlib, and LZVN and LZFSE (BSD-3) from their
# sources in the tree, built for the kernel as the disk image driver
# builds them (its kshim gives them <stdlib.h>).
LZFSE = ${ROOT_SOURCE_DIR}/BSD/lib/libcompression/dist
KSHIM = ${.CURDIR:H}/MinkarDiskImage/kshim
.PATH: ${LZFSE}
LZFSE_SRCS = lzfse_decode.c lzfse_decode_base.c lzfse_fse.c lzvn_decode_base.c
SRCS += ${LZFSE_SRCS}
.for f in ${LZFSE_SRCS} MinkarFSCompressionTypes.c
CFLAGS.${f} += -I${KSHIM} -DNDEBUG -Dmalloc=minkar_fsc_malloc -Dfree=minkar_fsc_free \
	-Wno-unused-parameter
.endfor

CFLAGS = -Wno-implicit-int-conversion -Wno-sign-conversion \
	-DPRIVATE=1 -I${LZFSE} \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/bsd \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/san

.include <minkar.kext.mk>
