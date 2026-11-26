# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# CoreImage.framework: images as recipes (CIImage), filters by name and
# category (CIFilter, its registry and image units), and a context that
# makes them into pixels over CoreGraphics. QuartzCore re-exports it.
# Its classes were in QuartzCore before 19 Nov 2026; the older code
# (CIColor, CIVector, CIImage, CIContext, the nib filters) is MRC, the
# rest ARC.

SUBDIR =
MK_WERROR = no
FRAMEWORK = CoreImage
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = CIConstants.m CIColor.m CIVector.m CIImage.m CIContext.m \
	CIFilter.m CIFilterRegistry.m CIBoxBlur.m CILinearGradient.m \
	CIBloom.m CIAffineTransform.m CIPlugIn.m
HEADERS = CoreImage.h CoreImageDefines.h CIColor.h CIVector.h CIImage.h \
	CIContext.h CIFilter.h CIFilterConstructor.h CIPlugIn.h CIPlugInInterface.h

ARC_SRCS = CIFilter.m CIFilterRegistry.m CIBoxBlur.m CILinearGradient.m CIPlugIn.m
.for s in ${ARC_SRCS}
CFLAGS.${s} = -fobjc-arc
.endfor

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR:H} -F${FMWKS} -fblocks
LDFLAGS += -F${FMWKS} -framework CoreGraphics -framework Foundation \
	-framework CoreFoundation -lobjc

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

# Optimised: it was built without any -O.
CFLAGS += -O2
.include <minkar.framework.mk>
