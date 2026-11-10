# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Swift's overlays for the system's C and Objective-C modules (Phase 3):
# ObjectiveC, Dispatch, CoreFoundation, CoreGraphics and Foundation, from
# Swift 5.4, the last
# release that has them in the open (MANIFEST; our patches in patches/),
# built with the toolchain's Swift 6.0. Each is a library evolution
# module (its .swiftinterface beside the .swiftmodule) next to Darwin's
# in the compiler's resource directory, and a /usr/lib/swift dylib that
# programs link through the module's link name. Built from
# Frameworks/Makefile, after the frameworks whose modules they extend.
# CoreGraphics's CGFloat and Foundation's NSValue come from gyb templates
# (Swift's gyb, from the same release). Dispatch's and Foundation's
# schedulers and publishers are Combine's (Frameworks/Combine).

SUBDIR =
FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
TC =		${TOOLCHAIN}/usr/bin
RESOURCE =	${TOOLCHAIN}/usr/lib/swift/macosx
TRIPLE =	x86_64-apple-macosx10.15
MODTRIPLE =	x86_64-apple-macos

PATCHES =	0001-dispatch-without-runtime-headers.patch \
		0002-foundation-without-security.patch \
		0003-foundation-thunks-without-runtime-headers.patch
SRC =		${.OBJDIR}/swift54
DARWIN =	${SRC}/stdlib/public/Darwin
OUT =		${.OBJDIR}/out
SHIMS =		${.OBJDIR}/overlay-shims

# In dependency order.
OVERLAYS =	ObjectiveC Dispatch CoreFoundation CoreGraphics Foundation
SOURCES_ObjectiveC =	ObjectiveC.swift
SOURCES_Dispatch =	Dispatch.swift Block.swift Data.swift IO.swift Private.swift \
			Queue.swift Source.swift Time.swift Schedulers+DispatchQueue.swift
OBJC_Dispatch =		Dispatch.mm
SOURCES_CoreFoundation = CoreFoundation.swift
SOURCES_CoreGraphics =	CoreGraphics.swift Private.swift
GENERATED_CoreGraphics = ${OUT}/CGFloat.swift
SOURCES_Foundation =	AffineTransform.swift Boxing.swift Calendar.swift \
			CharacterSet.swift CheckClass.swift Codable.swift \
			Collections+DataProtocol.swift CombineTypealiases.swift \
			ContiguousBytes.swift \
			Data.swift DataProtocol.swift Date.swift DateComponents.swift \
			DateInterval.swift Decimal.swift DispatchData+DataProtocol.swift \
			FileManager.swift Foundation.swift IndexPath.swift IndexSet.swift \
			JSONEncoder.swift Locale.swift Measurement.swift Notification.swift \
			NSArray.swift NSCoder.swift NSData+DataProtocol.swift NSDate.swift \
			NSDictionary.swift NSError.swift NSExpression.swift \
			NSFastEnumeration.swift NSGeometry.swift NSIndexSet.swift \
			NSItemProvider.swift NSNumber.swift NSObject.swift \
			NSOrderedCollectionDifference.swift NSPredicate.swift NSRange.swift \
			NSSet.swift NSSortDescriptor.swift NSString.swift NSStringAPI.swift \
			NSStringEncodings.swift NSTextCheckingResult.swift NSURL.swift \
			NSUndoManager.swift PersonNameComponents.swift PlistEncoder.swift \
			Pointers+DataProtocol.swift Progress.swift \
			Publishers+KeyValueObserving.swift Publishers+Locking.swift \
			Publishers+NotificationCenter.swift Publishers+Timer.swift \
			Publishers+URLSession.swift ReferenceConvertible.swift \
			Scanner.swift Schedulers+Date.swift Schedulers+OperationQueue.swift \
			Schedulers+RunLoop.swift \
			String.swift TimeZone.swift URL.swift URLCache.swift \
			URLComponents.swift URLRequest.swift URLSession.swift UUID.swift
