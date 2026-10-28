# IOKit.framework (IOKitUser), from ravynOS.
# Modifications Copyright (c) 2026 The Minkar contributors
MK_WERROR = no
FRAMEWORK = IOKit
FMWK_VERSION = A
# Info.plist here is OSSystemResources' bundle, not the framework's: the
# framework gets a generated one (minkar.framework.mk).
INFO_PLIST =
INSTALL_NAME_DIR = /System/Library/Frameworks/IOKit.framework/Versions/A

.PATH: ${.OBJDIR}
SRCS = \
        stubs/energytrace.c \
        stubs/state_private.c \
        stubs/msgtracer_client.c \
	iokitmig64.c \
	powermanagement.c \
	kextmanager_mig.c \
	hid.subproj/IOHIDEventObjc.m \
	hid.subproj/IOHIDServiceObjc.m \
	hid.subproj/IOHIDSessionObjc.m \
	hid.subproj/IOHIDConnectionObjc.m \
	hid.subproj/IOHIDElementObjc.mm \
	hid.subproj/IOHIDServiceClientObjc.m \
	hid.subproj/IOHIDDeviceObjc.mm \
	hid.subproj/HIDEventBase.mm \
	hid.subproj/HIDServiceBase.m \
	hid.subproj/HIDSessionBase.m \
	hid.subproj/HIDConnectionBase.m \
	hid.subproj/HIDElementBase.mm \
	hid.subproj/HIDServiceClientBase.m \
	hid.subproj/HIDDeviceBase.mm \
	kext.subproj/cross_link.c \
	IOCFSerialize.c \
	IOCFUnserialize.tab.c \
	IOKitLib.c \
	IOCFURLAccess.c \
	hid.subproj/IOHIDAnalytics.c \
	IODataQueueClient.c \
	IOCFPlugIn.c \
	IOTrap.s \
	pwr_mgt.subproj/IOPMLib.c \
	pwr_mgt.subproj/IOPMEnergyPrefs.c \
	pwr_mgt.subproj/IOPMAutoWake.c \
	pwr_mgt.subproj/IOPMRepeatingPower.c \
	pwr_mgt.subproj/IOPMUPSPrefs.c \
	usb.subproj/IOUSBLib.c \
	network.subproj/IONetworkLib.c \
	graphics.subproj/IOAccelSurfaceControl.c \
	graphics.subproj/IOGraphicsLib.c \
	graphics.subproj/IODisplayLib.c \
	hidsystem.subproj/IOEventStatusAPI.cpp \
	kext.subproj/AdjustForNewSegmentLocation.cpp \
	hidsystem.subproj/IOHIDLib.cpp \
	kext.subproj/KextManager.c \
	ps.subproj/IOPowerSources.c \
	hid.subproj/IOHIDServiceConnectionCache.c \
	ps.subproj/IOPowerSourcesPrivate.c \
	pwr_mgt.subproj/IOPMLibPrivate.c \
	IOSystemConfiguration.c \
	hid.subproj/IOHIDValue.cpp \
	hid.subproj/IOHIDSessionFilter.c \
	hid.subproj/IOHIDElement.cpp \
	kext.subproj/fat_util.c \
	kext.subproj/macho_util.c \
	hid.subproj/IOHIDDevice.cpp \
	hid.subproj/IOHIDServiceKeys.c \
	hid.subproj/IOHIDLibPrivate.cpp \
	hid.subproj/IOHIDManager.cpp \
	hid.subproj/IOHIDQueue.cpp \
	hid.subproj/IOHIDTransaction.cpp \
	hid.subproj/IOHIDEvent.cpp \
	hid.subproj/IOHIDEventSystem.c \
	hid.subproj/IOHIDConnectionFilter.c \
	hid.subproj/IOHIDEventSystemClient.c \
	hid.subproj/IOHIDNotification.c \
	hid.subproj/IOHIDService.c \
	hid.subproj/IOHIDSession.c \
	hid.subproj/IOHIDEventQueue.c \
	pwr_mgt.subproj/IOPMPowerNotifications.c \
	kext.subproj/OSKextVersion.c \
	kext.subproj/misc_util.c \
	kext.subproj/printPList_new.c \
	IOMIGMachPort.c \
	kext.subproj/OSKext.c \
	hid.subproj/IOHIDEventSystemPrivate.c \
	kext.subproj/kextsplit.h \
	hid.subproj/IOHIDServiceFilter.c \
	hid.subproj/IOHIDServiceClient.c \
	hid.subproj/IOHIDEventSystemConnection.c \
	hid.subproj/IOHIDEventServer.c \
	pwr_mgt.subproj/IOPMAssertions.c \
	network.subproj/IOUserEthernetController.c \
	hid.subproj/IOHIDUserDevice.cpp \
	hid.subproj/IOHIDPlugins.c \
	platform.subproj/IOPlatformSupport.c

