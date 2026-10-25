# scutil, configd's command line tool (configd-1061.141.1, APSL): the
# dynamic store, the preferences, DNS, proxy and reachability reports,
# the host names. --nc is not supported (nc_minkar.c; nc.c needs
# NetworkExtension).

SUBDIR =
MK_WERROR = no
MK_OBJC_RUNTIME = no
PROG =	scutil
MAN =	scutil.8
BINDIR = /usr/sbin

CONFIGD = ${.CURDIR:H}
SC =	${ROOT_SOURCE_DIR}/Frameworks/SystemConfiguration
SLF =	/System/Library/Frameworks

SRCS =	scutil.c cache.c commands.c dictionary.c net.c net_interface.c \
	net_protocol.c net_service.c net_set.c notifications.c prefs.c \
	session.c tests.c nc_minkar.c
# The shared preferences of the plugins, and SystemConfiguration's SHA-256.
SRCS +=	InterfaceNamerControlPrefs.c IPMonitorControlPrefs.c sc_sha256.c
.PATH: ${CONFIGD}/Plugins/common ${SC}/compat

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fblocks -DPRIVATE -DTARGET_OS_BRIDGE=0 -Wno-nullability-completeness \
	-Wno-elaborated-enum-base -Wno-deprecated-declarations \
	-I${.CURDIR} -I${CONFIGD}/Plugins/IPMonitor -I${CONFIGD}/dnsinfo \
	-I${CONFIGD}/nwi -I${CONFIGD}/libSystemConfiguration \
	-I${CONFIGD}/IPMonitorControl -I${CONFIGD}/config-agent-info \
	-I${CONFIGD}/Plugins/common \
	-I${MINKAR_SDKROOT}${SLF}/SystemConfiguration.framework/PrivateHeaders \
	-I${SC} -I${SC}/compat -I${SC}/helper \
	-I${MINKAR_SDKROOT}/usr/local/include \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libdispatch/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/liblaunch \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_c \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_notify \
	-I${MINKAR_SDKROOT}${SLF}/System.framework/PrivateHeaders \
	-idirafter ${MINKAR_SDKROOT}${SLF}/Kernel.framework/Versions/A/Headers
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD =	-framework SystemConfiguration -framework CoreFoundation -lbsm -ledit

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${BINDIR}
	install -m 0555 ${PROG} ${SYSROOT_DIR}${BINDIR}/

.include <bsd.prog.mk>