GENERATED_Foundation =	${OUT}/NSValue.swift
# Minkar's own Swift in an overlay (in ${.CURDIR}/<overlay>): API the
# header cannot name, and API 21's asynchronous API.
MINKAR_Foundation =	FileHandle.swift AsyncBytes.swift URLSession+Async.swift \
			CodableWithConfiguration.swift
OBJC_Foundation =	DataThunks.m
# Dispatch.mm is written for ARC, DataThunks.m for manual retain and release.
OBJCFLAGS_Dispatch =	-fobjc-arc
OBJCFLAGS_Foundation =	-fno-objc-arc
FLAGS_ObjectiveC =	-disable-objc-attr-requires-foundation-module
LIBS_ObjectiveC =	-lobjc
# Dispatch retains the blocks it is given: CoreFoundation makes them
# Objective-C objects (CFObjCClasses.c).
LIBS_Dispatch =		-lobjc -framework CoreFoundation -framework Combine
LIBS_CoreFoundation =	-framework CoreFoundation
LIBS_CoreGraphics =	-framework CoreGraphics -framework CoreFoundation
LIBS_Foundation =	-framework Foundation -framework CoreFoundation -framework Combine -lobjc \
			-lswiftObjectiveC -lswiftDispatch -lswiftCoreFoundation \
			-lswiftCoreGraphics

TOOLENV =	env -u MACOSX_DEPLOYMENT_TARGET

all: install-overlays

${SRC}/.minkar-patches: ${PATCHES:S,^,${.CURDIR}/patches/,} ${.CURDIR}/MANIFEST ${.CURDIR}/Makefile
	pristine=$$(${FETCH} ${.CURDIR}/MANIFEST ${CACHE}) && \
	rm -rf ${SRC} ${SRC}.part && mkdir -p ${SRC}.part/stdlib/public && \
	cp -a $$pristine/stdlib/public/Darwin $$pristine/stdlib/public/SwiftShims \
	    ${SRC}.part/stdlib/public/ && \
	mkdir -p ${SRC}.part/utils && \
	cp -a $$pristine/utils/gyb.py $$pristine/utils/SwiftIntTypes.py \
	    $$pristine/utils/gyb_foundation_support.py ${SRC}.part/utils/ && \
	for p in ${PATCHES}; do \
		patch -s -p1 -d ${SRC}.part < ${.CURDIR}/patches/$$p || exit 1; \
	done && \
	mv ${SRC}.part ${SRC}
	cat ${.ALLSRC} | cksum > ${.TARGET}

# The overlays' sources come from the fetched and patched tree above.
.for o in ${OVERLAYS}
${SOURCES_${o}:S,^,${DARWIN}/${o}/,}: ${SRC}/.minkar-patches
.endfor

# Each overlay imports the ones before it in OVERLAYS (Foundation uses
# CoreGraphics's CGFloat), also with -j.
_prev =
.for o in ${OVERLAYS}
.if !empty(_prev)
${OUT}/${o}.o: ${OUT}/${_prev}.o
.endif
_prev := ${o}
.endfor