DEFS = -DAPPLE -DPRIVATE -D_ANSI_C_SOURCE -Dvolatile=__volatile \
	-DNO_SECURITY_FRAMEWORK \
	-DAVAILABLE_MAC_OS_X_VERSION_10_13_AND_LATER="" \
	-DAVAILABLE_MAC_OS_X_VERSION_10_15_AND_LATER="" \
	-DIOKIT -DTYPE_LONGLONG -DTARGET_OS_IOSMAC=0 \
	-DDISPATCH_MACH_SPI \
	-DOS_CRASH_ENABLE_EXPERIMENTAL_LIBTRACE \
	-DOS_OBJECT_USE_OBJC -DHAVE_CFPLUGIN
#HAVE_CFPLUGIN HAVE_CFURLACCESS

SLF = /System/Library/Frameworks

# Avoid setting a global standard level. We'll control it in file CFLAGS.
CSTD = none
CXXSTD = none

CFLAGS = ${DEFS} -fblocks --sysroot=${MINKAR_SDKROOT} \
	-mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness \
	-Wno-implicit-function-declaration \
	-Wno-elaborated-enum-base -Wno-int-conversion
# CF_ENUM, rdar://121475724

CFLAGS += \
	-I${.CURDIR}/../xnu/osfmk \
	-I${.CURDIR}/../xnu/libkern -I${.CURDIR}/../xnu/iokit \
	-I${.OBJDIR} \
	-I${MINKAR_SDKROOT}/usr/local/include \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libdispatch/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/liblaunch \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/osfmk \
	-I${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Kernel/xnu/EXPORT_HDRS/san \
	-I${MINKAR_SDKROOT}${SLF}/System.framework/PrivateHeaders \
	-I${MINKAR_SDKROOT}${SLF}/Kernel.framework/Headers \
	${CFLAGS.${.TARGET:T}} \
	-include${MINKAR_SDKROOT}/usr/include/i386/endian.h \
	-include${.CURDIR:H}/xnu/libkern/libkern/OSTypes.h \
	-include${MINKAR_SDKROOT}/usr/include/MacTypes.h \
	-I${ROOT_SOURCE_DIR}/Frameworks/Security/header_symlinks \
	-I${ROOT_SOURCE_DIR}/Frameworks/Security/header_symlinks/macOS \
	-I${ROOT_SOURCE_DIR}/Frameworks \
	-I${ROOT_SOURCE_DIR}/Frameworks/Foundation/Headers

ACFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}

CXXFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fcolor-diagnostics ${DEFS} ${CFLAGS.${.TARGET:T}} \
	-I${ROOT_SOURCE_DIR}/Frameworks	\
	-I${ROOT_SOURCE_DIR}/Frameworks/Foundation/Headers \
	-I${MINKAR_SDKROOT}${SLF}/System.framework/PrivateHeaders \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/private

CFLAGS.msgtracer_client.o = -I${.CURDIR}
CFLAGS.IOMIGMachPort.o = -include${MINKAR_SDKROOT}/usr/include/mach/error.h
CFLAGS.IOHIDElementObjc.o = -ObjC++ -Wno-missing-method-return-type
CFLAGS.IOHIDDeviceObjc.o = -ObjC++ -Wno-missing-method-return-type
CFLAGS.IOHIDEventObjc.o = -include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h
CFLAGS.IOHIDServiceObjc.o = -include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h
CFLAGS.IOHIDServiceClientObjc.o = -include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h
CFLAGS.IOHIDSessionObjc.o = -include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h
CFLAGS.IOHIDConnectionObjc.o = -include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h
CFLAGS.HIDEventBase.o = -include${MINKAR_SDKROOT}/usr/include/i386/endian.h \
	-include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h \
	-Ddouble_t=double -I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern
CFLAGS.HIDServiceBase.o = -include${MINKAR_SDKROOT}/usr/include/i386/endian.h \
	-include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h
