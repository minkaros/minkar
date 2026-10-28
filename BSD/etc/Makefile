# Base configuration in /etc (architecture plan, Phase 2): the user and
# group databases, login shells, the shell profile (profile, shrc:
# environment, aliases, prompt), the PAM policies, services and protocols
# (FreeBSD's, from IANA's registries), the root CA certificates
# (ssl/cert.pem, Mozilla's), sudo's configuration, and
# resolv.conf, a link to the one dhcpclient writes, as on macOS.

SUBDIR =

# Run from the top of the tree (env SUBDIR=BSD/etc bmake ...): here alone,
# SYSROOT_DIR is unset and /private/etc would be the build host's.
.if empty(SYSROOT_DIR)
.error SYSROOT_DIR is not set: build BSD/etc from the top of the tree
.endif
ETC =	${SYSROOT_DIR}/private/etc

all: install-etc

install-etc: .PHONY
	mkdir -p ${ETC}/pam.d
	install -o 0 -g 0 -m 0600 ${.CURDIR}/master.passwd ${ETC}/
.for f in passwd group shells motd services protocols profile shrc
	install -o 0 -g 0 -m 0644 ${.CURDIR}/${f} ${ETC}/
.endfor
.for f in login other passwd su sudo
	install -o 0 -g 0 -m 0644 ${.CURDIR}/pam.d/${f} ${ETC}/pam.d/
.endfor
	ln -sfn ../var/run/resolv.conf ${ETC}/resolv.conf
	# Root CA certificates (Mozilla's, MPL-2.0: ssl/README).
	mkdir -p ${ETC}/ssl
	install -o 0 -g 0 -m 0644 ${.CURDIR}/ssl/cert.pem ${ETC}/ssl/
	# sudo: its configuration is for root's eyes only.
	mkdir -p ${ETC}/sudoers.d
	install -o 0 -g 0 -m 0644 ${.CURDIR}/sudo.conf ${ETC}/
	install -o 0 -g 0 -m 0440 ${.CURDIR}/sudoers ${ETC}/
.for f in eject-sudoers shutdown-sudoers
	install -o 0 -g 0 -m 0440 ${.CURDIR}/sudoers.d/${f} ${ETC}/sudoers.d/
.endfor

.include <bsd.prog.mk>
