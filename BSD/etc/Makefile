# Base configuration in /etc (architecture plan, Phase 2): the user and
# group databases, login shells, the PAM policies and resolv.conf, a link to
# the one dhcpclient writes, as on macOS. (Makefile.freebsd is
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
	ln -sfn ../var/run/resolv.conf ${ETC}/resolv.conf

.include <bsd.prog.mk>
