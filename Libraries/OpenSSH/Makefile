# OpenSSH (portable 10.6p1) built without OpenSSL: sshd, ssh, scp, sftp,
# ssh-keygen and friends (architecture plan, Phase 2, gate M2). See
# README.ravynos. The source lists follow openssh/Makefile.in;
# config.h is what openssh/configure produced for Minkar (see README).

SUBDIR =
MK_WERROR = no
LIB = ssh

OPENSSH =	${.CURDIR}/openssh
.PATH: ${OPENSSH} ${OPENSSH}/openbsd-compat

# libssh.a: the shared code plus openbsd-compat.
LIBOPENSSH =	ssh_api.c ssherr.c sshbuf.c sshkey.c sshbuf-getput-basic.c \
	sshbuf-misc.c sshbuf-getput-crypto.c krl.c bitmap.c
LIBSSH =	authfd.c authfile.c canohost.c channels.c cipher.c cipher-aes.c \
	cipher-aesctr.c cleanup.c compat.c fatal.c hostfile.c log.c match.c \
	moduli.c nchan.c packet.c readpass.c ttymodes.c xmalloc.c addr.c \
	addrmatch.c atomicio.c dispatch.c mac.c misc.c utf8.c monitor_fdpass.c \
	rijndael.c ssh-ecdsa.c ssh-ecdsa-sk.c ssh-ed25519-sk.c ssh-rsa.c dh.c \
	msg.c dns.c entropy.c gss-genr.c umac.c umac128.c smult_curve25519_ref.c \
	poly1305.c chacha.c cipher-chachapoly.c cipher-chachapoly-libcrypto.c \
	ssh-ed25519.c digest-openssl.c digest-libc.c libcrux-mlkem-mldsa.c \
	ssh-mldsa-eddsa.c hmac.c ed25519.c ed25519-openssl.c kex.c kex-names.c \
	kexdh.c kexgex.c kexecdh.c kexc25519.c kexgexc.c kexgexs.c \
	kexmlkem768ecdh.c kexsntrup761x25519.c kexmlkem768x25519.c sntrup761.c \
	kexgen.c sftp-realpath.c platform-pledge.c platform-tracing.c \
	platform-misc.c sshbuf-io.c misc-agent.c ssherr-libcrypto.c
COMPAT_OPENBSD = arc4random.c arc4random_uniform.c base64.c basename.c \
	bcrypt_pbkdf.c bindresvport.c blowfish.c daemon.c dirname.c \
	explicit_bzero.c fmt_scaled.c freezero.c fnmatch.c getcwd.c \
	getgrouplist.c getopt_long.c getrrsetbyname.c glob.c inet_aton.c \
	inet_ntoa.c inet_ntop.c md5.c memmem.c mktemp.c pwcache.c \
	readpassphrase.c reallocarray.c recallocarray.c rresvport.c setenv.c \
	setproctitle.c sha1.c sha2.c sigact.c strcasestr.c strlcat.c strlcpy.c \
	strmode.c strndup.c strnlen.c strptime.c strsep.c strtoll.c strtonum.c \
	strtoull.c strtoul.c timingsafe_bcmp.c vis.c
COMPAT_COMPAT = bsd-asprintf.c bsd-closefrom.c bsd-cygwin_util.c bsd-err.c \
	bsd-flock.c bsd-getentropy.c bsd-getline.c bsd-getpagesize.c \
	bsd-getpeereid.c bsd-malloc.c bsd-misc.c bsd-nextstep.c bsd-openpty.c \
	bsd-poll.c bsd-pselect.c bsd-setres_id.c bsd-signal.c bsd-snprintf.c \
	bsd-statvfs.c bsd-timegm.c bsd-waitpid.c fake-rfc2553.c \
	getrrsetbyname-ldns.c kludge-fd_set.c openssl-compat.c \
	libressl-api-compat.c xcrypt.c
