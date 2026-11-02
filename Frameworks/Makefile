# ------------------------------------------------------------------------
#  MINKAR BUILD SYSTEM - Frameworks
# ------------------------------------------------------------------------

SUBDIR = CoreFoundation
# IOKit.framework: it links CoreFoundation, and SystemConfiguration links it.
SUBDIR += ../Kernel/IOKitUser
#SUBDIR += CFNetwork
SUBDIR += SystemConfiguration
# DiskArbitration links IOKit; diskarbitrationd (BSD) links SystemConfiguration.
SUBDIR += DiskArbitration
SUBDIR += Foundation
# LaunchServices is inside CoreServices, which re-exports it.
SUBDIR += CoreServices/LaunchServices
SUBDIR += CoreServices
# IOSurface (roadmap step 4): pixel buffers shared across processes.
SUBDIR += IOSurface
SUBDIR += Onyx2D
# SkyLight (the window server's client side) comes before CoreGraphics,
# which is to re-export it (WindowServer plan).
SUBDIR += ../PrivateFrameworks/SkyLight
# CoreGraphics links OpenGL.
SUBDIR += OpenGL
SUBDIR += CoreGraphics
SUBDIR += CoreText
SUBDIR += CoreData
SUBDIR += QuartzCore
SUBDIR += AppKit

all: ${SUBDIR}

Foundation: CoreFoundation
IOSurface: CoreFoundation
CoreServices: CoreServices/LaunchServices
SystemConfiguration: CoreFoundation
DiskArbitration: CoreFoundation
CoreData: Foundation
QuartzCore: OpenGL
AppKit: CoreData QuartzCore

.include <bsd.subdir.mk>
