# Loadable bundles (Mach-O MH_BUNDLE): plug-ins such as System Preferences'
# panes (.prefPane) or Menu Extras (.menu).
#
# Builds PROG=${BUNDLE} with bsd.prog.mk, linked -bundle, and lays out
#   ${BUNDLE}.${BUNDLE_EXTENSION}/Contents/Info.plist
#   ${BUNDLE}.${BUNDLE_EXTENSION}/Contents/MacOS/${BUNDLE}
#   ${BUNDLE}.${BUNDLE_EXTENSION}/Contents/Resources/...
# then copies it into ${SYSROOT_DIR}${BUNDLE_INSTALL_DIR}.
#
# Variables:
#   BUNDLE              bundle and executable name (required)
#   BUNDLE_EXTENSION    the bundle's extension (default bundle)
#   BUNDLE_INSTALL_DIR  install location in the sysroot (required)
#   INFO_PLIST          Info.plist source (default Info.plist)
#   FRAMEWORKS          frameworks to link with, by name
#   RESOURCES           files or directories copied into Contents/Resources
#
# The executable is ad-hoc signed like codesign -s -.

BUNDLE_EXTENSION ?= bundle
BUNDLE_DIR = ${.OBJDIR}/${BUNDLE}.${BUNDLE_EXTENSION}
INFO_PLIST ?= Info.plist
PROG = ${BUNDLE}
MAN =
NO_ROOT = yes
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

LDFLAGS += -bundle
CODESIGN ?= python3 -B ${ROOT_SOURCE_DIR}/Developer/Default.xctoolchain/codesign/codesign.py

.for fmwk in ${FRAMEWORKS}
LDFLAGS += -framework ${fmwk}
.endfor

all: ${PROG} .WAIT bundle-layout .WAIT bundle-install-hook

bundle-layout: .PHONY
	@${ECHO} building ${BUNDLE_DIR:T}
	mkdir -p ${BUNDLE_DIR}/Contents/MacOS ${BUNDLE_DIR}/Contents/Resources
	cp -f ${.OBJDIR}/${PROG} ${BUNDLE_DIR}/Contents/MacOS/${BUNDLE}
.if exists(${.CURDIR}/${INFO_PLIST})
	cp -f ${.CURDIR}/${INFO_PLIST} ${BUNDLE_DIR}/Contents/Info.plist
.endif
.for rsc in ${RESOURCES}
	cp -Rf ${rsc} ${BUNDLE_DIR}/Contents/Resources/
.endfor
	${CODESIGN} -f -s - ${BUNDLE_DIR}/Contents/MacOS/${BUNDLE}

bundle-install-hook: .PHONY
	mkdir -p ${SYSROOT_DIR}${BUNDLE_INSTALL_DIR}
	rm -rf ${SYSROOT_DIR}${BUNDLE_INSTALL_DIR}/${BUNDLE_DIR:T}
	cp -Rf ${BUNDLE_DIR} ${SYSROOT_DIR}${BUNDLE_INSTALL_DIR}/

.include <bsd.prog.mk>
