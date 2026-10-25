# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
#
# libresolv.9.dylib: the DNS resolver library (BIND 9 API, res_9_* names),
# a library of its own on macOS, outside libSystem. The sources are those
# of libresolv.xcodeproj; minkar/ has our portability headers and the DNS
# configuration fallback (/etc/resolv.conf while configd does not serve it).

SUBDIR =
MK_WERROR = no
LIB = resolv

SRCS = base64.c dns_async.c dns.c dns_util.c dst_api.c dst_hmac_link.c \
	dst_support.c ns_date.c ns_name.c ns_netint.c ns_parse.c ns_print.c \
	ns_samedomain.c ns_sign.c ns_ttl.c ns_verify.c res_comp.c res_data.c \
	res_debug.c res_findzonecut.c res_init.c res_mkquery.c res_mkupdate.c \
	res_query.c res_send.c res_sendsigned.c res_update.c dnsinfo_none.c
.PATH: ${.CURDIR}/minkar

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-std=gnu99 -fno-common -I${.CURDIR}/minkar -I${.CURDIR} \
	-I${ROOT_SOURCE_DIR}/BSD/usr.sbin/configd/dnsinfo \
	-Wno-nullability-completeness
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

RESOLVLIB =	${.OBJDIR}/libresolv.9.dylib

all: ${RESOLVLIB} install-sdk install-sysroot

${RESOLVLIB}: lib${LIB}.a
	${CC} ${LDFLAGS} -shared -dylib -o ${.TARGET} \
	  -Wl,-install_name,/usr/lib/libresolv.9.dylib \
	  -Wl,-current_version,65 -Wl,-compatibility_version,1 \
	  -Wl,-force_load,${.OBJDIR}/lib${LIB}.a

install-sdk: ${RESOLVLIB}
	mkdir -p ${MINKAR_SDKROOT}/usr/lib
	cp -f ${RESOLVLIB} ${MINKAR_SDKROOT}/usr/lib/
	ln -sf libresolv.9.dylib ${MINKAR_SDKROOT}/usr/lib/libresolv.dylib

install-sysroot: ${RESOLVLIB}
	mkdir -p ${SYSROOT_DIR}/usr/lib
	cp -f ${RESOLVLIB} ${SYSROOT_DIR}/usr/lib/
	ln -sf libresolv.9.dylib ${SYSROOT_DIR}/usr/lib/libresolv.dylib

CLEANFILES +=	${RESOLVLIB}

.PHONY: install-sdk install-sysroot

.include <bsd.lib.mk>
