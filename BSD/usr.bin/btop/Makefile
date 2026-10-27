# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# btop 1.4.7 (Apache-2.0, dist/LICENSE; github.com/aristocratos/btop), the
# full-screen resource monitor, on its macOS backend (dist/src/osx: libproc,
# the Mach host interface, sysctl, IOKit and CoreFoundation). dist/ holds
# upstream's files that Minkar builds, unchanged but for one line marked
# "Minkar" in btop_config.cpp. C++23 with fmt header-only, as upstream's
# CMake builds it; no GPU support (Apple's needs Metal). Installs
# /usr/bin/btop, its manual page and its themes in /usr/share/btop/themes
# (btop looks in ../share/btop next to its binary).

SUBDIR =
BTOP_VERSION =	1.4.7
DIST =		${.CURDIR}/dist
BTOP_SRCS =	btop.cpp btop_cli.cpp btop_config.cpp btop_draw.cpp \
		btop_input.cpp btop_log.cpp btop_menu.cpp btop_shared.cpp \
		btop_theme.cpp btop_tools.cpp main.cpp \
		osx/btop_collect.cpp osx/sensors.cpp osx/smc.cpp
BTOP_OBJS =	${BTOP_SRCS:T:.cpp=.o}
BTOP_CXXFLAGS =	--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
		-std=c++23 -O2 -DFMT_HEADER_ONLY -DNDEBUG \
		-I${.OBJDIR} -I${DIST}/src -isystem ${DIST}/include \
		-Wno-nullability-completeness
BTOP_LDFLAGS =	--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
		-fuse-ld=${LD} -F${MINKAR_SDKROOT}/System/Library/Frameworks \
		-framework IOKit -framework CoreFoundation

all: to-sysroot

config.h: ${DIST}/src/config.h.in
	sed -e 's|@GIT_COMMIT@||' -e 's|@COMPILER@|clang|' \
	    -e "s|@COMPILER_VERSION@|$$(${CXX} -dumpversion)|" \
	    -e 's|@CONFIGURE_COMMAND@|bmake (Minkar)|' ${.ALLSRC} > ${.TARGET}

.for s in ${BTOP_SRCS}
${s:T:.cpp=.o}: ${DIST}/src/${s} config.h
	${CXX} ${BTOP_CXXFLAGS} -c ${DIST}/src/${s} -o ${.TARGET}
.endfor

btop: ${BTOP_OBJS}
	${CXX} ${BTOP_LDFLAGS} -o ${.TARGET} ${BTOP_OBJS}

to-sysroot: btop
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/share/man/man1 \
	    ${SYSROOT_DIR}/usr/share/btop/themes
	install -m 0555 btop ${SYSROOT_DIR}/usr/bin/
	install -m 0444 ${.CURDIR}/btop.1 ${SYSROOT_DIR}/usr/share/man/man1/
	install -m 0444 ${DIST}/themes/*.theme ${SYSROOT_DIR}/usr/share/btop/themes/

clean:
	rm -f btop ${BTOP_OBJS} config.h

