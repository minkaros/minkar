MK_WERROR = no
FRAMEWORK = QuartzCore
FMWK_VERSION = A
SUBDIR =

SRCS= \
	CAAnimation.m \
	CAAnimationGroup.m \
	CABase.m \
	CABasicAnimation.m \
	CALayer.m \
	CALayerContext.m \
	CAMediaTimingFunction.m \
	CAPropertyAnimation.m \
	CARenderer.m \
	CATransaction.m \
	CATransactionGroup.m \
	CATransform3D.m \
	CATransition.m \
	CAValueFunction.m \
	CAWindowOpenGLContext.m \
	CIAffineTransform.m \
	CIBloom.m \
	CIBoxBlur.m \
	CIColor.m \
	CIContext.m \
	CIFilter.m \
	CIImage.m \
	CILinearGradient.m \
	CIVector.m

HEADERS= \
	CAAction.h \
	CAAnimation.h \
	CAAnimationGroup.h \
	CABase.h \
	CABasicAnimation.h \
	CALayer.h \
	CALayerContext.h \
	CAMediaTiming.h \
	CAMediaTimingFunction.h \
	CAPropertyAnimation.h \
	CIBloom.h \
	CARenderer.h \
	CATransaction.h \
	CATransactionGroup.h \
	CATransform3D.h \
	CATransition.h \
	CAValueFunction.h \
	CAWindowOpenGLContext.h \
	CIAffineTransform.h \
	CIBoxBlur.h \
	CIColor.h \
	CIContext.h \
	CIFilter.h \
	CIImage.h \
	CILinearGradient.h \
	CIVector.h \
	CoreAnimation.h \
	CoreImage.h \
	CoreVideo.h \
	QuartzCore.h 

RESOURCES_FILES = Info.plist
RESOURCES_DIRS = English.lproj

CFLAGS = -I${.CURDIR:H} -fblocks
LDFLAGS = -framework OpenGL -framework Onyx2D -framework CoreGraphics \
	-framework Foundation -framework CoreFoundation -lobjc

fmwk-install-hook:
.for dir in ${RAVYN_SDKROOT}/System/Library/Frameworks/ ${SYSROOT_DIR}/System/Library/Frameworks/
	mkdir -p ${dir}
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${dir}
.endfor

.include <rvn.framework.mk>
