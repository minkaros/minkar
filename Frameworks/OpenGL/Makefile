FRAMEWORK = OpenGL

SUBDIR =
MK_INSTALLLIB = no
MK_WERROR = no

# Core OpenGL (CGL): hardware independent; rendering lives in drivers
# registered through CGLDriver.h.
SRCS = \
	CGLContext.c \
	CGLDriverRegistry.c \
	CGLGlobal.c \
	CGLNullDriver.c \
	CGLPBuffer.c \
	CGLPixelFormat.c \
	GLDispatch.gen.c

HEADERS = \
	include/CGLCurrent.h \
	include/CGLDevice.h \
	include/CGLRenderers.h \
	include/CGLTypes.h \
	include/OpenGL.h \
	include/gltypes.h \
	include/gl.h \
	include/gl3.h \
	include/gl3ext.h \
	include/glcorearb.h \
	include/glext.h \
	include/glmesa.h \
	include/egl.h \
	include/eglext.h \
	include/eglextchromium.h \
	include/eglexternalplatform.h \
	include/eglexternalplatformversion.h \
	include/eglmesaext.h \
	include/eglplatform.h \
	include/khrplatform.h

# ravynOS-private: driver interface and AppKit/QuartzCore extensions.
PRIVATE_HEADERS = \
	include/CGLDriver.h \
	include/CGLPrivate.h

RESOURCES = Info.plist

# <OpenGL/...> must resolve to these sources, not to the SDK copy from a
# previous build; the symlink is made at parse time, before any compile.
INCLUDE_ROOT != mkdir -p ${.OBJDIR}/include-root && \
	ln -sfn ${.CURDIR}/include ${.OBJDIR}/include-root/OpenGL && \
	echo ${.OBJDIR}/include-root
CFLAGS = -I${INCLUDE_ROOT} -I${.CURDIR} -std=gnu17 -fvisibility=hidden

# GL entry points (GLDispatch.gen.[ch]) are generated from the vendored
# headers and committed; regenerate after updating include/glmesa.h or
# include/glext.h.
regen-dispatch:
	python3 -B ${.CURDIR}/tools/gen_gl_dispatch.py ${.CURDIR}/include ${.CURDIR}

fmwk-install-hook:
.for dir in ${RAVYN_SDKROOT}/System/Library/Frameworks/ ${SYSROOT_DIR}/System/Library/Frameworks/
	mkdir -p ${dir}
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${dir}
.endfor

.include <rvn.framework.mk>
