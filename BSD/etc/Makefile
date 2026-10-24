# Base configuration in /etc (architecture plan, Phase 2): the user and
# group databases, login shells, the PAM policies, services and protocols
# (FreeBSD's, from IANA's registries), sudo's configuration, and
# resolv.conf, a link to the one dhcpclient writes, as on macOS.
# (Makefile.freebsd is the FreeBSD-era file list, kept for reference
# while it is ported.)

SUBDIR =

ETC =	${SYSROOT_DIR}/private/etc

all: install-etc

install-etc: .PHONY
	mkdir -p ${ETC}/pam.d
	install -o 0 -g 0 -m 0600 ${.CURDIR}/master.passwd ${ETC}/
.for f in passwd group shells motd services protocols
	install -o 0 -g 0 -m 0644 ${.CURDIR}/${f} ${ETC}/
.endfor
.for f in login other su sudo
	install -o 0 -g 0 -m 0644 ${.CURDIR}/pam.d/${f} ${ETC}/pam.d/
.endfor
	ln -sfn ../var/run/resolv.conf ${ETC}/resolv.conf
	# sudo: its configuration is for root's eyes only.
	mkdir -p ${ETC}/sudoers.d
	install -o 0 -g 0 -m 0644 ${.CURDIR}/sudo.conf ${ETC}/
	install -o 0 -g 0 -m 0440 ${.CURDIR}/sudoers ${ETC}/
.for f in eject-sudoers shutdown-sudoers
	install -o 0 -g 0 -m 0440 ${.CURDIR}/sudoers.d/${f} ${ETC}/sudoers.d/
.endfor

.include <bsd.prog.mk>
