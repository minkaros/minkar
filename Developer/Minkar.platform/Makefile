# ------------------------------------------------------------------------
#  RAVYNOS BUILD SYSTEM - Developer Platform
# ------------------------------------------------------------------------

PLATFORM_DIR = ${ROOT_BINARY_DIR}/Developer/Platforms/Minkar.platform

all: ${PLATFORM_DIR} Info.plist toolchains-link

# xcrun (Libsystem's linker_arguments.sh uses it) finds the toolchains in
# $DEVELOPER_DIR/Toolchains, as in Xcode.
toolchains-link: .PHONY ${PLATFORM_DIR}
	ln -sfn Platforms/Minkar.platform/Developer/Toolchains \
	  ${ROOT_BINARY_DIR}/Developer/Toolchains

${PLATFORM_DIR}:
	mkdir -p ${PLATFORM_DIR}

Info.plist: ${PLATFORM_DIR} ${.CURDIR}/Info.plist
	cp -f ${.CURDIR}/Info.plist ${PLATFORM_DIR}/
