# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
#
# libauditd (OpenBSM, BSD): auditd's library, as macOS has it in /usr/lib.
# launchd uses it to stop auditing at shutdown (audit_quick_stop).

SUBDIR =
MK_WERROR = no
LIB = auditd
OPENBSM = ${.CURDIR:H}

SRCS = auditd_lib.c

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.OBJDIR}/include -I${OPENBSM} -I${OPENBSM}/sys \
	-Wno-cast-align -Wno-cast-qual -Wno-sign-compare -Wno-shadow

LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wl,-install_name,/usr/lib/libauditd.0.dylib \
	-Wl,-current_version,1.0 -Wl,-compatibility_version,1.0 -lbsm -lSystem

all: ${.OBJDIR}/libauditd.0.dylib

${.OBJDIR}/include/config/config.h: ${OPENBSM}/config/config_minkar.h
	mkdir -p ${.TARGET:H}
	cp -f ${.ALLSRC} ${.TARGET}

${.OBJDIR}/libauditd.0.dylib: lib${LIB}.a
	${CC} -fuse-ld=${LD} -shared -dylib -o ${.TARGET} \
		-Wl,-force_load,${.OBJDIR}/lib${LIB}.a ${LDFLAGS}
.for d in ${MINKAR_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${d}/usr/lib ${d}/usr/include/bsm
	cp -f ${.TARGET} ${d}/usr/lib/
	ln -sf libauditd.0.dylib ${d}/usr/lib/libauditd.dylib
	cp -f ${OPENBSM}/bsm/auditd_lib.h ${d}/usr/include/bsm/
.endfor

.include <bsd.lib.mk>

${OBJS} ${POBJS} ${SOBJS}: ${.OBJDIR}/include/config/config.h
