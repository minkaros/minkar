# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# locale: the UTF-8 locales in /usr/share/locale, made by
# tools/locale/mklocales.py from data/:
#   unicode/    Unicode 16.0.0's UnicodeData.txt and EastAsianWidth.txt
#               (Unicode License v3, unicode/LICENSE): LC_CTYPE's classes,
#               case maps and widths
#   *def/       FreeBSD main's share/{timedef,monetdef,numericdef,msgdef}
#               (from CLDR, Unicode License v3), unchanged: LC_TIME,
#               LC_MONETARY, LC_NUMERIC and LC_MESSAGES
# Every <language>_<COUNTRY>.UTF-8 shares UTF-8/LC_CTYPE and
# UTF-8/LC_COLLATE (code point order).

SUBDIR =

all: to-sysroot

to-sysroot: .PHONY
	rm -rf ${.OBJDIR}/out
	python3 -B ${ROOT_SOURCE_DIR}/tools/locale/mklocales.py \
	    --data ${.CURDIR}/data --out ${.OBJDIR}/out
	mkdir -p ${SYSROOT_DIR}/usr/share/locale
	cd ${.OBJDIR}/out && cp -R . ${SYSROOT_DIR}/usr/share/locale/
