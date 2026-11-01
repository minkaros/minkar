# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# hidevents: prints IOHIDSystem's low-level event queue as the event server
# reads it (see hidevents.c), in /usr/local/bin. It needs the restricted
# entitlement com.apple.hid.system.server-access, so it is signed with it
# and the image build puts it in the trust cache. Built in the CoreServices
# stage, after IOKit.

SUBDIR =
MK_WERROR = no
WARNS ?= 2
MK_PIE = no
MK_DEBUG_FILES = no
PROG =	hidevents
SRCS =	hidevents.c
MAN =
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -framework IOKit -framework CoreFoundation

CODESIGN = python3 -B ${ROOT_SOURCE_DIR}/Developer/Default.xctoolchain/codesign/codesign.py

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/local/bin
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/usr/local/bin/hidevents
	${CODESIGN} -f -s - -i org.minkar.hidevents \
	    --entitlements ${.CURDIR}/hidevents.entitlements \
	    ${SYSROOT_DIR}/usr/local/bin/hidevents

.include <bsd.prog.mk>
