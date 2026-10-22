#
# FIXME: zoe 2/11/26 This should live in SkyLight now. See
# https://eclecticlight.co/2020/06/08/windowserver-display-compositor-and-input-event-router/
# "In Catalina, it's to be found in /System/Library/PrivateFrameworks/SkyLight.framework/Resources,
# alongside pointer/cursor support."
# Plist goes in /Library/Preferences/com.apple.windowserver.plist
#
APP = WindowServer
SUBDIR =
APP_INSTALL_DIR = /System/Library/CoreServices
ENTITLEMENTS = WindowServer.entitlements
MK_WERROR = no
WARNS = 1

# Display: IOKit IOFramebuffer. Input: IOKit HID (not connected yet);
# keyboard layouts through libxkbcommon and xkeyboard-config.
# The old FreeBSD backends (libinput, libevdev, mtdev) are not built.
SRCS = \
	IOFramebufferDisplay.m \
	WSAppRecord.m \
	WSDisplay.m \
	WSInput.m \
	WSKeymap.c \
	WSWindowRecord.m \
	WindowServer.m \
	main.m

RESOURCES = \
	${.CURDIR}/Icon.png \
	${.CURDIR}/Cursors/arrowCursor.png \
	${.CURDIR}/Cursors/resizeCursor.png \
	${.CURDIR}/Decorations/close.png \
	${.CURDIR}/Decorations/closeHover.png \
	${.CURDIR}/Decorations/mini.png \
	${.CURDIR}/Decorations/miniHover.png \
	${.CURDIR}/Decorations/zoom.png \
	${.CURDIR}/Decorations/zoomUpHover.png \
	${.CURDIR}/Decorations/zoomDownHover.png

FRAMEWORKS = AppKit Foundation CoreFoundation CoreGraphics Onyx2D IOKit

# Some SDK headers are not self-contained yet: <Foundation/Foundation.h>
# pulls in <CFNetwork/CFNetwork.h> and <AppKit/NSDisplay.h> pulls in
# <WindowServer/message.h>, neither of which is installed. Take them from
# the source tree, like the frameworks do.
CFLAGS += -fobjc-arc -g -pthread -D__WSRPC -std=gnu17 \
	-Wno-incompatible-sysroot \
	-I${.CURDIR} -I${ROOT_SOURCE_DIR}/Frameworks -I${ROOT_SOURCE_DIR}/CoreServices \
	-I${.CURDIR}/libxkbcommon/include
# libxkbcommon is built first by ../Makefile (static).
LDFLAGS += -pthread -lobjc -L${.OBJDIR}/libxkbcommon -lxkbcommon

.include <rvn.app.mk>
