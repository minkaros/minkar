# libmagic and the magic database of file(1) 5.46 (Christos Zoulas et al.,
# BSD-2; COPYING), from FreeBSD's contrib/file in git history: src/ (less
# the compat files Darwin doesn't need; fmtcheck.c and strlcpy/strlcat.c
# stay for the host tool) and magic/. config.h is FreeBSD's, adjusted for
# Darwin. Static library for file(1), as on macOS. The database is
# compiled by mkmagic, built for the build host (host/config.h), and
# installed as /usr/share/file/magic.mgc.

SUBDIR =
MK_WERROR = no
.PATH: ${.CURDIR}/src
LIB = magic
SRCS = apprentice.c ascmagic.c buffer.c cdf.c cdf_time.c compress.c der.c \
	encoding.c fsmagic.c funcs.c is_csv.c is_json.c is_simh.c is_tar.c \
	magic.c print.c readcdf.c readelf.c seccomp.c softmagic.c
MAGICPATH = /usr/share/file
CLEANFILES = magic.h magic magic.mgc mkmagic

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-DHAVE_CONFIG_H -DMAGIC='"${MAGICPATH}/magic"' \
	-I. -I${.CURDIR} -I${.CURDIR}/src -I${ROOT_SOURCE_DIR}/Libraries/zlib \
	-Wno-nullability-completeness

HOST_CC ?= cc
MKMAGIC_SRCS = apprentice.c cdf_time.c encoding.c funcs.c magic.c print.c \
	fmtcheck.c strlcpy.c strlcat.c

all: lib${LIB}.a magic.mgc
	mkdir -p ${SYSROOT_DIR}${MAGICPATH}
	install -m 0444 magic.mgc ${SYSROOT_DIR}${MAGICPATH}/

${SRCS:R:S/$/.o/}: magic.h

magic.h: ${.CURDIR}/src/magic.h.in
	sed -e 's,X.YY,546,g' ${.ALLSRC} > ${.TARGET}

mkmagic: magic.h ${MKMAGIC_SRCS:S,^,${.CURDIR}/src/,}
	${HOST_CC} -O2 -DHAVE_CONFIG_H -DCOMPILE_ONLY -I. -I${.CURDIR}/host \
	    -I${.CURDIR}/src -o ${.TARGET} \
	    ${MKMAGIC_SRCS:S,^,${.CURDIR}/src/,}

magic: ${.CURDIR}/magic/Header ${.CURDIR}/magic/Localstuff
	cat ${.CURDIR}/magic/Header ${.CURDIR}/magic/Localstuff \
	    `ls ${.CURDIR}/magic/Magdir/[a-z]* | LC_ALL=C sort` > ${.TARGET}

magic.mgc: mkmagic magic
	./mkmagic magic

.include <bsd.lib.mk>
