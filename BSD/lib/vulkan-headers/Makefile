# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Khronos's Vulkan headers (Vulkan-Headers, Apache-2.0, some MIT), fetched
# at MANIFEST's version and checked against its SHA-256, into the SDK's
# /usr/include/vulkan and vk_video with their licenses. Built with the
# frameworks (Frameworks/Makefile), before the window server, which
# uses Vulkan; the loader (../vulkan-loader) and Mesa come later.

SUBDIR =
FETCH =		sh ${.CURDIR}/../libdrm/fetch.sh
FW_CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache

all: install-headers

install-headers: .PHONY
	src=$$(${FETCH} ${.CURDIR}/MANIFEST ${FW_CACHE}) && \
	mkdir -p ${MINKAR_SDKROOT}/usr/include && \
	rm -rf ${MINKAR_SDKROOT}/usr/include/vulkan ${MINKAR_SDKROOT}/usr/include/vk_video && \
	cp -R $$src/include/vulkan $$src/include/vk_video ${MINKAR_SDKROOT}/usr/include/ && \
	install -m 0444 $$src/LICENSE.md ${MINKAR_SDKROOT}/usr/include/vulkan/LICENSE.md && \
	cp -R $$src/LICENSES ${MINKAR_SDKROOT}/usr/include/vulkan/

.include <bsd.prog.mk>
