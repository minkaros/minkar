# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Mesa's Vulkan driver for AMD GPUs (RADV), on Minkar's libdrm:
# /usr/lib/libvulkan_radeon.dylib and its ICD manifest in
# /usr/share/vulkan/icd.d. And, from a second build of the same tree, EGL
# and OpenGL ES (libEGL, libGLESv2) on the gallium drivers softpipe (in
# software, everywhere) and zink (OpenGL on the Vulkan driver), with EGL's
# surfaceless platform: what WebKit (W2) draws with. Mesa (MIT) is fetched at MANIFEST's version and
# checked against its SHA-256, copied and patched here (patches/), and built
# with Meson (minkar-cross.ini.in). The build tools come pinned too:
# glslang (MANIFEST.glslang), built for the build host, and Meson and Mako
# in a venv (requirements.txt, with their hashes; pip from its own pinned
# wheel, so the host needs no pip). Mesa's license goes with the driver;
# the SDK's Vulkan headers come from ../vulkan-loader (Khronos's own).

SUBDIR =
FETCH =		sh ${.CURDIR}/../libdrm/fetch.sh
FW_CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
TC =		${TOOLCHAIN}/usr/bin
PYTHON3 ?=	python3

PATCHES =	0001-darwin-libdrm.patch \
		0002-radv-darwin-weak-entrypoints.patch \
		0003-darwin-drm-ioctl.patch \
		0004-darwin-bo-map.patch \
		0005-minkar-metal-wsi.patch \
		0006-minkar-wsi-gpu-copy.patch \
		0007-zink-darwin-without-moltenvk.patch

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
BUILD_GL =	${.OBJDIR}/build-gl
GALLIUM =	libgallium-26.2.6.dylib
GL_LIBS =	src/egl/libEGL.1.dylib src/mesa/glapi/es2api/libGLESv2.2.dylib \
		src/gallium/targets/dri/${GALLIUM}

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

# EGL and OpenGL ES: softpipe and zink, no LLVM (llvmpipe later).
MESON_GL_OPTIONS = -Dbuildtype=release -Dprefix=/usr -Dplatforms= \
		-Dgallium-drivers=softpipe,zink -Dvulkan-drivers= -Dllvm=disabled \
		-Dglx=disabled -Degl=enabled -Dgbm=disabled -Dopengl=true \
		-Dgles1=disabled -Dgles2=enabled -Dexpat=disabled -Dzstd=disabled \
		-Dvalgrind=disabled -Dlibunwind=disabled -Dbuild-tests=false \
		-Dvideo-codecs= -Dvulkan-layers= -Dtools=

all: install-radv install-gl

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
	mkdir -p ${SYSROOT_DIR}/usr/lib ${SYSROOT_DIR}/usr/share/vulkan/icd.d
	rsync -cL ${RADV} ${.OBJDIR}/libvulkan_radeon.dylib
	${TC}/install_name_tool -id /usr/lib/libvulkan_radeon.dylib ${.OBJDIR}/libvulkan_radeon.dylib
	install -C -m 0755 ${.OBJDIR}/libvulkan_radeon.dylib ${SYSROOT_DIR}/usr/lib/
	install -C -m 0444 ${BUILD}/src/amd/vulkan/radeon_icd.x86_64.json ${SYSROOT_DIR}/usr/share/vulkan/icd.d/
	install -C -m 0444 ${SRC}/licenses/MIT ${SYSROOT_DIR}/usr/share/vulkan/LICENSE.mesa

${BUILD_GL}/.minkar-setup: ${SRC}/.minkar-patches ${CROSS} ${VENV}/.minkar-requirements \
    ${HOST}/bin/glslangValidator ${.CURDIR}/Makefile
	rm -rf ${BUILD_GL}
	${TOOLENV} meson setup ${BUILD_GL} ${SRC} \
	    --cross-file=${CROSS} ${MESON_GL_OPTIONS} > ${.OBJDIR}/meson-setup-gl.log 2>&1 || \
	    { tail -30 ${.OBJDIR}/meson-setup-gl.log; exit 1; }
	touch ${.TARGET}

build-gl: .PHONY ${BUILD_GL}/.minkar-setup
	${TOOLENV} ninja -C ${BUILD_GL} ${GL_LIBS}

# The libraries named for /usr/lib, in the SDK (headers and pkg-config
# files too, for WebKit's dependencies) and the system.
install-gl: .PHONY build-gl
.for lib in ${GL_LIBS}
	rsync -cL ${BUILD_GL}/${lib} ${.OBJDIR}/${lib:T}
	${TC}/install_name_tool -id /usr/lib/${lib:T} ${.OBJDIR}/${lib:T}
	${TC}/install_name_tool -change @rpath/${GALLIUM} /usr/lib/${GALLIUM} ${.OBJDIR}/${lib:T}
.endfor
.for dir in ${MINKAR_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${dir}/usr/lib
	for lib in ${GL_LIBS:T}; do install -C -m 0755 ${.OBJDIR}/$$lib ${dir}/usr/lib/; done
	ln -sf libEGL.1.dylib ${dir}/usr/lib/libEGL.dylib
	ln -sf libGLESv2.2.dylib ${dir}/usr/lib/libGLESv2.dylib
.endfor
	mkdir -p ${MINKAR_SDKROOT}/usr/include ${MINKAR_SDKROOT}/usr/lib/pkgconfig
	# Copied with rsync -c: an unchanged header keeps its date, and WebKit
	# is not compiled again.
	rsync -rlc ${SRC}/include/EGL ${SRC}/include/GLES2 ${SRC}/include/GLES3 ${SRC}/include/KHR \
	    ${MINKAR_SDKROOT}/usr/include/
	for pc in egl glesv2; do \
	    sed -e 's|^prefix=.*|prefix=${MINKAR_SDKROOT}/usr|' ${BUILD_GL}/meson-private/$$pc.pc \
	        > ${MINKAR_SDKROOT}/usr/lib/pkgconfig/$$pc.pc.new; \
	    cmp -s ${MINKAR_SDKROOT}/usr/lib/pkgconfig/$$pc.pc.new ${MINKAR_SDKROOT}/usr/lib/pkgconfig/$$pc.pc && \
	        rm -f ${MINKAR_SDKROOT}/usr/lib/pkgconfig/$$pc.pc.new || \
	        mv -f ${MINKAR_SDKROOT}/usr/lib/pkgconfig/$$pc.pc.new ${MINKAR_SDKROOT}/usr/lib/pkgconfig/$$pc.pc; \
	done
	install -C -m 0444 ${SRC}/licenses/MIT ${SYSROOT_DIR}/usr/share/vulkan/LICENSE.mesa

clean: .PHONY
	rm -rf ${SRC} ${BUILD} ${BUILD_GL} ${VENV} ${HOST} ${.OBJDIR}/glslang-build ${CROSS} ${LDWRAP} \
	    ${.OBJDIR}/libvulkan_radeon.dylib ${.OBJDIR}/meson-setup.log

.include <bsd.prog.mk>
