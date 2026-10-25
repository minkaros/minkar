# configd for Minkar, from Apple's configd-1061.141.1 (APSL), with the
# plugins built in, as Apple does: InterfaceNamer (interface names),
# KernelEventMonitor (interface state from the kernel), PreferencesMonitor
# (applies /Library/Preferences/SystemConfiguration), LinkConfiguration
# (MTU and media) and IPMonitor (primary service, routes, DNS: dnsinfo,
# network information: nwi, proxies). Each has an Info.plist bundle in
# /System/Library/SystemConfiguration. Not built: QoSMarking, the
# IPMonitor configuration agents, NAT64 and SMB (Plugins/IPMonitor/
# ravynos_stubs.c), which need NetworkExtension, Network.framework or SMB.
# __MINKAR__ marks the changes made for ravynOS. (Apple's buildit
# makefile was here.)

SUBDIR =
MK_WERROR = no
# configd uses no Foundation (its .m files only need libobjc); with
# Foundation loaded, CoreFoundation would bridge its objects to it.
MK_OBJC_RUNTIME = no
PROG =	configd
MAN =	configd.8
BINDIR = /usr/libexec

CONFIGD = ${.CURDIR}
SC =	${ROOT_SOURCE_DIR}/Frameworks/SystemConfiguration
SLF =	/System/Library/Frameworks

.PATH: ${CONFIGD}/configd.tproj ${CONFIGD}/Plugins/InterfaceNamer \
	${CONFIGD}/Plugins/IPMonitor ${CONFIGD}/Plugins/KernelEventMonitor \
	${CONFIGD}/Plugins/PreferencesMonitor ${CONFIGD}/Plugins/LinkConfiguration \
	${CONFIGD}/Plugins/common ${CONFIGD}/dnsinfo ${CONFIGD}/nwi \
	${CONFIGD}/libSystemConfiguration ${CONFIGD}/IPMonitorControl ${SC}/compat \
	${.OBJDIR}

SRCS =	configd.m configd_server.c plugin_support.c session.c pattern.c \
	_SCD.c _configadd.c _configclose.c _configget.c _configlist.c \
	_confignotify.c _configopen.c _configremove.c _configset.c \
	_configunlock.c _notifyadd.c _notifycancel.c _notifychanges.c \
	_notifyremove.c _notifyviafd.c _notifyviaport.c _snapshot.c \
	configServer.c
SRCS +=	ifnamer.c
SRCS +=	ip_plugin.c dns-configuration.c proxy-configuration.c set-hostname.c \
	ravynos_stubs.c IPMonitorControlServer.c
SRCS +=	eventmon.c ev_dlil.c ev_ipv4.c ev_ipv6.c ev_extra.m
SRCS +=	prefsmon.c linkconfig.c
SRCS +=	InterfaceNamerControlPrefs.c IPMonitorControlPrefs.c
SRCS +=	dnsinfo_create.c dnsinfo_server.c dnsinfo_flatfile.c \
	network_information_server.c \
	libSystemConfiguration_server.c
SRCS +=	sc_sha256.c	# SystemConfiguration's (hidden there), for dnsinfo and IPMonitor

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fblocks -DPRIVATE -DTARGET_OS_BRIDGE=0 -Wno-nullability-completeness \
	-Wno-elaborated-enum-base -Wno-deprecated-declarations \
	-I${.OBJDIR} -I${CONFIGD}/configd.tproj -I${CONFIGD}/Plugins/IPMonitor \
	-I${CONFIGD}/Plugins/KernelEventMonitor -I${CONFIGD}/Plugins/common \
	-I${CONFIGD}/dnsinfo -I${CONFIGD}/nwi -I${CONFIGD}/libSystemConfiguration \
	-I${CONFIGD}/IPMonitorControl -I${CONFIGD}/config-agent-info \
	-I${MINKAR_SDKROOT}${SLF}/SystemConfiguration.framework/PrivateHeaders \
	-I${SC} -I${SC}/compat -I${SC}/helper \
	-I${MINKAR_SDKROOT}/usr/local/include \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libdispatch/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/liblaunch \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_c \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_notify \
	-I${ROOT_SOURCE_DIR}/BSD/usr.sbin/mDNSResponder/mDNSShared \
	-I${MINKAR_SDKROOT}${SLF}/System.framework/PrivateHeaders \
	-idirafter ${MINKAR_SDKROOT}${SLF}/Kernel.framework/Versions/A/Headers \
	-idirafter ${ROOT_SOURCE_DIR}/Kernel/Extensions/IOStorageFamily

LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD =	-framework SystemConfiguration -framework CoreFoundation \
	-framework IOKit -lbsm -lobjc

MIGFLAGS = -novouchers --sysroot=${MINKAR_SDKROOT} \
	-mmacos-version-min=${MACOS_VERSION_MIN} -DPRIVATE -I${SC}

configServer.c configServer.h config.h: ${SC}/config.defs
	${MIG} ${MIGFLAGS} -user /dev/null -header config.h \
		-server configServer.c -sheader configServer.h ${SC}/config.defs

PLUGINS = IPMonitor InterfaceNamer KernelEventMonitor LinkConfiguration \
	PreferencesMonitor
PLUGINDIR = ${SYSROOT_DIR}/System/Library/SystemConfiguration

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}${BINDIR} ${SYSROOT_DIR}/Library/Preferences/SystemConfiguration
	install -m 0555 ${PROG} ${SYSROOT_DIR}${BINDIR}/
	# The default names (Minkar / minkar), as scutil --set would write them.
	install -m 0644 ${.CURDIR}/preferences.plist \
	    ${SYSROOT_DIR}/Library/Preferences/SystemConfiguration/
.for p in ${PLUGINS}
	mkdir -p ${PLUGINDIR}/${p}.bundle/Contents
	sed -e 's/$$(PRODUCT_BUNDLE_IDENTIFIER)/com.apple.SystemConfiguration.${p}/' \
		${CONFIGD}/Plugins/${p}/Info.plist > ${PLUGINDIR}/${p}.bundle/Contents/Info.plist
.endfor

.include <bsd.prog.mk>

# The MIG headers come before any object.
${OBJS}: config.h configServer.h

# bsd.prog.mk has no Objective-C rule for position-independent objects.
configd.pieo: ${CONFIGD}/configd.tproj/configd.m config.h configServer.h
	${CC} ${CFLAGS} -fPIE -fobjc-arc -c ${CONFIGD}/configd.tproj/configd.m -o ${.TARGET}

ev_extra.pieo: ${CONFIGD}/Plugins/KernelEventMonitor/ev_extra.m config.h
	${CC} ${CFLAGS} -fPIE -fobjc-arc -c ${CONFIGD}/Plugins/KernelEventMonitor/ev_extra.m -o ${.TARGET}