COMPAT_PORTS =	port-aix.c port-irix.c port-linux.c port-linux-selinux.c \
	port-prngd.c port-solaris.c port-net.c port-uw.c
SRCS =		${LIBOPENSSH} ${LIBSSH} ${COMPAT_OPENBSD} ${COMPAT_COMPAT} \
		${COMPAT_PORTS}

# The programs.
SKOBJS =	ssh-sk-client.o
P11OBJS =	ssh-pkcs11-client.o
SFTP_CLIENT =	sftp-common.o sftp-client.o sftp-glob.o ssherr-nolibcrypto.o
SSH_OBJS =	ssh.o readconf.o clientloop.o sshtty.o sshconnect.o \
	sshconnect2.o mux.o ssh-pkcs11.o ${SKOBJS}
SSHD_OBJS =	sshd.o platform-listen.o servconf.o srclimit.o groupaccess.o \
	auth2-methods.o dns.o fatal.o compat.o utf8.o authfd.o canohost.o \
	${P11OBJS} ${SKOBJS}
SSHD_SESSION_OBJS = sshd-session.o auth-rhosts.o auth-passwd.o audit.o \
	audit-bsm.o audit-linux.o platform.o sshpty.o sshlogin.o servconf.o \
	serverloop.o auth.o auth2.o auth2-methods.o auth-options.o session.o \
	auth2-chall.o groupaccess.o auth-bsdauth.o auth2-hostbased.o \
	auth2-kbdint.o auth2-none.o auth2-passwd.o auth2-pubkey.o \
	auth2-pubkeyfile.o monitor.o monitor_wrap.o auth-krb5.o auth2-gss.o \
	gss-serv.o gss-serv-krb5.o loginrec.o auth-pam.o auth-shadow.o \
	auth-sia.o sftp-server.o sftp-common.o uidswap.o platform-listen.o \
	${P11OBJS} ${SKOBJS}
SSHD_AUTH_OBJS = sshd-auth.o auth2-methods.o auth-rhosts.o auth-passwd.o \
	sshpty.o sshlogin.o servconf.o serverloop.o auth.o auth2.o \
	auth-options.o session.o auth2-chall.o groupaccess.o auth-bsdauth.o \
	auth2-hostbased.o auth2-kbdint.o auth2-none.o auth2-passwd.o \
	auth2-pubkey.o auth2-pubkeyfile.o auth2-gss.o gss-serv.o \
	gss-serv-krb5.o monitor_wrap.o auth-krb5.o audit.o audit-bsm.o \
	audit-linux.o platform.o loginrec.o auth-pam.o auth-shadow.o \
	auth-sia.o sandbox-null.o sandbox-rlimit.o sandbox-darwin.o \
	sandbox-seccomp-filter.o sandbox-capsicum.o sandbox-solaris.o \
	sftp-server.o sftp-common.o uidswap.o ${P11OBJS} ${SKOBJS}
SSH_KEYGEN_OBJS = ssh-keygen.o sshsig.o ssh-pkcs11.o ${SKOBJS}
SSH_ADD_OBJS =	ssh-add.o ${P11OBJS} ${SKOBJS}
SSH_AGENT_OBJS = ssh-agent.o ${P11OBJS} ${SKOBJS}
SSH_KEYSCAN_OBJS = ssh-keyscan.o ${P11OBJS} ${SKOBJS}
SCP_OBJS =	scp.o progressmeter.o ${SFTP_CLIENT}
SFTP_OBJS =	sftp.o sftp-usergroup.o progressmeter.o ${SFTP_CLIENT}
SFTP_SERVER_OBJS = sftp-common.o sftp-server.o sftp-server-main.o \
	ssherr-nolibcrypto.o

BINPROGS =	ssh scp sftp ssh-keygen ssh-add ssh-agent ssh-keyscan
SBINPROGS =	sshd
LIBEXECPROGS =	sshd-session sshd-auth sftp-server
PROGS =		${BINPROGS} ${SBINPROGS} ${LIBEXECPROGS}

