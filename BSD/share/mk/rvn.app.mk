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
#   APP_EXEC_DIR        executable directory (default ravynOS; Foundation
#                       and CoreFoundation also accept MacOS)
#   INFO_PLIST          Info.plist source (default Info.plist, if present)
#   FRAMEWORKS          frameworks to link with, by name
#   RESOURCES           files or directories copied into Contents/Resources

APP_DIR = ${.OBJDIR}/${APP}.app
APP_EXEC_DIR ?= ravynOS
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

.if defined(RAVYN_SDKROOT)
CFLAGS += --sysroot=${RAVYN_SDKROOT}
LDFLAGS += --sysroot=${RAVYN_SDKROOT}
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

app-install-hook: .PHONY
	mkdir -p ${SYSROOT_DIR}${APP_INSTALL_DIR}
	rm -rf ${SYSROOT_DIR}${APP_INSTALL_DIR}/${APP}.app
	cp -Rf ${APP_DIR} ${SYSROOT_DIR}${APP_INSTALL_DIR}/

.include <bsd.prog.mk>
