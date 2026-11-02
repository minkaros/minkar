# ------------------------------------------------------------------------
#  MINKAR BUILD SYSTEM - CoreServices
# ------------------------------------------------------------------------
#
# WindowServer/Legacy is the current window server (WindowServer plan:
# the new one is built in PrivateFrameworks/SkyLight from WS1; this one
# goes away in WS4b). WindowServer/Demo is its demo client. The keymap
# (WSKeymap and libxkbcommon) lives in PrivateFrameworks/SkyLight/Keymap.
#
# FIXME: Dock, Filer, DMIHelper, SystemUIServer and the login window of
# ravynOS (loginwindow/LoginWindow.m, LoadingWindow) still use the old
# build rules and are not built yet.

SUBDIR = ../PrivateFrameworks/SkyLight/Keymap/libxkbcommon .WAIT WindowServer/Legacy WindowServer/Demo
# The new window server and its test client (SkyLight.framework is built
# in the Frameworks stage). pixman, its compositor's library, is built
# first: the BSD stage comes after this one.
SUBDIR += .WAIT ../BSD/lib/libpixman .WAIT ../PrivateFrameworks/SkyLight/Server \
	../PrivateFrameworks/SkyLight/Tests ../PrivateFrameworks/SkyLight/Tests/eventtap ../PrivateFrameworks/SkyLight/Tests/cgwindow \
	../PrivateFrameworks/SkyLight/Tools/wsctl
# LaunchServices' session daemon (LaunchServices.framework is built in the
# Frameworks stage).
SUBDIR += launchservicesd launchservicesd/tests
# The minimal loginwindow, the session's owner (WindowServer plan, 10.3).
SUBDIR += loginwindow
# The code integrity policy's test program (it needs IOKit).
SUBDIR += ../Kernel/Extensions/MinkarCodeIntegrity/tests
# IOHIDSystem's event queue reader, for the input tests (it needs IOKit).
SUBDIR += ../Kernel/Extensions/IOHIDFamily/tests/hidevents

.include <bsd.subdir.mk>
