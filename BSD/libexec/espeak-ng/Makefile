# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# eSpeak NG 1.52.0 (GPL-3.0-or-later; the maintainer's choice, 2026-11-23),
# the speech synthesizer say(1) and AppKit's NSSpeechSynthesizer run as a
# process of its own (nothing of Minkar's links it): fetched at the version
# MANIFEST pins, built unchanged with our own rules (its CMake build runs
# the program it builds, which a cross build cannot), without MBROLA,
# Klatt, speech-player, sonic, pcaudio or its threads. Its voices' data is
# compiled by a build of the same sources for the build host, as upstream's
# data.cmake does, for every language it has. Installs
# /usr/libexec/espeak-ng, /usr/share/espeak-ng-data and its licenses in
# /usr/share/licenses/espeak-ng.

SUBDIR =
FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
HOST_CC ?=	cc
SRC =		${.OBJDIR}/espeak-ng-src
DATA =		${.OBJDIR}/data

LIB_SOURCES =	common mnemonics error ieee80 compiledata compiledict dictionary \
		encoding intonation langopts numbers phoneme phonemelist readclause \
		setlengths soundicon spect ssml synthdata synthesize tr_languages \
		translate translateword voices wavegen speech espeak_api
UCD_SOURCES =	case categories ctype proplist scripts tostring
SOURCES =	${LIB_SOURCES:@f@${SRC}/src/libespeak-ng/${f}.c@} \
		${UCD_SOURCES:@f@${SRC}/src/ucd-tools/src/${f}.c@} ${SRC}/src/espeak-ng.c
INCLUDES =	-I${.OBJDIR}/config -I${SRC}/src/include -I${SRC}/src/include/compat \
		-I${SRC}/src/ucd-tools/src/include -I${SRC}/src/libespeak-ng
DEFINES =	-DLIBESPEAK_NG_EXPORT=1

# The languages whose dictionaries are compiled: upstream's list.
LANGUAGES =	af am an ar as az ba be bg bn bpy bs ca chr cmn cs cv cy da de el en eo es \
		et eu fa fi fr ga gd gn grc gu hak haw he hi hr ht hu hy ia id io is it ja \
		jbo ka kk kl kn kok ko ku ky la lb lfn lt lv mi mk ml mr ms mto mt my nci ne \
		nl nog no om or pap pa piqd pl pt py qdb quc qu qya ro ru sd shn si sjn sk sl \
		smj sq sr sv sw ta te ti th tk tn tr tt ug uk ur uz vi yue

all: to-sysroot

${SRC}/.ready: ${.CURDIR}/MANIFEST
	src=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	rm -rf ${SRC} && cp -a $$src ${SRC}
	mkdir -p ${.OBJDIR}/config
	printf '%s\n' '#define HAVE_MKSTEMP 1' '#define USE_ASYNC 0' '#define USE_KLATT 0' \
	    '#define USE_LIBPCAUDIO 0' '#define USE_LIBSONIC 0' '#define USE_MBROLA 0' \
	    '#define USE_SPEECHPLAYER 0' '#define PACKAGE_VERSION "1.52.0"' > ${.OBJDIR}/config/config.h
	touch ${.TARGET}

# For Minkar.
${.OBJDIR}/espeak-ng: ${SRC}/.ready
	${CC} --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -fuse-ld=${LD} -O2 -w ${INCLUDES} ${DEFINES} \
	    '-DPATH_ESPEAK_DATA="/usr/share/espeak-ng-data"' -o ${.TARGET} ${SOURCES}

# For the build host: it compiles the voices' data.
${.OBJDIR}/host/espeak-ng: ${SRC}/.ready
	mkdir -p ${.OBJDIR}/host
	env -u MACOSX_DEPLOYMENT_TARGET ${HOST_CC} -O2 -w ${INCLUDES} ${DEFINES} \
	    '-DPATH_ESPEAK_DATA="/nonexistent"' -o ${.TARGET} ${SOURCES} -lm

${DATA}/.done: ${.OBJDIR}/host/espeak-ng
	rm -rf ${DATA} && mkdir -p ${DATA}/espeak-ng-data/voices ${DATA}/dictsource
	cp -a ${SRC}/espeak-ng-data/lang ${DATA}/espeak-ng-data/
	cp -a "${SRC}/espeak-ng-data/voices/!v" ${DATA}/espeak-ng-data/voices/
	cp -a ${SRC}/phsource ${DATA}/
	cd ${DATA}/phsource && ESPEAK_DATA_PATH=${DATA} ${.OBJDIR}/host/espeak-ng --compile-intonations > /dev/null
	cd ${DATA}/phsource && ESPEAK_DATA_PATH=${DATA} ${.OBJDIR}/host/espeak-ng --compile-phonemes > /dev/null
	cd ${DATA}/dictsource && for l in ${LANGUAGES}; do \
		cp ${SRC}/dictsource/$${l}_rules ${SRC}/dictsource/$${l}_list . || exit 1; \
		if [ -f ${SRC}/dictsource/extra/$${l}_listx ]; then cp ${SRC}/dictsource/extra/$${l}_listx .; \
		elif [ -f ${SRC}/dictsource/$${l}_listx ]; then cp ${SRC}/dictsource/$${l}_listx .; fi; \
		if [ -f ${SRC}/dictsource/$${l}_emoji ]; then cp ${SRC}/dictsource/$${l}_emoji .; fi; \
		ESPEAK_DATA_PATH=${DATA} ${.OBJDIR}/host/espeak-ng --compile=$$l > /dev/null || exit 1; \
	done
	touch ${.TARGET}

to-sysroot: ${.OBJDIR}/espeak-ng ${DATA}/.done
	mkdir -p ${SYSROOT_DIR}/usr/libexec ${SYSROOT_DIR}/usr/share/licenses/espeak-ng
	install -C -m 0555 -o 0 -g 0 ${.OBJDIR}/espeak-ng ${SYSROOT_DIR}/usr/libexec/espeak-ng
	rsync -rc --delete ${DATA}/espeak-ng-data/ ${SYSROOT_DIR}/usr/share/espeak-ng-data/
	install -C -m 0444 ${SRC}/COPYING ${SRC}/COPYING.APACHE ${SRC}/COPYING.BSD2 ${SRC}/COPYING.UCD \
	    ${SYSROOT_DIR}/usr/share/licenses/espeak-ng/

.include <bsd.obj.mk>
