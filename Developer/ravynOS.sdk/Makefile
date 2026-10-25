.include <bsd.own.mk>

PLATFORM_DIR = ${ROOT_BINARY_DIR}/Developer/Platforms/ravynOS.platform
FPATH = /System/Library/Frameworks/System.framework
libc_path = ${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_c
objc_path = ${ROOT_SOURCE_DIR}/Libraries/objc4

all: ${PLATFORM_DIR} ${TOOLS}/unifdef .EXEC
	mkdir -p ${MINKAR_SDKROOT}${FPATH}/Versions/B
	mkdir -p ${MINKAR_SDKROOT}/usr/local/include
	mkdir -p ${MINKAR_SDKROOT}/usr/local/lib
	mkdir -p ${MINKAR_SDKROOT}/usr/local/bin
	mkdir -p ${MINKAR_SDKROOT}/usr/include
	# libdispatch's <os/object.h> and the os/ headers it pulls in, where
	# <dispatch/dispatch.h> looks for them (their sources are with the
	# kernel copies in usr/local/include/kernel/os).
	mkdir -p ${MINKAR_SDKROOT}/usr/include/os
.for h in object.h object_private.h voucher_private.h voucher_activity_private.h \
	firehose_buffer_private.h firehose_server_private.h generic_unix_base.h
	cp -f ${.CURDIR}/usr/local/include/kernel/os/${h} ${MINKAR_SDKROOT}/usr/include/os/
.endfor
	mkdir -p ${MINKAR_SDKROOT}/usr/lib
	mkdir -p ${MINKAR_SDKROOT}/usr/bin
	cp -f ${.CURDIR}/SDKSettings.plist ${.CURDIR}/SDKSettings.json \
		${.CURDIR}/Entitlements.plist ${MINKAR_SDKROOT}/
	ln -sf Versions/Current/PrivateHeaders ${MINKAR_SDKROOT}${FPATH}/PrivateHeaders
	ln -sf Versions/Current/Resources ${MINKAR_SDKROOT}${FPATH}/Resources
	ln -sf B ${MINKAR_SDKROOT}${FPATH}/Versions/Current
	ln -sf Versions/Current/System ${MINKAR_SDKROOT}${FPATH}/System
	ln -sf ../../../../../../usr/lib/libSystem.B.dylib ${MINKAR_SDKROOT}${FPATH}/Versions/B/System
	ln -sf libSystem.B.dylib ${MINKAR_SDKROOT}/usr/lib/libSystem.dylib
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks/Kernel.framework/Versions/A
	ln -sf Versions/A/Headers ${MINKAR_SDKROOT}/System/Library/Frameworks/Kernel.framework/Headers
	ln -sf Versions/A/PrivateHeaders ${MINKAR_SDKROOT}/System/Library/Frameworks/Kernel.framework/PrivateHeaders
	# xnu installs IOReturn.h & co. in IOKit.framework/Versions/A, and
	# Libsystem includes them as <IOKit/...> before IOKitUser is built.
	mkdir -p ${MINKAR_SDKROOT}/System/Library/Frameworks/IOKit.framework/Versions/A/Headers \
		${MINKAR_SDKROOT}/System/Library/Frameworks/IOKit.framework/Versions/A/PrivateHeaders
	ln -sfn Versions/A/Headers ${MINKAR_SDKROOT}/System/Library/Frameworks/IOKit.framework/Headers
	ln -sfn Versions/A/PrivateHeaders ${MINKAR_SDKROOT}/System/Library/Frameworks/IOKit.framework/PrivateHeaders
	# Compatibility symlinks
	ln -sf ravynOS.platform ${ROOT_BINARY_DIR}/Developer/Platforms/MacOSX.platform
	ln -sf ravynOS.sdk ${ROOT_BINARY_DIR}/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
	cp -Rf ${.CURDIR}/usr ${.CURDIR}/System ${MINKAR_SDKROOT}/
	cp -f ${ROOT_SOURCE_DIR}/Libraries/Libsystem/private/pthread/tsd_private.h \
		${MINKAR_SDKROOT}/usr/local/include/pthread/
	${ROOT_SOURCE_DIR}/Kernel/xnu/bsd/sys/make_symbol_aliasing.sh \
		${MINKAR_SDKROOT} ${MINKAR_SDKROOT}/usr/include/sys/_symbol_aliasing.h
	env SRCROOT=${libc_path} DSTROOT=${MINKAR_SDKROOT} \
		SDK_INSTALL_HEADERS_ROOT=. "PATH=${TOOLS}:${PATH}" ARCHS="${CpuArch} " \
		VARIANT_PLATFORM_NAME=macosx bash ${libc_path}/xcodescripts/headers.sh
	cd ${ROOT_SOURCE_DIR}/Libraries/Libsystem/libsystem_pthread && \
		env DSTROOT=${MINKAR_SDKROOT} SDK_INSTALL_HEADERS_ROOT=. \
		sh xcodescripts/install-sys-headers.sh
	mkdir -p ${MINKAR_SDKROOT}/usr/include/objc
	cp -fR ${objc_path}/runtime/runtime.h ${objc_path}/runtime/objc.h \
		${objc_path}/runtime/objc-sync.h ${objc_path}/runtime/objc-exception.h \
		${objc_path}/runtime/objc-auto.h ${objc_path}/runtime/objc-api.h \
		${objc_path}/runtime/message.h ${objc_path}/runtime/NSObject.h \
		${objc_path}/runtime/NSObjCRuntime.h ${objc_path}/runtime/objc-runtime.h \
		${objc_path}/runtime/Module \
		${MINKAR_SDKROOT}/usr/include/objc/
	mkdir -p ${MINKAR_SDKROOT}/usr/local/include/objc
	cp -f ${objc_path}/runtime/maptable.h ${objc_path}/runtime/NSObject-internal.h \
		${objc_path}/runtime/objc-abi.h ${objc_path}/runtime/objc-block-trampolines.h \
		${objc_path}/runtime/objc-gdb.h ${objc_path}/runtime/objc-internal.h \
		${objc_path}/runtime/objc-ptrauth.h ${objc_path}/runtime/hashtable.h \
		${objc_path}/runtime/hashtable2.h ${objc_path}/runtime/objc-class.h \
		${MINKAR_SDKROOT}/usr/local/include/objc/

SUBDIR = rpcsvc

.include <bsd.subdir.mk>

