# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate BC's probe library and runner, in /usr/local/tests/bc.

SUBDIR =
DEST =	${SYSROOT_DIR}/usr/local/tests/bc
PROBE_FLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-incompatible-sysroot

all: install-bc

bcprobe.dylib: ${.CURDIR}/bcprobe.c
	${CC} ${PROBE_FLAGS} -O2 -Wall -dynamiclib -fuse-ld=${LD} \
	    -install_name /usr/local/tests/bc/bcprobe.dylib -o ${.TARGET} ${.ALLSRC}

install-bc: .PHONY bcprobe.dylib
	mkdir -p ${DEST}
	install -C -m 0644 bcprobe.dylib ${DEST}/bcprobe.dylib
	install -C -m 0755 ${.CURDIR}/../bc-run.sh ${DEST}/bc-run.sh

CLEANFILES += bcprobe.dylib

.include <bsd.prog.mk>
