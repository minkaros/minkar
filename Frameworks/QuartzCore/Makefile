MK_WERROR = no
FRAMEWORK = QuartzCore
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.2.0
SUBDIR =

SRCS= \
	CAAnimation.m \
	CAAnimationGroup.m \
	CABase.m \
	CABasicAnimation.m \
	CALayer.m \
	CAYUVImage.c \
	CAShapeLayer.m \
	CALayerContext.m \
	CAMetalLayer.m \
	CAOpenGLLayer.m \
	CAMediaTimingFunction.m \
	CAPropertyAnimation.m \
	CARenderer.m \
	CATransaction.m \
	CATransactionGroup.m \
	CATransform3D.m \
	CATransition.m \
	CAValueFunction.m \
	CAWindowOpenGLContext.m

HEADERS= \
	CAAction.h \
	CAAnimation.h \
	CAAnimationGroup.h \
	CABase.h \
	CABasicAnimation.h \
	CALayer.h \
	CAShapeLayer.h \
	CALayerContext.h \
	CAMetalLayer.h \
	CAOpenGLLayer.h \
	CAMetalLayerPrivate.h \
	CAMediaTiming.h \
	CAMediaTimingFunction.h \
	CAPropertyAnimation.h \
	CARenderer.h \
	CATransaction.h \
	CATransactionGroup.h \
	CATransform3D.h \
	CATransition.h \
	CAValueFunction.h \
	CAWindowOpenGLContext.h \
	CoreAnimation.h \
	CoreImage.h \
	CoreVideo.h \
	QuartzCore.h 

RESOURCES_FILES = Info.plist
RESOURCES_DIRS = English.lproj

CFLAGS = -I${.CURDIR:H} -fblocks -F${MINKAR_SDKROOT}/System/Library/PrivateFrameworks
# The YCbCr conversion runs for every pixel of every video frame shown.
CFLAGS.CAYUVImage.c = -O2
LDFLAGS = -framework IOSurface \
	-F${MINKAR_SDKROOT}/System/Library/PrivateFrameworks -framework SkyLight \
	-framework OpenGL -framework Onyx2D -framework CoreGraphics \
	-F${MINKAR_SDKROOT}/System/Library/Frameworks -Wl,-reexport_framework,CoreImage \
	-framework Foundation -framework CoreFoundation -lobjc

fmwk-install-hook:
.for dir in ${MINKAR_SDKROOT}/System/Library/Frameworks/ ${SYSROOT_DIR}/System/Library/Frameworks/
	mkdir -p ${dir}
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${dir}
.endfor

.include <minkar.framework.mk>
