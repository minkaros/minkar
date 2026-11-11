# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# CFNetwork.framework: Minkar's own subset of the API (the proxy API for
# now), beside the APSL CFNetwork sources in the parent directory, which
# are not built yet.

MK_WERROR = no
FRAMEWORK = CFNetwork
FMWK_VERSION = A
FMWK_COMPAT_VERSION = 1.0.0
SRCS = CFProxySupport.c
HEADERS = CFNetwork.h CFProxySupport.h
SUBDIR =

FMWKS = ${MINKAR_SDKROOT}/System/Library/Frameworks
CFLAGS += -I${.CURDIR} -F${FMWKS}
LDFLAGS += -F${FMWKS} -framework CoreFoundation

fmwk-install-hook:
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks \
	  ${SYSROOT_DIR}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${MINKAR_SDKROOT}/System/Library/Frameworks
	rsync -rlc --delete ${.OBJDIR}/${FRAMEWORK}.framework ${SYSROOT_DIR}/System/Library/Frameworks

.include <minkar.framework.mk>
