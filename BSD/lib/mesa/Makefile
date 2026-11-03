# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Mesa's Vulkan driver for AMD GPUs (RADV), on Minkar's libdrm:
# /usr/lib/libvulkan_radeon.dylib and its ICD manifest in
# /usr/share/vulkan/icd.d. Mesa (MIT) is fetched at MANIFEST's version and
# checked against its SHA-256, copied and patched here (patches/), and built
# with Meson (minkar-cross.ini.in). The build tools come pinned too:
# glslang (MANIFEST.glslang), built for the build host, and Meson and Mako
# in a venv (requirements.txt, with their hashes; pip from its own pinned
# wheel, so the host needs no pip). The Vulkan headers Mesa carries
# (Khronos, Apache-2.0; the maintainer's approval, 2026-11-03) go to the
# SDK's /usr/include/vulkan and vk_video with their license, for programs
# that use Vulkan; Mesa's license goes with the driver.

SUBDIR =
FETCH =		sh ${.CURDIR}/../libdrm/fetch.sh
FW_CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
TC =		${TOOLCHAIN}/usr/bin
PYTHON3 ?=	python3

PATCHES =	0001-darwin-libdrm.patch \
		0002-radv-darwin-weak-entrypoints.patch \
		0003-darwin-drm-ioctl.patch \
		0004-darwin-bo-map.patch \
		0005-minkar-metal-wsi.patch

PIP_WHEEL =	pip-25.0.1-py3-none-any.whl
PIP_URL =	https://files.pythonhosted.org/packages/c9/bc/b7db44f5f39f9d0494071bddae6880eb645970366d0a200022a1a93d57f5/${PIP_WHEEL}
PIP_SHA256 =	c46efd13b6aa8279f33f2864459c8ce587ea6a1a59ee20de055868d8f7688f7f

SRC =		${.OBJDIR}/mesa-src
VENV =		${.OBJDIR}/venv
HOST =		${.OBJDIR}/host-tools
BUILD =		${.OBJDIR}/build
CROSS =		${.OBJDIR}/minkar-cross.ini
LDWRAP =	${.OBJDIR}/ld64-meson
RADV =		${BUILD}/src/amd/vulkan/libvulkan_radeon.dylib

# The tools' environment: the toolchain's clang refuses more than one
# *_DEPLOYMENT_TARGET in it, and the cross file sets the target already.
TOOLENV =	env -u MACOSX_DEPLOYMENT_TARGET -u IPHONEOS_DEPLOYMENT_TARGET \
		-u TVOS_DEPLOYMENT_TARGET -u WATCHOS_DEPLOYMENT_TARGET \
		-u XROS_DEPLOYMENT_TARGET -u DRIVERKIT_DEPLOYMENT_TARGET \
		PATH=${VENV}/bin:${HOST}/bin:$${PATH}

# Only RADV, without LLVM (ACO compiles the shaders), with the Metal
# surface WSI (CAMetalLayer), no other APIs for now.
MESON_OPTIONS =	-Dbuildtype=release -Dprefix=/usr -Dplatforms=macos -Dgallium-drivers= \
		-Dvulkan-drivers=amd -Dllvm=disabled -Dglx=disabled -Degl=disabled \
		-Dgbm=disabled -Dopengl=false -Dgles1=disabled -Dgles2=disabled \
		-Dexpat=disabled -Dzstd=disabled -Dvalgrind=disabled -Dlibunwind=disabled \
		-Dbuild-tests=false -Dvideo-codecs= -Dvulkan-layers= -Dtools=

all: install-radv

# Mesa's tree with our patches, made again when the patches change.
${SRC}/.minkar-patches: ${PATCHES:S,^,${.CURDIR}/patches/,} ${.CURDIR}/MANIFEST
	pristine=$$(${FETCH} ${.CURDIR}/MANIFEST ${FW_CACHE}) && \
	rm -rf ${SRC} ${SRC}.part && cp -a $$pristine ${SRC}.part && \
	for p in ${PATCHES}; do \
		patch -s -p1 -d ${SRC}.part < ${.CURDIR}/patches/$$p || exit 1; \
	done && \
	mv ${SRC}.part ${SRC}
	cat ${.ALLSRC} | cksum > ${.TARGET}

