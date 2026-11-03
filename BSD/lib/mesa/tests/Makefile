# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# minkar-vktest: Vulkan through RADV, loaded as the Vulkan loader loads a
# driver; minkar-vkinfo: Vulkan through the loader, as apps use it;
# minkar-vkshared: a shared VRAM buffer imported into Vulkan;
# minkar-vkcopy: what presenting a frame costs, the GPU's and the CPU's
# copies timed (G4.6);
# minkar-vkwindow: frames presented in an AppKit window through a
# CAMetalLayer (VK_EXT_metal_surface); minkar-vkdraw: a triangle drawn off
# the screen, its shaders
# (shaders/) made SPIR-V by the glslang ../ builds. In /usr/local/bin with
# the test tools; the Vulkan headers come from the SDK (../../vulkan-loader
# installs them).

SUBDIR =
MK_WERROR = no
PROG =	minkar-vktest
SRCS =	vktest.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

GLSLANG =	${.OBJDIR}/../host-tools/bin/glslangValidator

all: to-sysroot

.for s in vert frag
triangle_${s}.h: ${.CURDIR}/shaders/triangle.${s}
	${GLSLANG} -V --vn triangle_${s} -o ${.TARGET} ${.ALLSRC} > /dev/null
.endfor

minkar-vkdraw: ${.CURDIR}/vkdraw.c triangle_vert.h triangle_frag.h
	${CC} ${CFLAGS} -I${.OBJDIR} ${LDFLAGS} -o ${.TARGET} ${.CURDIR}/vkdraw.c

minkar-vkinfo: ${.CURDIR}/vkinfo.c
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.CURDIR}/vkinfo.c -L${MINKAR_SDKROOT}/usr/lib -lvulkan

minkar-vkshared: ${.CURDIR}/vkshared.c
	${CC} ${CFLAGS} -I${ROOT_SOURCE_DIR}/Kernel/Extensions/MinkarDRM ${LDFLAGS} -o ${.TARGET} \
	    ${.CURDIR}/vkshared.c -L${MINKAR_SDKROOT}/usr/lib -lvulkan -ldrm_amdgpu -framework IOKit \
	    -framework CoreFoundation

minkar-vkcopy: ${.CURDIR}/vkcopy.c
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${.CURDIR}/vkcopy.c -L${MINKAR_SDKROOT}/usr/lib -lvulkan

minkar-vkwindow: ${.CURDIR}/vkwindow.m triangle_vert.h triangle_frag.h
	${CC} ${CFLAGS} -I${.OBJDIR} -I${ROOT_SOURCE_DIR}/Frameworks \
	    -F${MINKAR_SDKROOT}/System/Library/PrivateFrameworks ${LDFLAGS} -o ${.TARGET} \
	    ${.CURDIR}/vkwindow.m -framework AppKit -framework QuartzCore -framework Foundation \
	    -framework SkyLight -framework IOSurface -lobjc

to-sysroot: ${PROG} minkar-vkdraw minkar-vkwindow minkar-vkinfo minkar-vkshared minkar-vkcopy
	mkdir -p ${SYSROOT_DIR}/usr/local/bin
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/local/bin/minkar-vktest
	install -m 0555 -o 0 -g 0 minkar-vkdraw ${SYSROOT_DIR}/usr/local/bin/minkar-vkdraw
	install -m 0555 -o 0 -g 0 minkar-vkwindow ${SYSROOT_DIR}/usr/local/bin/minkar-vkwindow
	install -m 0555 -o 0 -g 0 minkar-vkinfo ${SYSROOT_DIR}/usr/local/bin/minkar-vkinfo
	install -m 0555 -o 0 -g 0 minkar-vkshared ${SYSROOT_DIR}/usr/local/bin/minkar-vkshared
	install -m 0555 -o 0 -g 0 minkar-vkcopy ${SYSROOT_DIR}/usr/local/bin/minkar-vkcopy

CLEANFILES +=	minkar-vkdraw minkar-vkwindow minkar-vkinfo minkar-vkshared minkar-vkcopy triangle_vert.h triangle_frag.h

.include <bsd.prog.mk>