# The overlays' own C headers (imported @_implementationOnly: only for
# building them). System modules, as in Swift's resource directory: their
# NSUInteger is Swift's Int.
${SHIMS}/module.modulemap: ${SRC}/.minkar-patches
	rm -rf ${SHIMS} && mkdir -p ${SHIMS}
	cp ${SRC}/stdlib/public/SwiftShims/*OverlayShims.h ${SRC}/stdlib/public/SwiftShims/CF*Shims.h \
	    ${SRC}/stdlib/public/SwiftShims/NS*Shims.h \
	    ${SRC}/stdlib/public/SwiftShims/FoundationShimSupport.h ${SHIMS}/
	awk '/^module _Swift[A-Za-z]*OverlayShims/,/^}/' \
	    ${SRC}/stdlib/public/SwiftShims/module.modulemap | \
	    sed 's/^\(module _Swift[A-Za-z]*OverlayShims\) {/\1 [system] {/' > ${.TARGET}

${OUT}/CGFloat.swift: ${SRC}/.minkar-patches
	mkdir -p ${OUT}
	PYTHONPATH=${SRC}/utils python3 ${SRC}/utils/gyb.py -DCMAKE_SIZEOF_VOID_P=8 \
	    --line-directive '' -o ${.TARGET} ${DARWIN}/CoreGraphics/CGFloat.swift.gyb

${OUT}/NSValue.swift: ${SRC}/.minkar-patches
	mkdir -p ${OUT}
	PYTHONPATH=${SRC}/utils python3 ${SRC}/utils/gyb.py -DCMAKE_SIZEOF_VOID_P=8 \
	    --line-directive '' -o ${.TARGET} ${DARWIN}/Foundation/NSValue.swift.gyb

.for o in ${OVERLAYS}
${OUT}/${o}.o: ${SHIMS}/module.modulemap ${SOURCES_${o}:S,^,${DARWIN}/${o}/,} ${GENERATED_${o}} ${.CURDIR}/Makefile \
	    ${MINKAR_${o}:S,^,${.CURDIR}/${o}/,}
	mkdir -p ${OUT}/${o}.swiftmodule
	${TOOLENV} ${TC}/swiftc -frontend -c -O -parse-as-library -enable-library-evolution -language-mode 5 \
	    -module-name ${o} -module-link-name swift${o} -target ${TRIPLE} \
	    -sdk ${MINKAR_SDKROOT} -runtime-compatibility-version none \
	    -I ${SHIMS} -I ${OUT} -module-cache-path ${.OBJDIR}/module-cache ${FLAGS_${o}} \
	    -emit-module-path ${OUT}/${o}.swiftmodule/${MODTRIPLE}.swiftmodule \
	    -emit-module-interface-path ${OUT}/${o}.swiftmodule/${MODTRIPLE}.swiftinterface \
	    -emit-module-doc-path ${OUT}/${o}.swiftmodule/${MODTRIPLE}.swiftdoc \
	    ${SOURCES_${o}:S,^,${DARWIN}/${o}/,} ${GENERATED_${o}} \
	    ${MINKAR_${o}:S,^,${.CURDIR}/${o}/,} -o ${.TARGET}

${OUT}/libswift${o}.dylib: ${OUT}/${o}.o ${.CURDIR}/Makefile
	cd ${OUT} && for m in ${OBJC_${o}}; do \
		${TOOLENV} ${TC}/clang -c -O2 ${OBJCFLAGS_${o}} -target ${TRIPLE} \
		    -isysroot ${MINKAR_SDKROOT} -Wno-incompatible-sysroot \
		    ${DARWIN}/${o}/$$m -o ${o}-$${m%.*}.o || exit 1; \
	done
	${TOOLENV} ${TC}/clang -dynamiclib -target ${TRIPLE} -isysroot ${MINKAR_SDKROOT} \
	    -fuse-ld=${TC}/ld64 -Wno-incompatible-sysroot \
	    -install_name /usr/lib/swift/libswift${o}.dylib \
	    ${OUT}/${o}.o ${OBJC_${o}:R:S,^,${OUT}/${o}-,:S,$,.o,} \
	    -L${MINKAR_SDKROOT}/usr/lib/swift -L${OUT} ${LIBS_${o}} -o ${.TARGET}
.endfor

install-overlays: .PHONY ${OVERLAYS:S,^,${OUT}/libswift,:S,$,.dylib,}
	mkdir -p ${SYSROOT_DIR}/usr/lib/swift ${MINKAR_SDKROOT}/usr/lib/swift ${RESOURCE} \
	    ${SYSROOT_DIR}/usr/share/swift
.for o in ${OVERLAYS}
	install -m 0755 ${OUT}/libswift${o}.dylib ${SYSROOT_DIR}/usr/lib/swift/
	install -m 0755 ${OUT}/libswift${o}.dylib ${MINKAR_SDKROOT}/usr/lib/swift/
	rm -rf ${RESOURCE}/${o}.swiftmodule
	cp -R ${OUT}/${o}.swiftmodule ${RESOURCE}/
.endfor

clean: .PHONY
	rm -rf ${SRC} ${SHIMS} ${OUT} ${.OBJDIR}/module-cache

.include <bsd.prog.mk>
