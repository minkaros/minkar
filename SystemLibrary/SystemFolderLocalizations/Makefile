# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# The names a folder marked with an empty .localized file is shown with
# (Desktop, Documents, Applications...), one table per language:
# /System/Library/CoreServices/SystemFolderLocalizations/<lang>.lproj.
# NSFileManager's displayNameAtPath: reads them.

SUBDIR =

LANGS =		en es
DIR =		${SYSROOT_DIR}/System/Library/CoreServices/SystemFolderLocalizations

all: install-names

install-names: .PHONY
.for l in ${LANGS}
	mkdir -p ${DIR}/${l}.lproj
	install -C -o 0 -g 0 -m 0644 ${.CURDIR}/${l}.lproj/SystemFolderLocalizations.strings ${DIR}/${l}.lproj/
.endfor

.include <bsd.prog.mk>