CFLAGS.HIDSessionBase.o = -include${MINKAR_SDKROOT}/usr/include/i386/endian.h \
	-include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h
CFLAGS.HIDConnectionBase.o = -include${MINKAR_SDKROOT}/usr/include/i386/endian.h \
	-include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h
CFLAGS.HIDServiceClientBase.o = -include${MINKAR_SDKROOT}/usr/include/i386/endian.h \
	-include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h
CFLAGS.HIDDeviceBase.o = -include${MINKAR_SDKROOT}/usr/include/i386/endian.h \
	-include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h \
	-Ddouble_t=double -I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern
CFLAGS.IOPMLib.o = -I${.CURDIR}
CFLAGS.IOPMAutoWake.o = -I${.CURDIR} \
	-I${MINKAR_SDKROOT}${SLF}/IOKit.framework/Headers/pwr_mgt
CFLAGS.IOPMEnergyPrefs.o = -I${.CURDIR} \
	-I${MINKAR_SDKROOT}${SLF}/IOKit.framework/Headers/pwr_mgt
CFLAGS.IOPMRepeatingPower.o = -I${.CURDIR} \
	-I${MINKAR_SDKROOT}${SLF}/IOKit.framework/Headers/pwr_mgt
CFLAGS.IOPMUPSPrefs.o = -I${.CURDIR}
CFLAGS.IOGraphicsLib.o = -I${.CURDIR}
CFLAGS.IOEventStatusAPI.o = -std=gnu++11 -I${.CURDIR} -I${.CURDIR}/hid.subproj \
	-Wno-elaborated-enum-base -I${.CURDIR:H}/xnu/libkern \
	-include${MINKAR_SDKROOT}/usr/include/i386/endian.h \
	-include${MINKAR_SDKROOT}/usr/include/math.h \
	-include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/Headers/hid/IOHIDEventSystemClientPrivate.h
CFLAGS.AdjustForNewSegmentLocation.o = -Wno-elaborated-enum-base
CFLAGS.IOHIDLib.o = -Ddouble_t=double -Wno-elaborated-enum-base \
	-std=gnu++03 -I${.CURDIR:H}/xnu/libkern -I${.OBJDIR} \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/liblaunch \
	-I${.CURDIR}/hidsystem.subproj -I${.CURDIR}/hid.subproj \
	-I${.CURDIR} \
	-I${MINKAR_SDKROOT}${SLF}/IOKit.framework/Headers/pwr_mgt \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h \
	-include${ROOT_SOURCE_DIR}/Libraries/Libsystem/private/_simple.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/Headers/hid/IOHIDProperties.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/Headers/hid/IOHIDEventSystemClientPrivate.h
CFLAGS.KextManager.o = -I${.OBJDIR}
CFLAGS.IOPowerSources.o = -I${.CURDIR}
CFLAGS.IOPowerSourcesPrivate.o = -I${.CURDIR}
CFLAGS.IOPMLibPrivate.o = -I${.CURDIR}
CFLAGS.IOHIDValue.o = -Wno-elaborated-enum-base -I${.CURDIR:H}/xnu/libkern \
	-include${MINKAR_SDKROOT}/usr/include/i386/endian.h \
	-include${MINKAR_SDKROOT}/usr/include/math.h \
	-include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h
CFLAGS.IOHIDElement.o = -Wno-elaborated-enum-base -I${.CURDIR:H}/xnu/libkern \
	-include${MINKAR_SDKROOT}/usr/include/i386/endian.h \
	-include${MINKAR_SDKROOT}/usr/include/math.h \
	-include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h \
	-include${.CURDIR}/CFIOKitShim.h
CFLAGS.IOHIDEvent.o = -ObjC++ -Wno-elaborated-enum-base -I${.CURDIR:H}/xnu/libkern \
	-include${MINKAR_SDKROOT}/usr/include/i386/endian.h \
	-include${MINKAR_SDKROOT}/usr/include/math.h \
	-include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h
CFLAGS.IOHIDDevice.o = -Wno-elaborated-enum-base -I${.CURDIR:H}/xnu/libkern \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/private \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libdispatch/private \
	-include${MINKAR_SDKROOT}/usr/include/i386/endian.h \
	-include${MINKAR_SDKROOT}/usr/include/math.h \
	-include${.CURDIR}/CFIOKitShim.h
