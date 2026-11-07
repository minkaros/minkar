# ------------------------------------------------------------------------
#  MINKAR BUILD SYSTEM - CoreServices
# ------------------------------------------------------------------------
#
# The window server is built in PrivateFrameworks/SkyLight (WindowServer
# plan); WindowServer/Demo is its demo client. The keymap (WSKeymap and
# libxkbcommon) lives in PrivateFrameworks/SkyLight/Keymap.
#
# FIXME: DMIHelper and the login window of ravynOS (loginwindow/LoginWindow.m,
# LoadingWindow) still use the old build rules and are not built yet.

SUBDIR = ../PrivateFrameworks/SkyLight/Keymap/libxkbcommon .WAIT WindowServer/Demo
# The window server and its test client (SkyLight.framework is built
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
# The IOSurface registry (roadmap step 4).
SUBDIR += iosurfaced ../Frameworks/IOSurface/tests
# The pasteboard server (Phase 4).
SUBDIR += pboard pboard/tests
# os_log's messages kept (Phase 3).
SUBDIR += logd
# The file system's events for FSEvents (Atlas.md, K0).
SUBDIR += fseventsd
# The desktop session apps loginwindow starts (Phase 5): the Dock and
# the desktop (Atlas.md, D1), Files, the file manager (K1), and the Menu Extras
# (Atlas.md, MB1).
SUBDIR += Dock Files SystemUIServer
# Text Editor (opens plain text through LaunchServices).
SUBDIR += TextEditor
# System Preferences and its panes (Phase 5, item 6).
SUBDIR += SystemPreferences SystemPreferences/Panes/Displays SystemPreferences/Panes/Dock \
	SystemPreferences/Panes/DateTime SystemPreferences/Panes/Keyboard SystemPreferences/Panes/Network
# The code integrity policy's test program (it needs IOKit).
SUBDIR += ../Kernel/Extensions/MinkarCodeIntegrity/tests
# IOHIDSystem's event queue reader, for the input tests (it needs IOKit).
SUBDIR += ../Kernel/Extensions/IOHIDFamily/tests/hidevents
# The AMD GPU driver's user client from a program (G2; it needs IOKit).
SUBDIR += ../Kernel/Extensions/MinkarDRM/tests/gputest

.include <bsd.subdir.mk>
