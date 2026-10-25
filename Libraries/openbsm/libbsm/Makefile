# libbsm (OpenBSM, BSD), as macOS has it in /usr/lib.

SUBDIR =
MK_WERROR = no
LIB = bsm
OPENBSM = ${.CURDIR:H}

SRCS = bsm_audit.c bsm_class.c bsm_control.c bsm_domain.c bsm_errno.c \
	bsm_event.c bsm_fcntl.c bsm_flags.c bsm_io.c bsm_mask.c bsm_notify.c \
	bsm_socket_type.c bsm_token.c bsm_user.c bsm_wrappers.c

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.OBJDIR}/include -I${OPENBSM} -I${OPENBSM}/libbsm -I${OPENBSM}/sys \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_notify \
	-Wno-cast-align -Wno-cast-qual -Wno-sign-compare -Wno-shadow

LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wl,-install_name,/usr/lib/libbsm.0.dylib \
	-Wl,-current_version,1.0 -Wl,-compatibility_version,1.0 -lSystem

INCS = ${OPENBSM}/bsm/libbsm.h ${OPENBSM}/bsm/audit_uevents.h

all: ${.OBJDIR}/libbsm.0.dylib

# The sources include <config/config.h>: ours, not the FreeBSD one in config/.
${.OBJDIR}/include/config/config.h: ${OPENBSM}/config/config_ravynos.h
	mkdir -p ${.TARGET:H}
	cp -f ${.ALLSRC} ${.TARGET}

${.OBJDIR}/libbsm.0.dylib: lib${LIB}.a
	${CC} -fuse-ld=${LD} -shared -dylib -o ${.TARGET} \
		-Wl,-force_load,${.OBJDIR}/lib${LIB}.a ${LDFLAGS}
.for d in ${MINKAR_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${d}/usr/lib ${d}/usr/include/bsm
	cp -f ${.TARGET} ${d}/usr/lib/
	ln -sf libbsm.0.dylib ${d}/usr/lib/libbsm.dylib
	cp -f ${INCS} ${d}/usr/include/bsm/
.endfor

.include <bsd.lib.mk>

${OBJS} ${POBJS} ${SOBJS}: ${.OBJDIR}/include/config/config.h
