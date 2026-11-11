# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# /System/Library/User Template/Non_localized: the folders a new home
# starts with, as on the reference system. Apps take some of them for
# granted (TextMate writes ~/Library/Logs, for one). The image copies it
# into the test account's home and root's (Image/mkdiskimage.py); the
# account tools do so for new accounts.

TEMPLATE =	${SYSROOT_DIR}/System/Library/User Template/Non_localized

# MODE PATH, relative to the template.
DIRS = \
	0700:Desktop \
	0700:Documents \
	0700:Downloads \
	0700:Library \
	0755:Library/Application_Support \
	0755:Library/Caches \
	0755:Library/Fonts \
	0755:Library/Logs \
	0700:Library/Preferences \
	0700:Movies \
	0700:Music \
	0700:Pictures \
	0755:Public \
	0733:Public/Drop_Box

all: template

template: ${.CURDIR}/Makefile
.for d in ${DIRS}
	mkdir -p "${TEMPLATE}/${d:C/^[0-7]*://:S/_/ /g}"
	chmod ${d:C/:.*//} "${TEMPLATE}/${d:C/^[0-7]*://:S/_/ /g}"
.endfor

.PHONY: all template

.include <bsd.prog.mk>
