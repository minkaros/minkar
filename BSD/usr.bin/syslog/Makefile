# syslog(1), Apple's ASL command (syslog project, APSL, in the tree;
# approved 2026-10-24): sends messages, searches and follows (-w) the
# ASL store through syslogd, and sets filters.

SUBDIR =
MK_WERROR = no
PROG =	syslog
MAN =
BINDIR = /usr/bin

ASLCOMMON = ${ROOT_SOURCE_DIR}/BSD/syslog/aslcommon
LIBASL =    ${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_asl
SLF =	/System/Library/Frameworks

.PATH: ${.OBJDIR} ${ASLCOMMON}
SRCS =	syslog.c asl_ipcUser.c asl_common.c

CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fblocks -DPRIVATE -DTARGET_OS_BRIDGE=0 -Wno-nullability-completeness \
	-Wno-deprecated-declarations \
	-I${.OBJDIR} -I${.CURDIR} -I${ASLCOMMON} -I${LIBASL} \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libdispatch/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/liblaunch \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_c \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_notify \
	-I${RAVYN_SDKROOT}/usr/local/include \
	-I${RAVYN_SDKROOT}${SLF}/System.framework/PrivateHeaders \
	-idirafter ${RAVYN_SDKROOT}${SLF}/Kernel.framework/Versions/A/Headers
LDFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD =	-lbsm

MIGFLAGS = -novouchers --sysroot=${RAVYN_SDKROOT} \
	-mmacos-version-min=${MACOS_VERSION_MIN} -DPRIVATE

asl_ipcUser.c asl_ipc.h: ${ASLCOMMON}/asl_ipc.defs
	${MIG} ${MIGFLAGS} -user asl_ipcUser.c -header asl_ipc.h \
		-server /dev/null -sheader /dev/null ${ASLCOMMON}/asl_ipc.defs

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${BINDIR}
	install -m 0555 ${PROG} ${SYSROOT_DIR}${BINDIR}/

.include <bsd.prog.mk>

${OBJS}: asl_ipc.h
