# Base configuration in /etc (architecture plan, Phase 2): the user and
# group databases, login shells and the PAM policies. (Makefile.freebsd is
# the FreeBSD-era file list, kept for reference while it is ported.)

SUBDIR =

ETC =	${SYSROOT_DIR}/private/etc

all: install-etc

install-etc: .PHONY
	mkdir -p ${ETC}/pam.d
	install -o 0 -g 0 -m 0600 ${.CURDIR}/master.passwd ${ETC}/
.for f in passwd group shells motd
	install -o 0 -g 0 -m 0644 ${.CURDIR}/${f} ${ETC}/
.endfor
.for f in login other
	install -o 0 -g 0 -m 0644 ${.CURDIR}/pam.d/${f} ${ETC}/pam.d/
.endfor

.include <bsd.prog.mk>
