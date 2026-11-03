# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Minkar's libdrm and libdrm_amdgpu (ours, drm.c and amdgpu.c): what Mesa's
# RADV asks of them, on MinkarAMDUserClient. libdrm's headers (MIT) are
# fetched at MANIFEST's version and installed unchanged in the SDK's
# /usr/include/libdrm, with pkg-config files for Mesa's build:
# /usr/lib/libdrm.dylib and /usr/lib/libdrm_amdgpu.dylib.

SUBDIR =
FW_CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
VERSION !=	awk '$$1 == "version" { print $$2 }' ${.CURDIR}/MANIFEST
INC =		${MINKAR_SDKROOT}/usr/include/libdrm
PKG =		${MINKAR_SDKROOT}/usr/lib/pkgconfig
DRM_CFLAGS =	--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
		-Wall -Wno-nullability-completeness -Wno-incompatible-sysroot \
		-I${.CURDIR} -I${INC} \
		-I${ROOT_SOURCE_DIR}/Kernel/Extensions/MinkarDRM
DRM_LDFLAGS =	--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
		-fuse-ld=${LD} -shared

all: install-headers libdrm.dylib libdrm_amdgpu.dylib

install-headers: .PHONY
	src=$$(sh ${.CURDIR}/fetch.sh ${.CURDIR}/MANIFEST ${FW_CACHE}) && \
	mkdir -p ${INC} ${PKG} && \
	install -m 0444 $$src/xf86drm.h $$src/libsync.h $$src/amdgpu/amdgpu.h ${INC}/ && \
	install -m 0444 $$src/include/drm/drm.h $$src/include/drm/drm_mode.h \
	    $$src/include/drm/drm_fourcc.h $$src/include/drm/amdgpu_drm.h ${INC}/
.for p in libdrm libdrm_amdgpu
	sed -e 's|@SDK@|${MINKAR_SDKROOT}|' -e 's|@VERSION@|${VERSION}|' \
	    ${.CURDIR}/${p}.pc.in > ${PKG}/${p}.pc
.endfor

drm.o: ${.CURDIR}/drm.c ${.CURDIR}/minkar_drm_private.h install-headers
	${CC} ${DRM_CFLAGS} -c ${.CURDIR}/drm.c -o ${.TARGET}

amdgpu.o: ${.CURDIR}/amdgpu.c ${.CURDIR}/minkar_drm_private.h install-headers
	${CC} ${DRM_CFLAGS} -c ${.CURDIR}/amdgpu.c -o ${.TARGET}

amdgpu_info.o: ${.CURDIR}/amdgpu_info.c ${.CURDIR}/minkar_drm_private.h install-headers
	${CC} ${DRM_CFLAGS} -c ${.CURDIR}/amdgpu_info.c -o ${.TARGET}

libdrm.dylib: drm.o amdgpu_info.o
	${CC} ${DRM_LDFLAGS} -o ${.TARGET} drm.o amdgpu_info.o -framework IOKit -framework CoreFoundation \
	    -Wl,-install_name,/usr/lib/libdrm.dylib \
	    -Wl,-compatibility_version,2.0.0 -Wl,-current_version,2.4.134
	mkdir -p ${SYSROOT_DIR}/usr/lib ${MINKAR_SDKROOT}/usr/lib
	cp -f ${.TARGET} ${SYSROOT_DIR}/usr/lib/libdrm.dylib
	cp -f ${.TARGET} ${MINKAR_SDKROOT}/usr/lib/libdrm.dylib

libdrm_amdgpu.dylib: amdgpu.o libdrm.dylib
	${CC} ${DRM_LDFLAGS} -o ${.TARGET} amdgpu.o -L${.OBJDIR} -ldrm -framework IOKit \
	    -Wl,-install_name,/usr/lib/libdrm_amdgpu.dylib \
	    -Wl,-compatibility_version,1.0.0 -Wl,-current_version,1.0.0
	cp -f ${.TARGET} ${SYSROOT_DIR}/usr/lib/libdrm_amdgpu.dylib
	cp -f ${.TARGET} ${MINKAR_SDKROOT}/usr/lib/libdrm_amdgpu.dylib

clean: .PHONY
	rm -f drm.o amdgpu.o amdgpu_info.o libdrm.dylib libdrm_amdgpu.dylib

.include <bsd.prog.mk>
