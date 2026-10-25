# sudo 1.9.15p5 (ISC, LICENSE.md; approved for ravynOS 2026-10-24), built
# with its own configure for the Minkar target (cross build) in the
# object directory, then GNU make. PAM (/etc/pam.d/sudo), BSM audit,
# syslog, the sudoers plugin linked in; no LDAP, OpenSSL, zlib, NLS or
# log server; no login classes (none on Minkar, as on macOS).
# Installs sudo (set-user-ID root) in /usr/bin and visudo in /usr/sbin;
# /etc/sudo.conf, sudoers and sudoers.d come from BSD/etc.

SUBDIR =
MK_WERROR = no

SUDO_CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -O2
SUDO_LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

CONFIGURE_ARGS = --host=x86_64-apple-darwin --build=x86_64-pc-linux-gnu \
	--prefix=/usr --sysconfdir=/etc --localstatedir=/var \
	--with-rundir=/var/run/sudo --with-vardir=/var/db/sudo \
	--disable-shared --enable-static-sudoers --disable-shared-libutil \
	--disable-root-mailer --without-sendmail --enable-shell-sets-home \
	--disable-log-server --disable-log-client --with-logging=syslog \
	--with-bsm-audit --with-pam --without-logincap --without-lecture \
	--without-ldap --disable-sasl --disable-openssl --disable-gcrypt \
	--disable-zlib --disable-nls

all: install-sudo

config.status: ${.CURDIR}/configure
	env CC=${CC} CFLAGS="${SUDO_CFLAGS}" LDFLAGS="${SUDO_LDFLAGS}" \
	    AR=${AR} RANLIB=${RANLIB} ${.CURDIR}/configure ${CONFIGURE_ARGS}

build-sudo: config.status .PHONY
	# DEVEL= : keep the shipped gram.c, toke.c, getdate.c (no yacc/flex).
	env MAKEFLAGS= ${GMAKE} -j${.MAKE.JOBS} DEVEL=

install-sudo: build-sudo .PHONY
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/sbin
	install -o 0 -g 0 -m 4111 src/sudo ${SYSROOT_DIR}/usr/bin/sudo
	install -o 0 -g 0 -m 0555 plugins/sudoers/visudo ${SYSROOT_DIR}/usr/sbin/visudo

clean:
	-env MAKEFLAGS= ${GMAKE} distclean

.include <bsd.obj.mk>
