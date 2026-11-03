# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Khronos's Vulkan conformance tests (VK-GL-CTS, Apache-2.0) for Minkar:
# deqp-vk on the headless target, which loads Vulkan through the loader
# (/usr/lib/libvulkan.1.dylib, ../vulkan-loader). Not part of the system
# build nor the image (several hundred MB): built on demand,
#
#	sudo env SUBDIR=BSD/lib/vulkan-cts bmake -m BSD/share/mk
#
# it leaves vulkan-cts.tar.gz in the object directory: deqp-vk, its data,
# the subset G3's gate runs (subset.txt) and run.sh, which runs it and
# summarizes the results. Unpacked anywhere on the machine under test,
# "sh run.sh" runs the subset; "sh run.sh LIST" another case list.

SUBDIR =
FETCH =		sh ${.CURDIR}/../libdrm/fetch.sh
FW_CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
TC =		${TOOLCHAIN}/usr/bin
PYTHON3 ?=	python3

PATCHES !=	ls ${.CURDIR}/patches/*.patch 2>/dev/null || true
SRC =		${.OBJDIR}/cts-src
BUILD =		${.OBJDIR}/build
TOOLFILE =	${.OBJDIR}/minkar-toolchain.cmake
PKG =		${.OBJDIR}/vulkan-cts
TOOLENV =	env -u MACOSX_DEPLOYMENT_TARGET -u IPHONEOS_DEPLOYMENT_TARGET

all: package

${TOOLFILE}: ${.CURDIR}/../vulkan-loader/minkar-toolchain.cmake.in
	sed -e 's|@TC@|${TC}|g' -e 's|@SDK@|${MINKAR_SDKROOT}|g' ${.ALLSRC} > ${.TARGET}

# The CTS's tree with its external sources fetched into it (once), then a
# copy with our patches, made again when the patches change.
FETCHED =	${.OBJDIR}/cts-fetched

${FETCHED}/.minkar-fetched: ${.CURDIR}/MANIFEST
	pristine=$$(${FETCH} ${.CURDIR}/MANIFEST ${FW_CACHE}) && \
	rm -rf ${FETCHED} ${FETCHED}.part && cp -a $$pristine ${FETCHED}.part && \
	(cd ${FETCHED}.part && ${PYTHON3} external/fetch_sources.py > ${.OBJDIR}/fetch-sources.log 2>&1) || \
	    { tail -20 ${.OBJDIR}/fetch-sources.log; exit 1; }
	mv ${FETCHED}.part ${FETCHED}
	touch ${.TARGET}

${SRC}/.minkar-ready: ${FETCHED}/.minkar-fetched ${PATCHES}
	rm -rf ${SRC} ${SRC}.part && cp -a ${FETCHED} ${SRC}.part
.for p in ${PATCHES}
	patch -s -p1 -d ${SRC}.part < ${p}
.endfor
	mv ${SRC}.part ${SRC}
	touch ${.TARGET}

${BUILD}/.minkar-setup: ${SRC}/.minkar-ready ${TOOLFILE} ${.CURDIR}/Makefile
	rm -rf ${BUILD}
	${TOOLENV} cmake -G Ninja -S ${SRC} -B ${BUILD} -DCMAKE_TOOLCHAIN_FILE=${TOOLFILE} \
	    -DCMAKE_BUILD_TYPE=Release -DDEQP_TARGET=vulkan_headless \
	    '-DCMAKE_CXX_FLAGS=-DDEQP_VULKAN_LIBRARY_PATH=\"libvulkan.1.dylib\"' \
	    '-DCMAKE_C_FLAGS=-DDEQP_VULKAN_LIBRARY_PATH=\"libvulkan.1.dylib\"' \
	    > ${.OBJDIR}/cmake-setup.log 2>&1 || { tail -40 ${.OBJDIR}/cmake-setup.log; exit 1; }
	touch ${.TARGET}

build-cts: .PHONY ${BUILD}/.minkar-setup
	${TOOLENV} ninja -C ${BUILD} deqp-vk

package: .PHONY build-cts
	rm -rf ${PKG} && mkdir -p ${PKG}
	cp ${BUILD}/external/vulkancts/modules/vulkan/deqp-vk ${PKG}/
	cp -R ${BUILD}/external/vulkancts/modules/vulkan/vulkan ${PKG}/
	cp ${.CURDIR}/subset.txt ${.CURDIR}/run.sh ${PKG}/
	# The subset's cases: Khronos's mustpass list under its prefixes.
	grep -v '^#' ${.CURDIR}/subset.txt | sed 's/^/^/; s/\./\\./g' > ${.OBJDIR}/subset.re
	cat ${SRC}/external/vulkancts/mustpass/main/vk-default/*.txt \
	    ${SRC}/external/vulkancts/mustpass/main/vk-default/*/*.txt | \
	    grep -E -f ${.OBJDIR}/subset.re > ${PKG}/caselist.txt
	wc -l < ${PKG}/caselist.txt
	cp ${SRC}/LICENSE ${PKG}/LICENSE.vk-gl-cts
	tar -C ${.OBJDIR} -czf ${.OBJDIR}/vulkan-cts.tar.gz vulkan-cts

clean: .PHONY
	rm -rf ${FETCHED} ${SRC} ${BUILD} ${PKG} ${TOOLFILE} ${.OBJDIR}/vulkan-cts.tar.gz \
	    ${.OBJDIR}/*.log

.include <bsd.prog.mk>
