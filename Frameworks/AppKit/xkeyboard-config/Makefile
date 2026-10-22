# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The ravynOS contributors
#
# xkeyboard-config: keyboard description data (keycodes, symbols, rules)
# for libxkbcommon. Data only, MIT/X11 licensed (see COPYING).
#
# Instead of meson, this reproduces the steps of meson.build and
# rules/meson.build (with compat-rules off) using only python3 and perl.
# When updating xkeyboard-config, re-check those files: the list of rules
# parts below must match rules/meson.build.

SUBDIR =

XKB_BASE =	/System/Library/KeyboardLayout
XKB_DEST =	${SYSROOT_DIR}${XKB_BASE}
RULES_SRC =	${.CURDIR}/rules
RULESETS =	base evdev

# Data directories installed as-is (meson.build: install_subdir).
KCGST_DIRS =	compat geometry keycodes symbols types

# rules/meson.build "parts", in order; @R@ is the ruleset name.
RULES_PARTS = \
	0000-hdr.part 0001-lists.part 0002-@R@.lists.part 0004-@R@.m_k.part \
	0005-l1_k.part 0006-l_k.part 0007-o_k.part 0008-ml_g.part \
	0009-m_g.part 0011-mlv_s.part 0013-ml_s.part 0015-ml1_s.part \
	0018-ml2_s.part 0020-ml3_s.part 0022-ml4_s.part 0026-@R@.m_s.part \
	0027-@R@.ml_s1.part 0033-ml_c.part 0034-ml1_c.part 0035-m_t.part \
	0036-lo_s.part 0037-l1o_s.part 0038-l2o_s.part 0039-l3o_s.part \
	0040-l4o_s.part 0042-o_s.part 0043-o_c.part 0044-o_t.part

PYTHON ?=	python3
PERL ?=		perl
VERSION !=	sed -n "s/^ *version: *'\([^']*\)'.*/\1/p" ${.CURDIR}/meson.build

all: install-data

# Generate the rules files and their .lst listings in the object dir.
generate:
.for r in ${RULESETS}
	${PYTHON} -B ${RULES_SRC}/merge.py \
	    ${RULES_PARTS:S/@R@/${r}/g:S,^,${RULES_SRC}/,} > ${.OBJDIR}/${r}.tmp
	mv -f ${.OBJDIR}/${r}.tmp ${.OBJDIR}/${r}
	${PERL} ${RULES_SRC}/xml2lst.pl ${RULES_SRC}/base.xml > ${.OBJDIR}/${r}.lst.tmp
	mv -f ${.OBJDIR}/${r}.lst.tmp ${.OBJDIR}/${r}.lst
.endfor

install-data: generate
	mkdir -p ${XKB_DEST}/rules
.for d in ${KCGST_DIRS}
	rm -rf ${XKB_DEST}/${d}
	cp -R ${.CURDIR}/${d} ${XKB_DEST}/
.endfor
	cp -f ${RULES_SRC}/README ${RULES_SRC}/xkb.dtd ${RULES_SRC}/xfree98 \
	    ${XKB_DEST}/rules/
.for r in ${RULESETS}
	cp -f ${.OBJDIR}/${r} ${.OBJDIR}/${r}.lst ${XKB_DEST}/rules/
	cp -f ${RULES_SRC}/base.xml ${XKB_DEST}/rules/${r}.xml
	cp -f ${RULES_SRC}/base.extras.xml ${XKB_DEST}/rules/${r}.extras.xml
.endfor
	# pkg-config data file, as meson.build generates it
	mkdir -p ${RAVYN_SDKROOT}/usr/share/pkgconfig
	printf '%s\n' 'xkb_base=${XKB_BASE}' '' \
	    'Name: XKeyboardConfig' \
	    'Description: X Keyboard configuration data' \
	    'Version: ${VERSION}' \
	    > ${RAVYN_SDKROOT}/usr/share/pkgconfig/xkeyboard-config.pc

clean:
.for r in ${RULESETS}
	rm -f ${.OBJDIR}/${r} ${.OBJDIR}/${r}.lst ${.OBJDIR}/${r}.tmp ${.OBJDIR}/${r}.lst.tmp
.endfor

.PHONY: all generate install-data clean
