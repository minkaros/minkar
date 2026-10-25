# Application bundles.
#
# Builds PROG=${APP} with bsd.prog.mk and lays out
#   ${APP}.app/Contents/Info.plist
#   ${APP}.app/Contents/${APP_EXEC_DIR}/${APP}
#   ${APP}.app/Contents/Resources/...
# then copies the bundle into ${SYSROOT_DIR}${APP_INSTALL_DIR}.
#
# Variables:
#   APP                 bundle and executable name (required)
#   APP_INSTALL_DIR     install location in the sysroot (default /Applications)
#   APP_EXEC_DIR        executable directory (default MacOS, as on macOS)
#   INFO_PLIST          Info.plist source (default Info.plist, if present)
#   FRAMEWORKS          frameworks to link with, by name
#   RESOURCES           files or directories copied into Contents/Resources
#   ENTITLEMENTS        XML property list of entitlements for the signature
#
# The executable is ad-hoc signed like codesign -s -: the identifier is
# CFBundleIdentifier and the signature seals Info.plist.

APP_DIR = ${.OBJDIR}/${APP}.app
APP_EXEC_DIR ?= MacOS
APP_INSTALL_DIR ?= /Applications
INFO_PLIST ?= Info.plist
PROG = ${APP}
MAN =
NO_ROOT = yes
# Mach-O executables: no .pieo objects (no Objective-C rule for them) and
# no ELF-style split debug files.
MK_PIE = no
MK_DEBUG_FILES = no

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
.endif

# Run from the source tree so the toolchain need not be rebuilt first.
CODESIGN ?= python3 -B ${ROOT_SOURCE_DIR}/Developer/Default.xctoolchain/codesign/codesign.py
.if defined(ENTITLEMENTS) && !empty(ENTITLEMENTS)
_CODESIGN_FLAGS = --entitlements ${.CURDIR}/${ENTITLEMENTS}
.endif

.for fmwk in ${FRAMEWORKS}
LDFLAGS += -framework ${fmwk}
.endfor

all: ${PROG} .WAIT app-bundle .WAIT app-install-hook

app-bundle: .PHONY
	@${ECHO} building ${APP_DIR:T} bundle
	mkdir -p ${APP_DIR}/Contents/${APP_EXEC_DIR} ${APP_DIR}/Contents/Resources
	cp -f ${.OBJDIR}/${PROG} ${APP_DIR}/Contents/${APP_EXEC_DIR}/${APP}
.if exists(${.CURDIR}/${INFO_PLIST})
	cp -f ${.CURDIR}/${INFO_PLIST} ${APP_DIR}/Contents/Info.plist
.endif
.for rsc in ${RESOURCES}
	cp -Rf ${rsc} ${APP_DIR}/Contents/Resources/
.endfor
	${CODESIGN} -f -s - ${_CODESIGN_FLAGS} \
	    ${APP_DIR}/Contents/${APP_EXEC_DIR}/${APP}

app-install-hook: .PHONY
	mkdir -p ${SYSROOT_DIR}${APP_INSTALL_DIR}
	rm -rf ${SYSROOT_DIR}${APP_INSTALL_DIR}/${APP}.app
	cp -Rf ${APP_DIR} ${SYSROOT_DIR}${APP_INSTALL_DIR}/

.include <bsd.prog.mk>
