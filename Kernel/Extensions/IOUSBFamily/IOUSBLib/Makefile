# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# IOUSBLib: the CFPlug-In apps reach USB devices and interfaces through
# (IOCreatePlugInInterfaceForService with kIOUSBDeviceUserClientTypeID or
# kIOUSBInterfaceUserClientTypeID; IOUSBFamily's nubs name it in their
# IOCFPlugInTypes). Its classes are the APSL sources of IOUSBFamily-560.4.2
# (IOUSBLib/), unchanged but for building here; the kernel side is
# IOUSBFamily's own user clients.

SUBDIR =
BUNDLE =		IOUSBLib
BUNDLE_EXTENSION =	bundle
BUNDLE_INSTALL_DIR =	/System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns
SRCS =			IOUSBIUnknown.cpp IOUSBInterfaceClass.cpp IOUSBDeviceClass.cpp
FRAMEWORKS =		CoreFoundation IOKit
MK_WERROR =		no
CFLAGS +=		-I${.CURDIR} -I${.CURDIR}/include -Wno-incompatible-sysroot -Wno-nullability-completeness \
			-Wno-deprecated-declarations -Wno-everything
CXXFLAGS +=		${CFLAGS}
LDFLAGS +=		-lc++

.include <minkar.bundle.mk>
