# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# minkar-vktest: Vulkan through RADV, loaded as the Vulkan loader loads a
# driver; minkar-vkdraw: a triangle drawn off the screen, its shaders
# (shaders/) made SPIR-V by the glslang ../ builds. In /usr/local/bin with
# the test tools; the Vulkan headers come from the SDK (../ installs them).

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

to-sysroot: ${PROG} minkar-vkdraw
	mkdir -p ${SYSROOT_DIR}/usr/local/bin
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/local/bin/minkar-vktest
	install -m 0555 -o 0 -g 0 minkar-vkdraw ${SYSROOT_DIR}/usr/local/bin/minkar-vkdraw

CLEANFILES +=	minkar-vkdraw triangle_vert.h triangle_frag.h

.include <bsd.prog.mk>
