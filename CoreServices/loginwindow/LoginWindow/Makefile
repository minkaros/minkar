# The login window (Atlas.md, M5): the app loginwindow runs when nobody is
# logged in, inside loginwindow.app.
SUBDIR =
APP =		LoginWindow
APP_INSTALL_DIR = /System/Library/CoreServices/loginwindow.app/Contents/Resources
SRCS =		main.m
FRAMEWORKS =	AppKit Foundation OpenDirectory IOKit
MK_WERROR =	no
CFLAGS +=	-fobjc-arc -g -std=gnu17 -Wno-incompatible-sysroot -Wno-nullability-completeness
LDFLAGS +=	-lobjc
RESOURCES =	${.CURDIR}/es.lproj

.include <minkar.app.mk>
