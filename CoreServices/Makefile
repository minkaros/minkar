# ------------------------------------------------------------------------
#  MINKAR BUILD SYSTEM - CoreServices
# ------------------------------------------------------------------------
#
# WindowServer/Legacy is the current window server (WindowServer plan:
# the new one is built in PrivateFrameworks/SkyLight from WS1; this one
# goes away in WS4). WindowServer/Demo is its demo client.
#
# FIXME: Dock, Filer, DMIHelper, SystemUIServer and loginwindow (with
# LoadingWindow) still use the old build rules and are not built yet.

SUBDIR = WindowServer/Legacy/libxkbcommon .WAIT WindowServer/Legacy WindowServer/Demo
# The new window server and its test client (SkyLight.framework is built
# in the Frameworks stage). pixman, its compositor's library, is built
# first: the BSD stage comes after this one.
SUBDIR += .WAIT ../BSD/lib/libpixman .WAIT ../PrivateFrameworks/SkyLight/Server \
	../PrivateFrameworks/SkyLight/Tests ../PrivateFrameworks/SkyLight/Tests/eventtap \
	../PrivateFrameworks/SkyLight/Tools/wsctl

.include <bsd.subdir.mk>
