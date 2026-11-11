# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# The Vulkan loader (Khronos Vulkan-Loader, Apache-2.0):
# /usr/lib/libvulkan.1.dylib (and libvulkan.dylib), which loads the drivers
# /usr/share/vulkan/icd.d names (RADV, BSD/lib/mesa). Built with CMake
# through minkar-toolchain.cmake.in, against Khronos's Vulkan-Headers at
# the same version (../vulkan-headers, which also installs them in the
# SDK). Both are fetched at build time and checked against their SHA-256.

SUBDIR =
FETCH =		sh ${.CURDIR}/../libdrm/fetch.sh
FW_CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
TC =		${TOOLCHAIN}/usr/bin
VERSION !=	awk '$$1 == "version" { print $$2 }' ${.CURDIR}/MANIFEST

TOOLFILE =	${.OBJDIR}/minkar-toolchain.cmake
HEADERS =	${.OBJDIR}/headers
BUILD =		${.OBJDIR}/build
LOADER =	${BUILD}/loader/libvulkan.${VERSION}.dylib
# The tools' environment: the toolchain's clang refuses more than one
# *_DEPLOYMENT_TARGET in it, and the toolchain file sets the target.
TOOLENV =	env -u MACOSX_DEPLOYMENT_TARGET -u IPHONEOS_DEPLOYMENT_TARGET

all: install-loader

${TOOLFILE}: ${.CURDIR}/minkar-toolchain.cmake.in
	sed -e 's|@TC@|${TC}|g' -e 's|@SDK@|${MINKAR_SDKROOT}|g' ${.ALLSRC} > ${.TARGET}

# Vulkan-Headers installed in the object directory, for the loader's CMake
# (its VulkanHeaders package and registry).
${HEADERS}/.minkar-installed: ${.CURDIR}/../vulkan-headers/MANIFEST ${TOOLFILE}
	src=$$(${FETCH} ${.CURDIR}/../vulkan-headers/MANIFEST ${FW_CACHE}) && \
	rm -rf ${.OBJDIR}/headers-build ${HEADERS} && \
	${TOOLENV} cmake -G Ninja -S $$src -B ${.OBJDIR}/headers-build \
	    -DCMAKE_TOOLCHAIN_FILE=${TOOLFILE} -DCMAKE_INSTALL_PREFIX=${HEADERS} > /dev/null && \
	${TOOLENV} cmake --install ${.OBJDIR}/headers-build > /dev/null && \
	cp $$src/LICENSE.md ${HEADERS}/ && cp -R $$src/LICENSES ${HEADERS}/
	touch ${.TARGET}

${BUILD}/.minkar-setup: ${.CURDIR}/MANIFEST ${HEADERS}/.minkar-installed ${.CURDIR}/Makefile
	src=$$(${FETCH} ${.CURDIR}/MANIFEST ${FW_CACHE}) && \
	rm -rf ${BUILD} && \
	${TOOLENV} cmake -G Ninja -S $$src -B ${BUILD} -DCMAKE_TOOLCHAIN_FILE=${TOOLFILE} \
	    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
	    -DCMAKE_PREFIX_PATH=${HEADERS} -DVulkanHeaders_DIR=${HEADERS}/share/cmake/VulkanHeaders \
	    -DBUILD_TESTS=OFF -DCMAKE_INSTALL_NAME_DIR=/usr/lib \
	    > ${.OBJDIR}/cmake-setup.log 2>&1 || { tail -30 ${.OBJDIR}/cmake-setup.log; exit 1; }
	touch ${.TARGET}

build-loader: .PHONY ${BUILD}/.minkar-setup
	${TOOLENV} ninja -C ${BUILD} vulkan

install-loader: .PHONY build-loader
	mkdir -p ${SYSROOT_DIR}/usr/lib ${SYSROOT_DIR}/usr/share/vulkan/icd.d \
	    ${MINKAR_SDKROOT}/usr/lib
	cp -f ${LOADER} ${.OBJDIR}/libvulkan.1.dylib
	${TC}/install_name_tool -id /usr/lib/libvulkan.1.dylib ${.OBJDIR}/libvulkan.1.dylib
	install -C -m 0755 ${.OBJDIR}/libvulkan.1.dylib ${SYSROOT_DIR}/usr/lib/libvulkan.1.dylib
	install -C -m 0755 ${.OBJDIR}/libvulkan.1.dylib ${MINKAR_SDKROOT}/usr/lib/libvulkan.1.dylib
.for d in ${SYSROOT_DIR} ${MINKAR_SDKROOT}
	ln -sf libvulkan.1.dylib ${d}/usr/lib/libvulkan.dylib
.endfor
	src=$$(${FETCH} ${.CURDIR}/MANIFEST ${FW_CACHE}) && \
	    install -C -m 0444 $$src/LICENSE.txt ${SYSROOT_DIR}/usr/share/vulkan/LICENSE.loader.txt

clean: .PHONY
	rm -rf ${BUILD} ${HEADERS} ${.OBJDIR}/headers-build ${TOOLFILE} ${.OBJDIR}/cmake-setup.log

.include <bsd.prog.mk>
