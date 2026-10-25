# libxo (Juniper Networks, BSD-2-Clause; LICENSE): the structured output
# (text, XML, JSON, HTML) of FreeBSD's commands: df, du, wc, netstat, ...
# Sources from FreeBSD's contrib/libxo 1.6.0 (git history); xo_config.h is
# FreeBSD's, adjusted for Darwin (marked Minkar). Installed as
# /usr/lib/libxo.0.dylib, headers in <libxo/xo.h>. No encoders yet.

SUBDIR =
MK_WERROR = no
LIB = xo

SRCS = libxo.c xo_encoder.c xo_syslog.c

CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR} -I${ROOT_SOURCE_DIR}/BSD/lib/libutil \
	-DXO_ENCODERDIR=\"/usr/lib/libxo/encoder\" \
	-Wno-nullability-completeness

LDFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wl,-install_name,/usr/lib/libxo.0.dylib \
	-Wl,-current_version,1.6 -Wl,-compatibility_version,1.0 \
	-L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libutil -lutil -lSystem

INCS = xo.h xo_encoder.h

all: ${.OBJDIR}/libxo.0.dylib

${.OBJDIR}/libxo.0.dylib: lib${LIB}.a
	${CC} -fuse-ld=${LD} -shared -dylib -o ${.TARGET} \
		-Wl,-force_load,${.OBJDIR}/lib${LIB}.a ${LDFLAGS}
.for d in ${RAVYN_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${d}/usr/lib ${d}/usr/include/libxo
	cp -f ${.TARGET} ${d}/usr/lib/
	ln -sf libxo.0.dylib ${d}/usr/lib/libxo.dylib
.for h in ${INCS}
	cp -f ${.CURDIR}/${h} ${d}/usr/include/libxo/
.endfor
.endfor

.include <bsd.lib.mk>