CFLAGS.IOHIDLibPrivate.o = -ObjC++ -Wno-elaborated-enum-base -I${.CURDIR:H}/xnu/libkern \
	-I${.CURDIR:H}/xnu/libkern/libkern \
	-I${.CURDIR}/hid.subproj \
	-I${.CURDIR:H}/Extensions/IOHIDFamily/IOHIDFamily \
	-include${MINKAR_SDKROOT}/usr/include/math.h \
	-include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h \
	-include${.CURDIR:H}/Extensions/IOHIDFamily/IOHIDFamily/IOHIDEventTypes.h \
	-include${.CURDIR:H}/Extensions/IOHIDFamily/IOHIDFamily/AppleHIDUsageTables.h
CFLAGS.IOHIDManager.o = -ObjC++ -Wno-elaborated-enum-base -I${.CURDIR:H}/xnu/libkern \
	-I${.CURDIR:H}/xnu/libkern/libkern \
	-I${.CURDIR}/hid.subproj \
	-I${.CURDIR:H}/Extensions/IOHIDFamily/IOHIDFamily \
	-include${MINKAR_SDKROOT}/usr/include/math.h \
	-include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h \
	-include${.CURDIR}/CFIOKitShim.h
CFLAGS.IOHIDQueue.o = -ObjC++ -Wno-elaborated-enum-base -I${.CURDIR:H}/xnu/libkern \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libdispatch/private \
	-include${MINKAR_SDKROOT}/usr/include/math.h \
	-include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h
CFLAGS.IOHIDTransaction.o = -ObjC++ -Wno-elaborated-enum-base -I${.CURDIR:H}/xnu/libkern \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libdispatch/private \
	-include${MINKAR_SDKROOT}/usr/include/math.h \
	-include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h
CFLAGS.IOPMPowerNotifications.o = -I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_c \
        -I${.CURDIR}
CFLAGS.OSKext.o = -I${.CURDIR}/kext.subproj -I${.CURDIR}/stubs \
        -I${.CURDIR:H}/xnu/libkern/libkern
