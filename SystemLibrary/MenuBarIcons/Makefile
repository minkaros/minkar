# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# The menu bar's icons (README.md): the PNGs in
# /System/Library/MenuBarIcons, with the license and the SVG sources.

SUBDIR =
DIR =		${SYSROOT_DIR}/System/Library/MenuBarIcons
STAGE =		${.OBJDIR}/menubaricons

all: install-icons

# Exactly what is here: an icon no longer listed goes.
install-icons: .PHONY
	rm -rf ${STAGE}
	mkdir -p ${STAGE}/Sources ${DIR}
	install -C -o 0 -g 0 -m 0644 ${.CURDIR}/png/*.png ${STAGE}/
	install -C -o 0 -g 0 -m 0644 ${.CURDIR}/svg/*.svg ${STAGE}/Sources/
	install -C -o 0 -g 0 -m 0644 ${.CURDIR}/LICENSE ${.CURDIR}/README.md ${STAGE}/
	rsync -rlc --delete ${STAGE}/ ${DIR}/

.include <bsd.prog.mk>
