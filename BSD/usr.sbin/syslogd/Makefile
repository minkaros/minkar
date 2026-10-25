# syslogd, Apple's system log server (syslog project, APSL, in the tree;
# approved for Minkar 2026-10-24): the ASL server on the Mach service
# com.apple.system.logger (syslog(3), asl(3)), the BSD socket
# /var/run/syslog, the kernel log, /var/log/system.log and the ASL store
# in /var/log/asl. __RAVYNOS__ marks the changes.

SUBDIR =
MK_WERROR = no
PROG =	syslogd
MAN =
BINDIR = /usr/sbin

ASLCOMMON = ${ROOT_SOURCE_DIR}/BSD/syslog/aslcommon
LIBASL =    ${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_asl
SLF =	/System/Library/Frameworks

.PATH: ${.OBJDIR} ${ASLCOMMON}
SRCS =	syslogd.c daemon.c dbserver.c asl_action.c bsd_in.c bsd_out.c \
	klog_in.c remote.c udp_in.c asl_common.c asl_memory.c asl_ipcServer.c

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

asl_ipcServer.c asl_ipcServer.h asl_ipc.h: ${ASLCOMMON}/asl_ipc.defs
	${MIG} ${MIGFLAGS} -user /dev/null -header asl_ipc.h \
		-server asl_ipcServer.c -sheader asl_ipcServer.h \
		${ASLCOMMON}/asl_ipc.defs

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${BINDIR} ${SYSROOT_DIR}/etc
	install -m 0555 ${PROG} ${SYSROOT_DIR}${BINDIR}/
	install -m 0644 ${.CURDIR}/asl.conf.osx ${SYSROOT_DIR}/etc/asl.conf
	install -m 0644 ${.CURDIR}/syslog.conf ${SYSROOT_DIR}/etc/syslog.conf

.include <bsd.prog.mk>

${OBJS}: asl_ipc.h asl_ipcServer.h
