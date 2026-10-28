# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# libiconv: FreeBSD main's Citrus iconv (lib/libc/iconv and
# lib/libiconv_modules), as macOS has it since macOS 14: one library,
# /usr/lib/libiconv.2.dylib (libiconv.dylib links to it), and <iconv.h>.
# FreeBSD loads each encoding module from /usr/lib/i18n with dlopen; here
# the modules are linked in and citrus_module_static.c finds them in a
# table. GNU libiconv's names (libiconv_open, libiconv...) are exported
# too. The conversion tables are built by i18n/ (share/i18n in FreeBSD).

SUBDIR =
MK_WERROR = no
LIB = iconv
MODULES = BIG5 DECHanyu EUC EUCTW GBK2K HZ iconv_none iconv_std ISO2022 JOHAB mapper_646 mapper_none mapper_serial mapper_std mapper_zone MSKanji UES UTF1632 UTF7 UTF8 VIQR ZW 
.PATH: ${.CURDIR}/citrus ${MODULES:S,^,${.CURDIR}/modules/,}

SRCS = citrus_bcs.c citrus_bcs_strtol.c citrus_bcs_strtoul.c \
	citrus_csmapper.c citrus_db.c citrus_db_factory.c citrus_db_hash.c \
	citrus_esdb.c citrus_hash.c citrus_iconv.c citrus_lookup.c \
	citrus_lookup_factory.c citrus_mapper.c citrus_memstream.c \
	citrus_mmap.c citrus_module.c citrus_none.c citrus_pivot_factory.c \
	citrus_prop.c citrus_stdenc.c bsd_iconv.c \
	__iconv.c __iconv_free_list.c __iconv_get_list.c iconv.c \
	iconv_canonicalize.c iconv_close.c iconv_open.c iconv_open_into.c \
	iconv_set_relocation_prefix.c iconvctl.c iconvlist.c \
	citrus_module_static.c libiconv_aliases.c
SRCS += citrus_big5.c citrus_dechanyu.c citrus_euc.c citrus_euctw.c citrus_gbk2k.c citrus_hz.c citrus_iconv_none.c citrus_iconv_std.c citrus_iso2022.c citrus_johab.c citrus_mapper_646.c citrus_mapper_none.c citrus_mapper_serial.c citrus_mapper_std.c citrus_mapper_zone.c citrus_mskanji.c citrus_ues.c citrus_utf1632.c citrus_utf7.c citrus_utf8.c citrus_viqr.c citrus_zw.c 

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -Wno-pointer-sign \
	-I${.CURDIR}/compat -I${.CURDIR}/include -I${.CURDIR}/citrus \
	${MODULES:S,^,-I${.CURDIR}/modules/,} \
	-include minkar_iconv.h -Dbool=_Bool
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wl,-install_name,/usr/lib/libiconv.2.dylib \
	-Wl,-compatibility_version,7.0.0 -Wl,-current_version,7.0.0 \
	-Wl,-exported_symbols_list,${.CURDIR}/libiconv.exports

MK_MANCOMPRESS = no
MAN =
MAN3 = iconv.3 iconv_canonicalize.3 iconvctl.3 iconvlist.3 __iconv_get_list.3

all: ${.OBJDIR}/libiconv.2.dylib i18n

# The conversion tables (FreeBSD's share/i18n), built on the host by
# tools/build-i18n.sh into /usr/share/i18n of the sysroot.
I18N_STAMP = ${.OBJDIR}/i18n.done
i18n: ${I18N_STAMP}
${I18N_STAMP}: ${.CURDIR}/tools/build-i18n.sh
	JOBS=${.MAKE.JOBS:U4} sh ${.CURDIR}/tools/build-i18n.sh ${.CURDIR} ${.OBJDIR}/i18n-work \
	    ${SYSROOT_DIR}
	touch ${.TARGET}

${.OBJDIR}/libiconv.2.dylib: lib${LIB}.a ${.CURDIR}/libiconv.exports
	${CC} -fuse-ld=${LD} -shared -dylib -o ${.TARGET} \
		-Wl,-force_load,${.OBJDIR}/lib${LIB}.a ${LDFLAGS}
	mkdir -p ${MINKAR_SDKROOT}/usr/include
	cp -f ${.CURDIR}/include/iconv.h ${MINKAR_SDKROOT}/usr/include/
.for d in ${MINKAR_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${d}/usr/lib ${d}/usr/share/man/man3
	cp -f ${.TARGET} ${d}/usr/lib/
	ln -sf libiconv.2.dylib ${d}/usr/lib/libiconv.dylib
	cd ${.CURDIR}/citrus && cp -f ${MAN3} ${d}/usr/share/man/man3/
.for l in iconv_open iconv_open_into iconv_close __iconv
	ln -f ${d}/usr/share/man/man3/iconv.3 ${d}/usr/share/man/man3/${l}.3
.endfor
	ln -f ${d}/usr/share/man/man3/__iconv_get_list.3 \
	    ${d}/usr/share/man/man3/__iconv_free_list.3
.endfor

.include <bsd.lib.mk>
