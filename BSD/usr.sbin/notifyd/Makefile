# notifyd, Apple's notification server (libnotify-241, APSL, in the tree):
# serves notify(3) (libsystem_notify) on the Mach service
# com.apple.system.notification_center. __MINKAR__ marks the changes:
# no sandbox checks (Minkar has no sandbox), and a name's public service
# closed by its info (service_close_info) instead of as an index.

SUBDIR =
MK_WERROR = no
PROG =	notifyd
MAN =	notifyd.8
BINDIR = /usr/sbin

LIBNOTIFY = ${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_notify
SLF =	/System/Library/Frameworks

.PATH: ${.OBJDIR}
SRCS =	notifyd.c notify_proc.c pathwatch.c service.c timer.c \
	notify_ipcServer.c notifyServer.c

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fblocks -DPRIVATE -DTARGET_OS_BRIDGE=0 -Wno-nullability-completeness \
	-I${.OBJDIR} -I${.CURDIR} -I${LIBNOTIFY} \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libdispatch/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/liblaunch \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_c \
	-I${MINKAR_SDKROOT}/usr/local/include \
	-I${MINKAR_SDKROOT}${SLF}/System.framework/PrivateHeaders \
	-idirafter ${MINKAR_SDKROOT}${SLF}/Kernel.framework/Versions/A/Headers
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD =	-lbsm

MIGFLAGS = -novouchers --sysroot=${MINKAR_SDKROOT} \
	-mmacos-version-min=${MACOS_VERSION_MIN} -DPRIVATE

notify_ipcServer.c notify_ipcServer.h notify_ipc.h: ${LIBNOTIFY}/notify_ipc.defs
	${MIG} ${MIGFLAGS} -user /dev/null -header notify_ipc.h \
		-server notify_ipcServer.c -sheader notify_ipcServer.h \
		${LIBNOTIFY}/notify_ipc.defs

# Port death notifications.
notifyServer.c notifyServer.h: ${MINKAR_SDKROOT}/usr/include/mach/notify.defs
	${MIG} ${MIGFLAGS} -user /dev/null -header /dev/null \
		-server notifyServer.c -sheader notifyServer.h \
		${MINKAR_SDKROOT}/usr/include/mach/notify.defs

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${BINDIR} ${SYSROOT_DIR}/etc
	install -m 0555 ${PROG} ${SYSROOT_DIR}${BINDIR}/
	install -m 0644 ${.CURDIR}/notify.conf.MacOSX ${SYSROOT_DIR}/etc/notify.conf

.include <bsd.prog.mk>

${OBJS}: notify_ipc.h notify_ipcServer.h notifyServer.h
