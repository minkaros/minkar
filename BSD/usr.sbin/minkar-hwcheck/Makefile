# minkar-hwcheck: Minkar's own unattended hardware check (a shell script,
# not a port), installed as /usr/local/bin/minkar-hwcheck.

SCRIPT =	minkar-hwcheck
BINDIR_SYSROOT = ${SYSROOT_DIR}/usr/local/bin

all: install

install:
	mkdir -p ${BINDIR_SYSROOT}
	install -m 0755 ${.CURDIR}/${SCRIPT}.sh ${BINDIR_SYSROOT}/${SCRIPT}

obj depend includes clean cleandir:
