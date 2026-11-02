# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# MinkarDRM: GPU drivers from drm-kmod on XNU (ADR 0002). Our LinuxKPI
# layer (linuxkpi/), files imported unchanged (dist/, by
# tools/drm-import), and the IOKit drivers around them. So far:
# MinkarAMDGPU, which reads and parses an AMD GPU's VBIOS, and
# MinkarAMDFramebuffer, the display of a GCN 1.0 GPU (G1).
MK_WERROR = no
SUBDIR =
KEXT = MinkarDRM
KERNEL_PRIVATE = yes
INFO_PLIST = Info.plist
BUNDLE_IDENTIFIER = org.minkar.driver.MinkarDRM
BUNDLE_VERSION = 1.0.0

AMD = ${.CURDIR}/dist/drm-kmod/drivers/gpu/drm/amd
.PATH: ${.CURDIR}/linuxkpi/xnu ${.CURDIR}/amdgpu ${AMD}/amdgpu

SRCS = lkpi_xnu.cpp atom_glue.c atom.c MinkarAMDDevice.cpp MinkarAMDGPU.cpp \
	MinkarAMDFramebuffer.cpp MinkarAMDDMA.cpp MinkarAMDGART.cpp MinkarAMDUserClient.cpp minkar_vram.c

CFLAGS = -Wno-implicit-int-conversion -Wno-sign-conversion -Wno-format-zero-length \
	-Wno-pointer-sign -Wno-unused-parameter -Wno-missing-prototypes \
	-Wno-cast-qual -Wno-shadow -Wno-missing-variable-declarations \
	-Wno-strict-prototypes -Wno-cast-align -Wno-unused-but-set-variable \
	-I${.CURDIR}/linuxkpi/include -I${.CURDIR}/amdgpu \
	-I${AMD}/amdgpu -I${AMD}/include \
	-I${.CURDIR:H}/IOPCIFamily \
	-I${.CURDIR:H}/IOGraphics/IOGraphicsFamily \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/iokit \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/bsd

.include <minkar.kext.mk>
