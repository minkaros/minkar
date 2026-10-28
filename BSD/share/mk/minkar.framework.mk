# minkar.framework.mk: builds a framework bundle (from ravynOS).
# Modifications Copyright (c) 2026 The Minkar contributors
FRAMEWORK_DIR = ${.OBJDIR}/${FRAMEWORK}.framework
FMWK_VERSION ?= A
NO_ROOT=yes

UID != id -u
.if defined(UID) && ${UID} != 0
INSTALLFLAGS=-U
.endif

.if defined(MACOS_VERSION_MIN)
CFLAGS += -mmacos-version-min=${MACOS_VERSION_MIN}
LDFLAGS += -mmacos-version-min=${MACOS_VERSION_MIN}
.endif

.if defined(MINKAR_SDKROOT)
CFLAGS += --sysroot=${MINKAR_SDKROOT}
LDFLAGS += --sysroot=${MINKAR_SDKROOT}
.else
.if defined(SDKROOT)
CFLAGS += --sysroot=${SDKROOT}
LDFLAGS += --sysroot=${SDKROOT}
.endif
.endif

# The install name is where the framework lives in the system, as on
# macOS; without one, ld64 would record the object directory and nothing
# linked against the framework could load it.
FMWK_INSTALL_DIR ?= /System/Library/Frameworks
.if defined(INSTALL_NAME) && !empty(INSTALL_NAME)
LDFLAGS += -Wl,-install_name,${INSTALL_NAME}
.else
INSTALL_NAME_DIR ?= ${FMWK_INSTALL_DIR}/${FRAMEWORK}.framework/Versions/${FMWK_VERSION}
LDFLAGS += -Wl,-install_name,${INSTALL_NAME_DIR}/${FRAMEWORK}
.endif

# dyld refuses a library older than the compatibility version a program
# was linked with: use macOS's (otool -L of a macOS app shows them).
.if defined(FMWK_COMPAT_VERSION) && !empty(FMWK_COMPAT_VERSION)
FMWK_CURRENT_VERSION ?= ${FMWK_COMPAT_VERSION}
LDFLAGS += -Wl,-compatibility_version,${FMWK_COMPAT_VERSION} \
	   -Wl,-current_version,${FMWK_CURRENT_VERSION}
.endif

.if defined(SRCS) && !empty(SRCS)
LIBMODE ?= 0555
LIB = ${FRAMEWORK}
.endif

.for rpath in ${RPATHS}
LDFLAGS += -rpath,${rpath}
.endfor

RESOURCES_DIR = ${FRAMEWORK_DIR}/Versions/${FMWK_VERSION}/Resources
HEADER_DIR = ${FRAMEWORK_DIR}/Versions/${FMWK_VERSION}/Headers
PRIVHDR_DIR = ${FRAMEWORK_DIR}/Versions/${FMWK_VERSION}/PrivateHeaders

${FRAMEWORK}: ${FRAMEWORK_DIR} lib${LIB}.a
	${CC} -shared -dylib \
	  -o ${FRAMEWORK_DIR}/Versions/${FMWK_VERSION}/${FRAMEWORK} \
	  -Wl,-force_load,${.OBJDIR}/lib${LIB}.a ${LDFLAGS}

.if defined(HEADERS) && !empty(HEADERS)
${HEADER_DIR}:
	mkdir -p ${HEADER_DIR}

_copy_headers: ${HEADER_DIR}
.for hdr in ${HEADERS}
	cp -f ${.CURDIR}/${hdr} ${HEADER_DIR}/${hdr:T}
.endfor

${FRAMEWORK}: _copy_headers
_fmwk_headers += _copy_headers
.endif

.if defined(PRIVATE_HEADERS) && !empty(PRIVATE_HEADERS)
_copy_private_headers: ${FRAMEWORK_DIR}
	mkdir -p ${PRIVHDR_DIR}
.for hdr in ${PRIVATE_HEADERS}
	cp -f ${.CURDIR}/${hdr} ${PRIVHDR_DIR}/${hdr:T}
.endfor

${FRAMEWORK}: _copy_private_headers
_fmwk_headers += _copy_private_headers
.endif

.if defined(SRCS) && !empty(SRCS) && !empty(_fmwk_headers)
# The sources include the framework's own headers from the bundle: copy
# them first, also with -j (bsd.lib.mk adds the objects after the .WAIT).
lib${LIB}.a: ${_fmwk_headers} .WAIT
.endif

${RESOURCES_DIR}:
	mkdir -p ${RESOURCES_DIR}

.if defined(RESOURCES_DIRS) && !empty(RESOURCES_DIRS)
${FRAMEWORK}: _resource_dirs

_resource_dirs: ${RESOURCES_DIR}
.for rsc in ${RESOURCES_DIRS}
	mkdir -p ${RESOURCES_DIR}/${rsc:H}
	cp -fR ${.CURDIR}/${rsc} ${RESOURCES_DIR}/${rsc:H}
.endfor
.endif

.if defined(RESOURCES_FILES) && !empty(RESOURCES_FILES)
${FRAMEWORK}: _resource_files

_resource_files: ${RESOURCES_DIR}
.for rsc in ${RESOURCES_FILES}
	cp -fv ${.CURDIR}/${rsc} ${RESOURCES_DIR}
.endfor
.endif

# Every framework has an Info.plist, as on macOS: CFBundle reads its
# identifier and version there. INFO_PLIST (by default the Info.plist
# next to the framework's Makefile; empty for none) with Xcode's
# variables and template identifiers filled in, or a minimal one. One
# copied by RESOURCES_FILES is filled in too; one an install hook
# writes replaces it.
INFO_PLIST ?=	Info.plist
FMWK_BUNDLE_ID ?= org.minkar.${FRAMEWORK}
${FRAMEWORK}: _info_plist
.ORDER: _resource_files _info_plist

