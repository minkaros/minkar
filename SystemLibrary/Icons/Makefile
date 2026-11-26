# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Atlas's icons (README.md): the PNGs in /System/Library/Icons, with the
# license, the authors and the SVG sources beside them.

SUBDIR =
DIR =		${SYSROOT_DIR}/System/Library/Icons

STAGE =	${.OBJDIR}/icons

all: install-icons

install-icons: .PHONY
	# Made in the object directory and copied by content: unchanged icons
	# keep their dates, removed ones go.
	rm -rf ${STAGE}
	mkdir -p ${STAGE} ${DIR}
	# A group left empty (a renamed one, in a tree copied over) is skipped.
	cd ${.CURDIR}/png && for g in *; do \
		ls $$g/*.png >/dev/null 2>&1 || continue; \
		mkdir -p ${STAGE}/$$g && \
		install -C -o 0 -g 0 -m 0644 $$g/*.png ${STAGE}/$$g/ || exit 1; \
	done
	mkdir -p ${STAGE}/Sources
	cd ${.CURDIR}/svg && for g in *; do \
		ls $$g/*.svgz >/dev/null 2>&1 || continue; \
		mkdir -p ${STAGE}/Sources/$$g && \
		install -C -o 0 -g 0 -m 0644 $$g/*.svgz ${STAGE}/Sources/$$g/ || exit 1; \
	done
	install -C -o 0 -g 0 -m 0644 ${.CURDIR}/COPYING ${.CURDIR}/AUTHORS ${.CURDIR}/README.md ${STAGE}/
	rsync -rlc --delete ${STAGE}/ ${DIR}/

.include <bsd.prog.mk>
