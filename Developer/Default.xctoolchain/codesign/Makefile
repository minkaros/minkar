# Ad-hoc code signing tool for the build host (BSD-2, see codesign.py).
# Host tests: python3 -B tests/test_codesign.py

all: ${TOOLS}/codesign

${TOOLS}/codesign: ${.CURDIR}/codesign.py
	mkdir -p ${TOOLS}
	cp -f ${.ALLSRC} ${.TARGET}
	chmod 755 ${.TARGET}

test:
	python3 -B ${.CURDIR}/tests/test_codesign.py
