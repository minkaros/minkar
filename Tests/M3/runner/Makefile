# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# /usr/local/tests/m3/run.sh, which boottest's M3 gate runs.

SUBDIR =

all: install-runner

install-runner: .PHONY
	mkdir -p ${SYSROOT_DIR}/usr/local/tests/m3
	install -m 0755 ${.CURDIR}/run-m3.sh ${SYSROOT_DIR}/usr/local/tests/m3/run.sh

.include <bsd.prog.mk>
