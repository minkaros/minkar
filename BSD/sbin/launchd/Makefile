# launchd: PID 1 and the per-user service managers (architecture plan,
# Phase 2). The MIG protocol (job.defs and friends) must stay identical to
# the client side in Libraries/Libsystem/liblaunch.

SUBDIR =
MK_WERROR = no
MK_PIE = no

PROG =	launchd
MAN =	launchd.conf.5 launchd.plist.5 launchd.8

.PATH: ${.OBJDIR}

LIBLAUNCH =	${ROOT_SOURCE_DIR}/Libraries/Libsystem/liblaunch
LIBSYSTEM =	${ROOT_SOURCE_DIR}/Libraries/Libsystem
XNU =		${ROOT_SOURCE_DIR}/Kernel/xnu

COMMONFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-DPRIVATE -fblocks \
	-I${.OBJDIR} -I${.CURDIR} -I${LIBLAUNCH} \
	-I${LIBSYSTEM}/private \
	-I${RAVYN_SDKROOT}/System/Library/Frameworks/System.framework/PrivateHeaders \
	-I${RAVYN_SDKROOT}/usr/local/include

MIG_FLAGS =	${COMMONFLAGS} -D__MigTypeCheck
CFLAGS =	${COMMONFLAGS} -Wno-nullability-completeness
LDFLAGS =	--sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

# Generated MIG interfaces, the sides Apple's launchd target builds.
# Server: what launchd answers (job, internal, exceptions, dead-name
# notifications). User: the replies, forwards and internal calls it sends.
MIG_SRCS =	jobServer.c job_replyUser.c job_forwardUser.c \
		internalServer.c internalUser.c notifyServer.c mach_excServer.c
MIG_HDRS =	job.h jobServer.h job_reply.h job_forward.h \
		internal.h internalServer.h notify.h notifyServer.h mach_excServer.h \
		helper.h helperServer.h

SRCS =	${MIG_SRCS} launchd.c core.c kill2.c ktrace.c ipc.c log.c runtime.c

job.h jobServer.h jobServer.c: ${.CURDIR}/job.defs
	${MIG} ${MIG_FLAGS} -user /dev/null -header job.h -sheader jobServer.h \
	    -server jobServer.c ${.CURDIR}/job.defs
job_reply.h job_replyUser.c: ${.CURDIR}/job_reply.defs
	${MIG} ${MIG_FLAGS} -header job_reply.h -server /dev/null \
	    -user job_replyUser.c ${.CURDIR}/job_reply.defs
job_forward.h job_forwardUser.c: ${.CURDIR}/job_forward.defs
	${MIG} ${MIG_FLAGS} -header job_forward.h -server /dev/null \
	    -user job_forwardUser.c ${.CURDIR}/job_forward.defs
internal.h internalServer.h internalUser.c internalServer.c: ${.CURDIR}/internal.defs
	${MIG} ${MIG_FLAGS} -header internal.h -sheader internalServer.h \
	    -user internalUser.c -server internalServer.c ${.CURDIR}/internal.defs
notify.h notifyServer.h notifyServer.c: ${.CURDIR}/notify.defs
	${MIG} ${MIG_FLAGS} -header notify.h -sheader notifyServer.h \
	    -user /dev/null -server notifyServer.c ${.CURDIR}/notify.defs
mach_excServer.h mach_excServer.c: ${.CURDIR}/mach_exc.defs
	${MIG} ${MIG_FLAGS} -header /dev/null -sheader mach_excServer.h \
	    -user /dev/null -server mach_excServer.c ${.CURDIR}/mach_exc.defs
helper.h helperServer.h: ${.CURDIR}/helper.defs
	${MIG} ${MIG_FLAGS} -header helper.h -sheader helperServer.h \
	    -user /dev/null -server /dev/null ${.CURDIR}/helper.defs

# Every C file may include any generated header.
${SRCS:M*.c:N*Server.c:N*User.c:S/.c$/.o/}: ${MIG_HDRS}

CLEANFILES +=	${MIG_SRCS} ${MIG_HDRS}

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin
	cp -f ${PROG} ${SYSROOT_DIR}/sbin/

.include <bsd.prog.mk>