_info_plist: .PHONY ${FRAMEWORK_DIR}
.if !empty(INFO_PLIST) && exists(${.CURDIR}/${INFO_PLIST})
	sed -e 's/[$$][({]EXECUTABLE_NAME[)}]/${FRAMEWORK}/g' \
	    -e 's/[$$][({]PRODUCT_NAME[^)}]*[)}]/${FRAMEWORK}/g' \
	    -e 's/[$$][({]PRODUCT_BUNDLE_IDENTIFIER[)}]/${FMWK_BUNDLE_ID}/g' \
	    -e 's/[$$][({]CURRENT_PROJECT_VERSION[)}]/${PROD_VERSION:U1}/g' \
	    -e 's/com[.]yourcompany[.][A-Za-z0-9_.-]*/${FMWK_BUNDLE_ID}/g' \
	    ${.CURDIR}/${INFO_PLIST} > ${RESOURCES_DIR}/Info.plist
.else
	(echo '<?xml version="1.0" encoding="UTF-8"?>'; \
	 echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">'; \
	 echo '<plist version="1.0">'; \
	 echo '<dict>'; \
	 echo '	<key>CFBundleDevelopmentRegion</key>'; \
	 echo '	<string>English</string>'; \
	 echo '	<key>CFBundleExecutable</key>'; \
	 echo '	<string>${FRAMEWORK}</string>'; \
	 echo '	<key>CFBundleIdentifier</key>'; \
	 echo '	<string>${FMWK_BUNDLE_ID}</string>'; \
	 echo '	<key>CFBundleInfoDictionaryVersion</key>'; \
	 echo '	<string>6.0</string>'; \
	 echo '	<key>CFBundleName</key>'; \
	 echo '	<string>${FRAMEWORK}</string>'; \
	 echo '	<key>CFBundlePackageType</key>'; \
	 echo '	<string>FMWK</string>'; \
	 echo '	<key>CFBundleShortVersionString</key>'; \
	 echo '	<string>${PROD_VERSION:U1}</string>'; \
	 echo '	<key>CFBundleVersion</key>'; \
	 echo '	<string>${PROD_VERSION:U1}</string>'; \
	 echo '</dict>'; \
	 echo '</plist>') > ${RESOURCES_DIR}/Info.plist
.endif

all: ${FRAMEWORK} .WAIT fmwk-install-hook

.if !target(fmwk-install-hook)
fmwk-install-hook: .PHONY
.endif

.if defined(MODULEMAP) && !empty(MODULEMAP)
all: copy_modulemap

copy_modulemap: ${.CURDIR}/${MODULEMAP} ${FRAMEWORK_DIR}
	rm -f ${FRAMEWORK_DIR}/Modules
	ln -sf Versions/${FMWK_VERSION}/Modules ${FRAMEWORK_DIR}/Modules
	cp -f ${.CURDIR}/${MODULEMAP} \
	  ${FRAMEWORK_DIR}/Versions/${FMWK_VERSION}/Modules/module.modulemap
.else
all: create_modulemap

# The bundle directory may already exist (made by another rule), and then
# ${FRAMEWORK_DIR}'s recipe does not run: make Modules here. A parallel
# build hit "Directory nonexistent" (CI, CoreGraphics).
create_modulemap: ${FRAMEWORK_DIR}
	mkdir -p ${FRAMEWORK_DIR}/Versions/${FMWK_VERSION}/Modules
	test -e ${FRAMEWORK_DIR}/Modules || \
	    ln -sf Versions/${FMWK_VERSION}/Modules ${FRAMEWORK_DIR}/Modules
	(echo 'framework module ${FRAMEWORK} {'; \
	 echo '    umbrella header "${FRAMEWORK}.h"'; \
	 echo '    export *'; \
	 echo '    module * { export * }'; \
	 echo '}') > ${FRAMEWORK_DIR}/Versions/${FMWK_VERSION}/Modules/module.modulemap
.endif

# The install hook copies the bundle: after its module map is in place.
.ORDER: copy_modulemap fmwk-install-hook
.ORDER: create_modulemap fmwk-install-hook

${FRAMEWORK_DIR}:
	@${ECHO} building ${FRAMEWORK_DIR:T} bundle
	mkdir -p "${FRAMEWORK_DIR}/Versions/${FMWK_VERSION}/Headers" \
		"${FRAMEWORK_DIR}/Versions/${FMWK_VERSION}/PrivateHeaders" \
		"${FRAMEWORK_DIR}/Versions/${FMWK_VERSION}/Modules" \
		"${FRAMEWORK_DIR}/Versions/${FMWK_VERSION}/Resources"
	(cd "${FRAMEWORK_DIR}"; \
		rm -f Headers PrivateHeaders Resources; \
		ln -sf Versions/${FMWK_VERSION}/Headers Headers; \
		ln -sf Versions/${FMWK_VERSION}/PrivateHeaders PrivateHeaders; \
		ln -sf Versions/${FMWK_VERSION}/Modules Modules; \
		ln -sf Versions/${FMWK_VERSION}/Resources Resources)
.if defined(SRCS) && !empty(SRCS)
	(cd ${FRAMEWORK_DIR}; \
		ln -sf Versions/${FMWK_VERSION}/${FRAMEWORK} ${FRAMEWORK})
.endif
	(cd "${FRAMEWORK_DIR}/Versions"; ln -sf ${FMWK_VERSION} Current)

.include <bsd.subdir.mk>
.include <bsd.lib.mk>
.include <bsd.incs.mk>
