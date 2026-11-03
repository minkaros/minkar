# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# IOSurface.framework (roadmap step 4): pixel buffers shared across
# processes, as Mach memory entries; their global IDs through the registry,
# iosurfaced (CoreServices/iosurfaced).

FRAMEWORK = IOSurface
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
MK_WERROR = no

SRCS = IOSurface.c IOSurfaceRegistryUser.c
HEADERS = IOSurface.h IOSurfaceRef.h IOSurfaceTypes.h IOSurfaceAPI.h
CFLAGS += -I${.OBJDIR} -I${.OBJDIR}/include -I${.CURDIR} \
	-I${ROOT_SOURCE_DIR}/Kernel/Extensions/MinkarDRM
LDFLAGS += -framework CoreFoundation -framework IOKit
CLEANFILES += IOSurfaceRegistryUser.c IOSurfaceRegistryUser.h

# The client side of the MIG interface.
IOSurfaceRegistryUser.c IOSurfaceRegistryUser.h: ${.CURDIR}/IOSurfaceRegistry.defs
	${MIG} -cc ${CC} -novouchers -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -isysroot ${MINKAR_SDKROOT} -I${.CURDIR} \
	    -user IOSurfaceRegistryUser.c -header IOSurfaceRegistryUser.h \
	    -server /dev/null -sheader /dev/null ${.ALLSRC}

# <IOSurface/...> includes find the sources.
${.OBJDIR}/include/IOSurface:
	mkdir -p ${.OBJDIR}/include
	ln -sfn ${.CURDIR} ${.TARGET}

IOSurface.o: IOSurfaceRegistryUser.h ${.OBJDIR}/include/IOSurface

# Into the SDK (for the programs that link it) and the system.
fmwk-install-hook:
.for dir in ${MINKAR_SDKROOT}/System/Library/Frameworks/ ${SYSROOT_DIR}/System/Library/Frameworks/
	mkdir -p ${dir}
	cp -Rf ${.OBJDIR}/${FRAMEWORK}.framework ${dir}
.endfor

.include <minkar.framework.mk>
