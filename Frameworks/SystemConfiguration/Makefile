# SystemConfiguration.framework, from Apple's configd sources (APSL).
#
# Left out: SCNetworkConnection (PPP and VPN, needs PPPController),
# Apple's SCNetworkReachability (built on Network.framework's private
# API; SCNetworkReachabilityMinkar.c is ours), VPN* and SNHelper
# (NetworkExtension). compat/ has Minkar stand-ins: Security types,
# the serial registry keys and SHA-256. SCSchemaDefinitions.[ch] are the
# files genSCPreferences generated (Apple's makefile for that was here).
# __MINKAR__ marks the changes made for Minkar.

SUBDIR =
MK_WERROR = no
FRAMEWORK = SystemConfiguration
FMWK_VERSION = A
INSTALL_NAME_DIR = /System/Library/Frameworks/SystemConfiguration.framework/Versions/A

CONFIGD = ${ROOT_SOURCE_DIR}/BSD/usr.sbin/configd
SLF = /System/Library/Frameworks

.PATH: ${.CURDIR}/helper ${.CURDIR}/compat ${CONFIGD}/IPMonitorControl ${.OBJDIR}
SRCS = \
	BondConfiguration.c BridgeConfiguration.c CaptiveNetwork.c DHCP.c \
	LinkConfiguration.c SCD.c SCDAdd.c SCDCache.c SCDConsoleUser.c \
	SCDGet.c SCDHostName.c SCDKeys.c SCDList.c SCDNotifierAdd.c \
	SCDNotifierCancel.c SCDNotifierGetChanges.c \
	SCDNotifierInformViaCallback.c SCDNotifierInformViaFD.c \
	SCDNotifierRemove.c SCDNotifierSetKeys.c SCDNotifierWait.c \
	SCDNotify.c SCDOpen.c SCDPlugin.c SCDPrivate.c SCDRemove.c SCDSet.c \
	SCDSnapshot.c SCLocation.c SCNetwork.c \
	SCNetworkConfigurationInternal.c SCNetworkConfigurationPrivate.c \
	SCNetworkInterface.c SCNetworkInterfaceProvider.c \
	SCNetworkMigration.c SCNetworkProtocol.c SCNetworkService.c \
	SCNetworkSet.c SCNetworkSignature.c SCP.c SCPAdd.c SCPApply.c \
	SCPCommit.c SCPGet.c SCPList.c SCPLock.c SCPOpen.c SCPPath.c \
	SCPRemove.c SCPSet.c SCPUnlock.c SCPreferencesKeychainPrivate.c \
	SCPreferencesPathKey.c SCProxies.c SCSchemaDefinitions.c \
	VLANConfiguration.c dy_framework.c scprefs_observer.c \
	SCHelper_client.c configUser.c helperUser.c \
	SCNetworkReachabilityMinkar.c IPMonitorControl.c sc_sha256.c

HEADERS = SystemConfiguration.h SCDynamicStore.h SCDynamicStoreKey.h \
	SCDynamicStoreCopySpecific.h SCDynamicStoreCopyDHCPInfo.h \
	SCPreferences.h SCPreferencesPath.h SCPreferencesSetSpecific.h \
	SCNetwork.h SCNetworkConfiguration.h SCNetworkConnection.h \
	SCNetworkReachability.h SCSchemaDefinitions.h CaptiveNetwork.h \
	DHCPClientPreferences.h

# configd and its plugins build against these, as in Apple's tree.
PRIVATE_HEADERS = SCPrivate.h SCDPlugin.h SCValidation.h \
	SCDynamicStorePrivate.h SCDynamicStoreCopySpecificPrivate.h \
	SCDynamicStoreSetSpecificPrivate.h SCPreferencesPrivate.h \
	SCPreferencesGetSpecificPrivate.h SCPreferencesSetSpecificPrivate.h \
	SCPreferencesPathKey.h SCPreferencesKeychainPrivate.h \
	SCSchemaDefinitionsPrivate.h SCNetworkConfigurationPrivate.h \
	SCNetworkConnectionPrivate.h SCNetworkInterfaceProvider.h \
	SCNetworkSignature.h SCNetworkSignaturePrivate.h scprefs_observer.h \
	VPNConfiguration.h VPNTunnel.h VPNTunnelPrivate.h VPNFlow.h \
	VPNPrivate.h config_types.h

RESOURCES_DIRS = en.lproj
RESOURCES_FILES = NetworkConfiguration.plist

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fblocks -DPRIVATE -DTARGET_OS_BRIDGE=0 -Wno-nullability-completeness \
	-Wno-elaborated-enum-base -Wno-deprecated-declarations \
	-I${.OBJDIR}/include -I${.OBJDIR} -I${.CURDIR} -I${.CURDIR}/helper \
	-I${.CURDIR}/compat \
	-I${CONFIGD}/dnsinfo -I${CONFIGD}/nwi \
	-I${CONFIGD}/libSystemConfiguration -I${CONFIGD}/IPMonitorControl \
	-I${CONFIGD}/Plugins/common -I${CONFIGD}/Plugins/IPMonitor \
	-I${MINKAR_SDKROOT}/usr/local/include \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libdispatch/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/liblaunch \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_c \
	-I${MINKAR_SDKROOT}${SLF}/System.framework/PrivateHeaders \
	-idirafter ${MINKAR_SDKROOT}${SLF}/Kernel.framework/Versions/A/Headers \
	-idirafter ${ROOT_SOURCE_DIR}/Kernel/Extensions/IOStorageFamily

LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-framework CoreFoundation -framework IOKit -lSystem \
	-Wl,-current_version,1061.141.1 -Wl,-compatibility_version,1.0.0

MIGFLAGS = -novouchers --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -DPRIVATE -I${.CURDIR}

# <SystemConfiguration/...> includes find the sources.
${.OBJDIR}/include/SystemConfiguration:
	mkdir -p ${.OBJDIR}/include
	ln -sfn ${.CURDIR} ${.TARGET}

configUser.c config.h: ${.CURDIR}/config.defs ${.OBJDIR}/include/SystemConfiguration
	${MIG} ${MIGFLAGS} -user configUser.c -server /dev/null \
		-header config.h -sheader /dev/null ${.CURDIR}/config.defs

helperUser.c helper.h: ${.CURDIR}/helper/helper.defs ${.OBJDIR}/include/SystemConfiguration
	${MIG} ${MIGFLAGS} -I${.CURDIR}/helper -user helperUser.c -server /dev/null \
		-header helper.h -sheader /dev/null ${.CURDIR}/helper/helper.defs

fmwk-install-hook:
	sed -e 's/$$(PRODUCT_BUNDLE_IDENTIFIER)/com.apple.SystemConfiguration/' \
		${.CURDIR}/Info.plist > ${RESOURCES_DIR}/Info.plist
	mkdir -p ${MINKAR_SDKROOT}${SLF} ${SYSROOT_DIR}${SLF}
	cp -Rf ${FRAMEWORK_DIR} ${MINKAR_SDKROOT}${SLF}/
	cp -Rf ${FRAMEWORK_DIR} ${SYSROOT_DIR}${SLF}/

.include <minkar.framework.mk>

# The MIG headers come before any object.
${OBJS} ${POBJS} ${SOBJS}: config.h helper.h ${.OBJDIR}/include/SystemConfiguration