# Where things are, as in openssh/Makefile.in's PATHS.
PATHS =	-DSSHDIR=\"/etc/ssh\" \
	-D_PATH_SSH_PROGRAM=\"/usr/bin/ssh\" \
	-D_PATH_SSH_ASKPASS_DEFAULT=\"/usr/libexec/ssh-askpass\" \
	-D_PATH_SFTP_SERVER=\"/usr/libexec/sftp-server\" \
	-D_PATH_SSH_KEY_SIGN=\"/usr/libexec/ssh-keysign\" \
	-D_PATH_SSHD_SESSION=\"/usr/libexec/sshd-session\" \
	-D_PATH_SSHD_AUTH=\"/usr/libexec/sshd-auth\" \
	-D_PATH_SSH_PKCS11_HELPER=\"/usr/libexec/ssh-pkcs11-helper\" \
	-D_PATH_SSH_SK_HELPER=\"/usr/libexec/ssh-sk-helper\" \
	-D_PATH_SSH_PIDDIR=\"/var/run\" \
	-D_PATH_PRIVSEP_CHROOT_DIR=\"/var/empty\"

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR} -I${.CURDIR}/include -I${OPENSSH} \
	-DHAVE_CONFIG_H ${PATHS} -fno-strict-aliasing -fno-builtin-memset \
	-Wno-nullability-completeness -Wno-pointer-sign -Wno-unused-parameter \
	-Wno-unused-result -Wno-sign-compare -Wno-cast-qual \
	-Wno-missing-prototypes -Wno-strict-prototypes -Wno-shadow \
	-Wno-cast-align -Wno-unused-function -Wno-missing-variable-declarations \
	-Wno-format-nonliteral -Wno-old-style-definition -Wno-unused-variable
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

# sshd-session and sshd-auth do the PAM conversation and BSM audit.
LDADD.sshd-session =	-lpam -lbsm
LDADD.sshd-auth =	-lpam -lbsm

all: ${PROGS} install-sysroot

.for p in ${PROGS}
_OBJS.${p} = ${${p:tu:S/-/_/g}_OBJS}
${p}: ${_OBJS.${p}} lib${LIB}.a
	${CC} ${LDFLAGS} -o ${.TARGET} ${_OBJS.${p}} lib${LIB}.a ${LDADD.${p}} -lresolv
CLEANFILES += ${p} ${_OBJS.${p}}
.endfor

ETCSSH =	${SYSROOT_DIR}/private/etc/ssh

install-sysroot: ${PROGS}
	mkdir -p ${SYSROOT_DIR}/usr/bin ${SYSROOT_DIR}/usr/sbin \
	    ${SYSROOT_DIR}/usr/libexec ${ETCSSH} ${SYSROOT_DIR}/private/var/empty
	chmod 0555 ${SYSROOT_DIR}/private/var/empty
.for p in ${BINPROGS}
	install -m 0555 ${p} ${SYSROOT_DIR}/usr/bin/
.endfor
.for p in ${SBINPROGS}
	install -m 0555 ${p} ${SYSROOT_DIR}/usr/sbin/
.endfor
.for p in ${LIBEXECPROGS}
	install -m 0555 ${p} ${SYSROOT_DIR}/usr/libexec/
.endfor
	install -m 0555 ${.CURDIR}/sshd-keygen-wrapper ${SYSROOT_DIR}/usr/libexec/
	install -m 0644 ${.CURDIR}/sshd_config ${ETCSSH}/sshd_config
	install -m 0644 ${OPENSSH}/ssh_config ${ETCSSH}/ssh_config
	install -m 0644 ${OPENSSH}/moduli ${ETCSSH}/moduli
	mkdir -p ${SYSROOT_DIR}/private/etc/pam.d
	install -m 0644 ${.CURDIR}/pam.d/sshd ${SYSROOT_DIR}/private/etc/pam.d/sshd

.PHONY: install-sysroot

.include <bsd.lib.mk>
