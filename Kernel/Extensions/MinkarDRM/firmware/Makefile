# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# AMD's GPU microcode for MinkarAMDGPU (ADR 0002): fetched from
# linux-firmware at MANIFEST's tag, checked, and installed unchanged with
# its license in /usr/share/firmware/amdgpu.

SUBDIR =
FW_CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
DIR =		${SYSROOT_DIR}/usr/share/firmware/amdgpu

all: install-firmware

install-firmware: .PHONY
	src=$$(sh ${.CURDIR}/fetch.sh ${.CURDIR}/MANIFEST ${FW_CACHE}) && \
	mkdir -p ${DIR} && \
	for f in $$(awk '!/^#/ && $$1 != "tag" && NF == 2 { print $$1 }' ${.CURDIR}/MANIFEST); do \
		install -o 0 -g 0 -m 0644 $$src/$$f ${DIR}/; \
	done

.include <bsd.prog.mk>
