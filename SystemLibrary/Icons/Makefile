# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Atlas's icons (README.md): the PNGs in /System/Library/Icons, with the
# license, the authors and the SVG sources beside them.

SUBDIR =
DIR =		${SYSROOT_DIR}/System/Library/Icons

all: install-icons

install-icons: .PHONY
	rm -rf ${DIR}
	mkdir -p ${DIR}
	cd ${.CURDIR}/png && for g in *; do \
		mkdir -p ${DIR}/$$g && \
		install -o 0 -g 0 -m 0644 $$g/*.png ${DIR}/$$g/ || exit 1; \
	done
	mkdir -p ${DIR}/Sources
	cd ${.CURDIR}/svg && for g in *; do \
		mkdir -p ${DIR}/Sources/$$g && \
		install -o 0 -g 0 -m 0644 $$g/*.svgz ${DIR}/Sources/$$g/ || exit 1; \
	done
	install -o 0 -g 0 -m 0644 ${.CURDIR}/COPYING ${.CURDIR}/AUTHORS ${.CURDIR}/README.md ${DIR}/

.include <bsd.prog.mk>
