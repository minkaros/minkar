# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# System sounds: /System/Library/Sounds, made by gen-sounds.py at build
# time (Minkar's own, synthesized; nothing recorded or taken from
# elsewhere). Chime is the alert sound.

SUBDIR =

SOUNDS =	Chime Notice Error Sent Received Trash Pop Tink
DIR =		${SYSROOT_DIR}/System/Library/Sounds

all: install-sounds

sounds.stamp: ${.CURDIR}/gen-sounds.py
	python3 -I ${.CURDIR}/gen-sounds.py ${.OBJDIR}/sounds
	touch ${.TARGET}

install-sounds: sounds.stamp .PHONY
	mkdir -p ${DIR}
.for s in ${SOUNDS}
	install -C -o 0 -g 0 -m 0644 ${.OBJDIR}/sounds/${s}.aiff ${DIR}/
.endfor

.include <bsd.prog.mk>
