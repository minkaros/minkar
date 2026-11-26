# CoreVideo: CVDisplayLink, on a 60 Hz timer for now, and the types
# QuartzCore's headers use (CVTimeStamp). Installed in the SDK too:
# QuartzCore.h imports it, so AppKit cannot be imported from Swift
# without it.
FRAMEWORK = CoreVideo
FMWK_COMPAT_VERSION = 1.0.0

SUBDIR =
MK_INSTALLLIB = no
MK_WERROR = no

SRCS =		CVDisplayLink.m CVConstants.c CVPixelBuffer.c CVPixelBufferPool.c CVPixelFormatDescription.c CVImageBuffer.c
HEADERS =	CVBase.h CVDisplayLink.h CVPixelBuffer.h CVPixelBufferPool.h CVPixelFormatDescription.h CVReturn.h CoreVideo.h
RESOURCES =	Info.plist

# <CoreVideo/...> resolves to these sources.
INCLUDE_ROOT != mkdir -p ${.OBJDIR}/include-root && \
	ln -sfn ${.CURDIR} ${.OBJDIR}/include-root/CoreVideo && \
	echo ${.OBJDIR}/include-root
CFLAGS =	-I${INCLUDE_ROOT} -std=gnu17 -Wno-nullability-completeness
LDFLAGS =	-framework OpenGL -framework Foundation -framework CoreFoundation -framework CoreGraphics -framework IOSurface -lobjc

fmwk-install-hook:
.for dir in ${MINKAR_SDKROOT}/System/Library/Frameworks/ ${SYSROOT_DIR}/System/Library/Frameworks/
	mkdir -p ${dir}
	rsync -rlc ${.OBJDIR}/${FRAMEWORK}.framework ${dir}
.endfor

# Optimised: it was built without any -O.
CFLAGS += -O2
.include <minkar.framework.mk>
