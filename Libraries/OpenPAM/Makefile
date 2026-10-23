# OpenPAM: libpam.2.dylib, the security/ headers and the PAM modules
# (architecture plan, Phase 2: login, later sudo and sshd). See
# README.ravynos. The list of sources follows openpam/lib/libpam/Makefile.am.

SUBDIR =
MK_WERROR = no
LIB = pam

OPENPAM =	${.CURDIR}/openpam
.PATH: ${OPENPAM}/lib/libpam

SRCS = \
	openpam_asprintf.c openpam_borrow_cred.c openpam_check_owner_perms.c \
	openpam_configure.c openpam_constants.c openpam_dispatch.c \
	openpam_dynamic.c openpam_features.c openpam_findenv.c \
	openpam_free_data.c openpam_free_envlist.c openpam_get_feature.c \
	openpam_get_option.c openpam_load.c openpam_log.c openpam_nullconv.c \
	openpam_readline.c openpam_readlinev.c openpam_readword.c \
	openpam_restore_cred.c openpam_set_option.c openpam_set_feature.c \
	openpam_static.c openpam_straddch.c openpam_strlcat.c \
	openpam_strlcpy.c openpam_strlset.c openpam_subst.c \
	openpam_vasprintf.c openpam_ttyconv.c \
	pam_acct_mgmt.c pam_authenticate.c pam_chauthtok.c pam_close_session.c \
	pam_end.c pam_error.c pam_get_authtok.c pam_get_data.c pam_get_item.c \
	pam_get_user.c pam_getenv.c pam_getenvlist.c pam_info.c \
	pam_open_session.c pam_prompt.c pam_putenv.c pam_set_data.c \
	pam_set_item.c pam_setcred.c pam_setenv.c pam_start.c pam_strerror.c \
	pam_verror.c pam_vinfo.c pam_vprompt.c

MODULES =	pam_deny pam_permit pam_return pam_unix

HEADERS =	openpam.h openpam_attr.h openpam_version.h pam_appl.h \
		pam_constants.h pam_modules.h pam_types.h

CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-DHAVE_CONFIG_H -I${.CURDIR} -I${OPENPAM}/include -I${OPENPAM}/lib/libpam \
	-Wno-nullability-completeness

LDFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

PAMLIB =	${.OBJDIR}/libpam.2.dylib
PAMDIR =	/usr/lib/pam

all: ${PAMLIB} ${MODULES:S/$/.so.2/} install-sdk install-sysroot

${PAMLIB}: lib${LIB}.a
	${CC} ${LDFLAGS} -shared -dylib -o ${.TARGET} \
	  -Wl,-install_name,/usr/lib/libpam.2.dylib \
	  -Wl,-current_version,3.0.0 -Wl,-compatibility_version,3.0.0 \
	  -Wl,-force_load,${.OBJDIR}/lib${LIB}.a

# Modules are bundles that export only their pam_sm_* entry points.
.for m in ${MODULES}
${m}.so.2: ${OPENPAM}/modules/${m}/${m}.c ${PAMLIB}
	${CC} ${CFLAGS} ${LDFLAGS} -bundle -o ${.TARGET} \
	  ${OPENPAM}/modules/${m}/${m}.c ${PAMLIB} \
	  -Wl,-exported_symbol,_pam_sm_\*
.endfor

install-sdk: ${PAMLIB}
	mkdir -p ${RAVYN_SDKROOT}/usr/lib ${RAVYN_SDKROOT}/usr/include/security
	cp -f ${PAMLIB} ${RAVYN_SDKROOT}/usr/lib/
	ln -sf libpam.2.dylib ${RAVYN_SDKROOT}/usr/lib/libpam.dylib
.for h in ${HEADERS}
	cp -f ${OPENPAM}/include/security/${h} ${RAVYN_SDKROOT}/usr/include/security/
.endfor

install-sysroot: ${PAMLIB} ${MODULES:S/$/.so.2/}
	mkdir -p ${SYSROOT_DIR}/usr/lib ${SYSROOT_DIR}${PAMDIR}
	cp -f ${PAMLIB} ${SYSROOT_DIR}/usr/lib/
	ln -sf libpam.2.dylib ${SYSROOT_DIR}/usr/lib/libpam.dylib
.for m in ${MODULES}
	cp -f ${m}.so.2 ${SYSROOT_DIR}${PAMDIR}/
.endfor

CLEANFILES +=	${PAMLIB} ${MODULES:S/$/.so.2/}

.PHONY: install-sdk install-sysroot

.include <bsd.lib.mk>