CFLAGS.IOPMAssertions.o = -I${.CURDIR} -I${.CURDIR}/stubs
CFLAGS.IOHIDUserDevice.o = -ObjC++ -Wno-elaborated-enum-base \
        -Wno-c++11-narrowing -I${.CURDIR:H}/xnu/libkern \
	-I${ROOT_SOURCE_DIR}/Libraries/Libsystem/libdispatch/private \
        -I${.CURDIR}/hid.subproj \
	-include${MINKAR_SDKROOT}/usr/include/math.h \
	-include${MINKAR_SDKROOT}/usr/local/include/kernel/os/object.h \
	-include${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders/hidobjc/hidobjcbase.h

MIG_DEFS = -DIOKIT -DPRIVATE -DNO_SECURITY_FRAMEWORK
MIGFLAGS = -cc ${CC} -novouchers -server /dev/null ${MIG_DEFS} \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-mmacos-version-min=${MACOS_VERSION_MIN}

iokitPrivateHdrs = ${MINKAR_SDKROOT}/System/Library/Frameworks/IOKit.framework/Versions/A/PrivateHeaders
iokitHdrs = ${MINKAR_SDKROOT}/System/Library/Frameworks/IOKit.framework/Versions/A/Headers

# IOKit's Objective-C (the HID classes) needs only libobjc. As on macOS it
# must not pull in Foundation: with Foundation loaded, CoreFoundation
# bridges its objects to Foundation classes in every IOKit client.
MK_OBJC_RUNTIME = no

# The HID event system (IOHIDEvent, IOHIDEventSystemClient,
# IOHIDServiceClient...): Apple never published it, and its files here are
# empty. Ours comes with WindowServer's input (architecture plan, Phase 4).
# Until then only these may stay undefined; any other undefined symbol
# fails the link.
HID_EVENT_SYSTEM_PENDING = \
	_IOHIDEventAppendEvent \
	_IOHIDEventConformsTo \
	_IOHIDEventCopyDescription \
	_IOHIDEventCreateVendorDefinedEvent \
	_IOHIDEventGetChildren \
	_IOHIDEventGetIntegerValue \
	_IOHIDEventGetVendorDefinedData \
	_IOHIDEventSystemClientCopyProperty \
	_IOHIDEventSystemClientCopyServiceForRegistryID \
	_IOHIDEventSystemClientCopyServices \
	_IOHIDEventSystemClientCreateWithType \
	_IOHIDEventSystemClientSetProperty \
	_IOHIDEventSystemConnectionCopyDescription \
	__IOHIDEventSystemConnectionReleasePrivate \
	_IOHIDServiceClientConformsTo \
	_IOHIDServiceClientCopyDescription \
	_IOHIDServiceClientCopyProperty \
	__IOHIDServiceClientReleasePrivate \
	_IOHIDServiceClientSetProperty \
	_IOHIDServiceCopyDescription \
	__IOHIDServiceReleasePrivate \
	__IOHIDSessionReleasePrivate

LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wl,-compatibility_version,1.0.0 -Wl,-current_version,275.0.0 \
	${HID_EVENT_SYSTEM_PENDING:S/^/-Wl,-U,/} \
        -L${MINKAR_SDKROOT}/usr/local/lib/kernel -lcc_kext -lkxld \
        -framework CoreFoundation -lobjc -lc++ -lz -lSystem

# Headers first: the sources include them from the SDK, also with -j.
all: copy_headers .WAIT libkxld.a ${FRAMEWORK} to-sysroot

to-sysroot: ${FRAMEWORK}
	cp -Rf ${.OBJDIR}/IOKit.framework ${MINKAR_SDKROOT}/System/Library/Frameworks/
	cp -Rf ${.OBJDIR}/IOKit.framework ${SYSROOT_DIR}/System/Library/Frameworks/

copy_headers: .PHONY
	# IOKit/hid and hidobjc headers (IOHIDEvent.h...): IOHIDFamily's HID
	# framework is not built yet, but its header list is the one in use.
	${MAKE} -C ${ROOT_SOURCE_DIR}/Kernel/Extensions/IOHIDFamily/HID copy_headers
	mkdir -p ${MINKAR_SDKROOT}/usr/local/include
	cp -f ${.CURDIR}/kext.subproj/bootfiles.h ${MINKAR_SDKROOT}/usr/local/include/
	mkdir -p ${iokitPrivateHdrs}/hidobjc
	cp -f \
		${.CURDIR}/hid.subproj/HIDDeviceBase.h \
		${.CURDIR}/hid.subproj/HIDDeviceIvar.h  \
		${.CURDIR}/hid.subproj/HIDServiceClientIvar.h \
		${.CURDIR}/hid.subproj/HIDServiceClientBase.h \
		${.CURDIR}/hid.subproj/HIDElementBase.h \
		${.CURDIR}/hid.subproj/HIDElementIvar.h \
		${.CURDIR}/hid.subproj/HIDConnectionBase.h \
		${.CURDIR}/hid.subproj/HIDConnectionIvar.h \
		${.CURDIR}/hid.subproj/HIDSessionIvar.h \
		${.CURDIR}/hid.subproj/HIDSessionBase.h \
		${.CURDIR}/hid.subproj/HIDEventBase.h \
		${.CURDIR}/hid.subproj/HIDServiceBase.h \
		${.CURDIR}/hid.subproj/HIDServiceIvar.h \
		${.CURDIR}/hid.subproj/hidobjcbase.h \
		${iokitPrivateHdrs}/hidobjc/
	mkdir -p ${iokitHdrs}/graphics
	cp -Rf ${.CURDIR}/graphics.subproj/IOAccelSurfaceControl.h \
		${.CURDIR}/graphics.subproj/IOGraphicsLibPrivate.h \
		${.CURDIR}/graphics.subproj/IOGraphicsLib.h \
		${.CURDIR:H}/Extensions/IOGraphics/IOGraphicsFamily/IOKit/graphics/* \
		${iokitHdrs}/graphics/
	mkdir -p ${iokitHdrs}/i2c
	cp -Rf ${.CURDIR:H}/Extensions/IOGraphics/IOGraphicsFamily/IOKit/i2c/* \
		${iokitHdrs}/i2c
	mkdir -p ${MINKAR_SDKROOT}/usr/local/include/IOKit/kext
	cp -f \
		${.CURDIR}/kext.subproj/OSKext.h \
		${.CURDIR}/kext.subproj/OSKextPrivate.h \
		${.CURDIR}/kext.subproj/misc_util.h \
		${.CURDIR}/kext.subproj/fat_util.h \
		${.CURDIR}/kext.subproj/macho_util.h \
		${.CURDIR}/kext.subproj/KextManager.h \
		${.CURDIR}/kext.subproj/KextManagerPriv.h \
		${.CURDIR}/kext.subproj/kextmanager_types.h \
		${.CURDIR}/kext.subproj/kextmanager_mig.defs \
		${MINKAR_SDKROOT}/usr/local/include/IOKit/kext/
	mkdir -p ${iokitHdrs}/audio
	cp -f ${.CURDIR}/audio.subproj/IOAudioLib.h ${iokitHdrs}/audio/
	mkdir -p ${iokitHdrs}/hidsystem
	cp -f \
		${.CURDIR}/hidsystem.subproj/IOHIDUserDevice.h \
		${.CURDIR}/hidsystem.subproj/IOHIDServiceClient.h \
		${.CURDIR}/hidsystem.subproj/IOHIDEventSystemClient.h \
		${.CURDIR}/hidsystem.subproj/IOHIDLib.h \
		${.CURDIR}/hidsystem.subproj/event_status_driver.h \
		${iokitHdrs}/hidsystem/
	mkdir -p ${iokitHdrs}/hid
	cp -f \
		${.CURDIR}/hid.subproj/IOHIDAnalytics.h \
		${.CURDIR}/hid.subproj/IOHIDEventPrivate.h \
		${.CURDIR}/hid.subproj/IOHIDConnectionFilterPlugin.h \
		${.CURDIR}/hid.subproj/IOHIDServiceConnectionCache.h \
		${.CURDIR}/hid.subproj/IOHIDEventSystemClientPrivate.h \
		${.CURDIR}/hid.subproj/IOHIDEventSystemConnection.h \
		${.CURDIR}/hid.subproj/IOHIDEventSystemPrivate.h \
		${.CURDIR}/hid.subproj/IOHIDEventSystemKeysPrivate.h \
		${.CURDIR}/hid.subproj/IOHIDServiceClient.h \
		${.CURDIR}/hid.subproj/IOHIDServiceKeys.h \
		${.CURDIR}/hid.subproj/IOHIDServicePlugIn.h \
		${.CURDIR}/hid.subproj/IOHIDEventQueue.h \
		${.CURDIR}/hid.subproj/IOHIDEventSystem.h \
		${.CURDIR}/hidsystem.subproj/IOHIDEventSystemClient.h \
		${.CURDIR}/hid.subproj/IOHIDEventSystemKeys.h \
		${.CURDIR}/hid.subproj/IOHIDNotification.h \
		${.CURDIR}/hid.subproj/IOHIDServicePrivate.h \
		${.CURDIR}/hid.subproj/IOHIDService.h \
		${.CURDIR}/hid.subproj/IOHIDTypes.h \
		${.CURDIR}/hid.subproj/IOHIDServiceFilterPlugIn.h \
		${.CURDIR}/hid.subproj/IOHIDSessionFilterPlugIn.h \
		${.CURDIR}/hid.subproj/IOHIDUserDevice.h \
		${.CURDIR}/hid.subproj/IOHIDBase.h \
		${.CURDIR}/hid.subproj/IOHIDDevice.h \
		${.CURDIR}/hid.subproj/IOHIDDevicePlugIn.h \
		${.CURDIR}/hid.subproj/IOHIDElement.h \
		${.CURDIR}/hid.subproj/IOHIDManager.h \
		${.CURDIR}/hid.subproj/IOHIDQueue.h \
		${.CURDIR}/hid.subproj/IOHIDTransaction.h \
		${.CURDIR}/hid.subproj/IOHIDValue.h \
		${.CURDIR}/hid.subproj/IOHIDLibPrivate.h \
		${.CURDIR}/hid.subproj/IOHIDLib.h \
		${.CURDIR:H}/Extensions/IOHIDFamily/IOHIDFamily/IOHIDProperties.h \
		${iokitHdrs}/hid/
	mkdir -p ${iokitHdrs}/network
	cp -f \
		${.CURDIR}/network.subproj/IOUserEthernetController.h \
		${.CURDIR}/network.subproj/IONetworkLib.h \
		${iokitHdrs}/network/
	# The family's headers, as in macOS (SystemConfiguration includes
	# IONetworkInterface.h and IONetworkController.h).
	cp -f ${.CURDIR:H}/Extensions/IONetworkingFamily/IOKit/network/*.h \
		${iokitHdrs}/network/
	# Also used by SystemConfiguration, configd and fsck_hfs.
	cp -f ${.CURDIR}/IOCFBundle.h ${iokitHdrs}/
	mkdir -p ${iokitHdrs}/storage ${iokitHdrs}/usb
.for h in IOBlockStorageDevice.h IOMedia.h IOStorageDeviceCharacteristics.h
	cp -f ${.CURDIR:H}/Extensions/IOStorageFamily/IOKit/storage/${h} \
		${iokitHdrs}/storage/
.endfor
	cp -f ${ROOT_SOURCE_DIR}/Frameworks/IOKit/usb/*.h ${iokitHdrs}/usb/
	mkdir -p ${iokitHdrs}/ps
	cp -f \
		${.CURDIR}/ps.subproj/IOPowerSourcesPrivate.h \
		${.CURDIR}/ps.subproj/IOPowerSources.h \
		${.CURDIR}/ps.subproj/IOPSKeysPrivate.h \
		${.CURDIR}/ps.subproj/IOPSKeys.h \
		${iokitHdrs}/ps/
	mkdir -p ${iokitHdrs}/pwr_mgt
	cp -f \
		${.CURDIR}/pwr_mgt.subproj/IOPMUPSPrivate.h \
		${.CURDIR}/pwr_mgt.subproj/IOPMLibPrivate.h \
		${.CURDIR}/pwr_mgt.subproj/IOPMKeys.h \
		${.CURDIR}/pwr_mgt.subproj/IOPMLib.h \
		${.CURDIR}/pwr_mgt.subproj/powermanagement.defs \
		${.CURDIR}/pwr_mgt.subproj/powermanagement_mig.h \
		${iokitHdrs}/pwr_mgt/
	cp -f \
		${.CURDIR}/IOKitLibPrivate.h \
		${.CURDIR}/IOCFSerialize.h \
		${.CURDIR}/IOCFUnserialize.h \
		${.CURDIR}/IOKitLib.h \
		${.CURDIR}/IOCFURLAccess.h \
		${.CURDIR}/IODataQueueClient.h \
		${iokitHdrs}/
	mkdir -p ${iokitHdrs}/platform
	cp -f ${.CURDIR}/platform.subproj/IOPlatformSupportPrivate.h \
		${iokitHdrs}/platform/
	rm -f ${MINKAR_SDKROOT}${SLF}/IOKit.framework/Headers
	rm -f ${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders
	ln -sf Versions/A/Headers ${MINKAR_SDKROOT}${SLF}/IOKit.framework/Headers
	ln -sf Versions/A/PrivateHeaders \
	  ${MINKAR_SDKROOT}${SLF}/IOKit.framework/PrivateHeaders
	mkdir -p ${MINKAR_SDKROOT}${SLF}/DriverKit.framework/Versions/A/Headers
	cp -f ${ROOT_SOURCE_DIR}/Kernel/xnu/libkern/libkern/c++/OSMetaClass.h \
		${MINKAR_SDKROOT}${SLF}/DriverKit.framework/Versions/A/Headers
	rm -f ${MINKAR_SDKROOT}${SLF}/DriverKit.framework/Headers \
		${MINKAR_SDKROOT}${SLF}/DriverKit.framework/PrivateHeaders
	ln -sf Versions/A/Headers ${MINKAR_SDKROOT}${SLF}/DriverKit.framework/Headers
	ln -sf Versions/A/PrivateHeaders \
		${MINKAR_SDKROOT}${SLF}/DriverKit.framework/PrivateHeaders

kextmanager_mig.c kextmanager_mig.h: ${.CURDIR}/kext.subproj/kextmanager_mig.defs
	${MIG} ${MIGFLAGS} -header kextmanager_mig.h -user kextmanager_mig.c \
		${.ALLSRC}

powermanagement.c powermanagement.h: ${.CURDIR}/pwr_mgt.subproj/powermanagement.defs
	${MIG} ${MIGFLAGS} -header powermanagement.h -user powermanagement.c \
		${.ALLSRC}

iokitmig64.h iokitmig64.c: ${.CURDIR}/iokitmig.defs
	${MIG} ${MIGFLAGS} -header iokitmig64.h -user iokitmig64.c \
		${.CURDIR}/iokitmig.defs

libkxld.a:
	${MAKE} -C${.CURDIR} -f Makefile.kxld all
	cp -f ${.OBJDIR}/${.TARGET} ${MINKAR_SDKROOT}/usr/local/lib/kernel

SUBDIR =

.include <minkar.framework.mk>
.include <bsd.subdir.mk>
