# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#

SUBDIR =

all: install-runner

install-runner: .PHONY
	mkdir -p ${SYSROOT_DIR}/usr/local/tests/w0
	install -C -m 0755 ${.CURDIR}/run-w0.sh ${SYSROOT_DIR}/usr/local/tests/w0/run.sh

.include <bsd.prog.mk>