# glslangValidator for the build host.
${HOST}/bin/glslangValidator: ${.CURDIR}/MANIFEST.glslang
	src=$$(${FETCH} ${.CURDIR}/MANIFEST.glslang ${FW_CACHE}) && \
	cmake -G Ninja -S $$src -B ${.OBJDIR}/glslang-build -DCMAKE_BUILD_TYPE=Release \
	    -DCMAKE_INSTALL_PREFIX=${HOST} -DENABLE_OPT=OFF -DENABLE_HLSL=OFF \
	    -DBUILD_EXTERNAL=OFF -DGLSLANG_TESTS=OFF -DBUILD_TESTING=OFF > /dev/null && \
	ninja -C ${.OBJDIR}/glslang-build install > /dev/null

# Meson and Mako, pinned, in a venv of the build directory.
${VENV}/.minkar-requirements: ${.CURDIR}/requirements.txt
	mkdir -p ${FW_CACHE}
	test -f ${FW_CACHE}/${PIP_WHEEL} || \
	    curl -sfL -o ${FW_CACHE}/${PIP_WHEEL} ${PIP_URL}
	echo "${PIP_SHA256}  ${FW_CACHE}/${PIP_WHEEL}" | sha256sum -c --quiet
	rm -rf ${VENV} && ${PYTHON3} -m venv --without-pip ${VENV}
	${VENV}/bin/python ${FW_CACHE}/${PIP_WHEEL}/pip install -q --disable-pip-version-check \
	    --require-hashes -r ${.CURDIR}/requirements.txt
	cksum < ${.CURDIR}/requirements.txt > ${.TARGET}

# The cross file and the linker wrapper Meson takes (its "PROJECT:ld64" line).
${CROSS}: ${.CURDIR}/minkar-cross.ini.in ${.CURDIR}/ld64-meson.in
	sed -e 's|@TC@|${TC}|g' ${.CURDIR}/ld64-meson.in > ${LDWRAP}
	chmod 0755 ${LDWRAP}
	sed -e 's|@TC@|${TC}|g' -e 's|@SDK@|${MINKAR_SDKROOT}|g' -e 's|@LD@|${LDWRAP}|g' \
	    ${.CURDIR}/minkar-cross.ini.in > ${.TARGET}

# Set up again when the tree, the cross file or the options change.
${BUILD}/.minkar-setup: ${SRC}/.minkar-patches ${CROSS} ${VENV}/.minkar-requirements \
    ${HOST}/bin/glslangValidator ${.CURDIR}/Makefile
	rm -rf ${BUILD}
	${TOOLENV} meson setup ${BUILD} ${SRC} \
	    --cross-file=${CROSS} ${MESON_OPTIONS} > ${.OBJDIR}/meson-setup.log 2>&1 || \
	    { tail -30 ${.OBJDIR}/meson-setup.log; exit 1; }
	touch ${.TARGET}

build-radv: .PHONY ${BUILD}/.minkar-setup
	${TOOLENV} ninja -C ${BUILD} src/amd/vulkan/libvulkan_radeon.dylib \
	    src/amd/vulkan/radeon_icd.x86_64.json

install-radv: .PHONY build-radv
	mkdir -p ${SYSROOT_DIR}/usr/lib ${SYSROOT_DIR}/usr/share/vulkan/icd.d \
	    ${MINKAR_SDKROOT}/usr/include/vulkan ${MINKAR_SDKROOT}/usr/include/vk_video
	cp -f ${RADV} ${.OBJDIR}/libvulkan_radeon.dylib
	${TC}/install_name_tool -id /usr/lib/libvulkan_radeon.dylib ${.OBJDIR}/libvulkan_radeon.dylib
	install -m 0755 ${.OBJDIR}/libvulkan_radeon.dylib ${SYSROOT_DIR}/usr/lib/
	install -m 0444 ${BUILD}/src/amd/vulkan/radeon_icd.x86_64.json ${SYSROOT_DIR}/usr/share/vulkan/icd.d/
	install -m 0444 ${SRC}/licenses/MIT ${SYSROOT_DIR}/usr/share/vulkan/LICENSE.mesa
	install -m 0444 ${SRC}/include/vulkan/*.h ${MINKAR_SDKROOT}/usr/include/vulkan/
	install -m 0444 ${SRC}/include/vk_video/*.h ${MINKAR_SDKROOT}/usr/include/vk_video/
	install -m 0444 ${SRC}/licenses/Apache-2.0 ${MINKAR_SDKROOT}/usr/include/vulkan/LICENSE.Apache-2.0

clean: .PHONY
	rm -rf ${SRC} ${BUILD} ${VENV} ${HOST} ${.OBJDIR}/glslang-build ${CROSS} ${LDWRAP} \
	    ${.OBJDIR}/libvulkan_radeon.dylib ${.OBJDIR}/meson-setup.log

.include <bsd.prog.mk>
