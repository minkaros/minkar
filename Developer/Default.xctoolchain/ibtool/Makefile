# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# ibtool: compiles Interface Builder documents (.xib) to binary nibs for
# the build host (BSD-2, see ibtool.py); nibdump prints a binary nib.
# Host tests: python3 -B tests/test_ibtool.py

all: ${TOOLS}/ibtool ${TOOLS}/nibdump ${TOOLS}/nibarchive.py

${TOOLS}/ibtool: ${.CURDIR}/ibtool.py
	mkdir -p ${TOOLS}
	rsync -cL ${.ALLSRC} ${.TARGET}
	chmod 755 ${.TARGET}

${TOOLS}/nibdump: ${.CURDIR}/nibdump.py
	mkdir -p ${TOOLS}
	rsync -cL ${.ALLSRC} ${.TARGET}
	chmod 755 ${.TARGET}

${TOOLS}/nibarchive.py: ${.CURDIR}/nibarchive.py
	mkdir -p ${TOOLS}
	rsync -cL ${.ALLSRC} ${.TARGET}

test:
	python3 -B ${.CURDIR}/tests/test_ibtool.py
