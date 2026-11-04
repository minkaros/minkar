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
SUBDIR += CoreServices/FSEvents
SUBDIR += CoreServices
# IOSurface (roadmap step 4): pixel buffers shared across processes.
SUBDIR += IOSurface
SUBDIR += Onyx2D
# Khronos's Vulkan headers, for the window server's Vulkan compositor
# (GPU track G4); the loader and Mesa come with BSD/lib.
SUBDIR += ../BSD/lib/vulkan-headers
# SkyLight (the window server's client side) comes before CoreGraphics,
# which is to re-export it (WindowServer plan).
SUBDIR += ../PrivateFrameworks/SkyLight
# CoreGraphics links OpenGL.
SUBDIR += OpenGL
SUBDIR += CoreGraphics
SUBDIR += CoreText
# The umbrella of CoreGraphics and CoreText (AppKit's headers import it).
SUBDIR += ApplicationServices
SUBDIR += CoreData
SUBDIR += QuartzCore
SUBDIR += AppKit
# The umbrella apps import and link.
SUBDIR += Cocoa

all: ${SUBDIR}

Foundation: CoreFoundation
IOSurface: CoreFoundation ../Kernel/IOKitUser
CoreServices: CoreServices/LaunchServices CoreServices/FSEvents
CoreServices/FSEvents: CoreFoundation
SystemConfiguration: CoreFoundation
DiskArbitration: CoreFoundation
CoreData: Foundation
QuartzCore: OpenGL IOSurface ../PrivateFrameworks/SkyLight
AppKit: CoreData QuartzCore ApplicationServices
ApplicationServices: CoreGraphics CoreText
Cocoa: AppKit CoreData

.include <bsd.subdir.mk>
