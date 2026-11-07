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
# CoreVideo (CVDisplayLink, CVTimeStamp): QuartzCore's headers import it.
SUBDIR += CoreVideo
SUBDIR += QuartzCore
SUBDIR += AppKit
# The umbrella apps import and link.
SUBDIR += Cocoa
# System Preferences' panes.
SUBDIR += PreferencePanes
# Combine, from OpenCombine (Swift; Dispatch's and Foundation's overlays
# extend it).
SUBDIR += Combine
# Swift's overlays for ObjectiveC, Dispatch, CoreFoundation, CoreGraphics and
# Foundation (in Libraries/swift: built here, after the frameworks they
# extend).
SUBDIR += ../Libraries/swift/overlays
# XCTest, for test programs (Swift; on the Foundation overlay).
SUBDIR += XCTest

all: ${SUBDIR}

Foundation: CoreFoundation
IOSurface: CoreFoundation ../Kernel/IOKitUser
CoreServices: CoreServices/LaunchServices CoreServices/FSEvents
CoreServices/FSEvents: CoreFoundation
SystemConfiguration: CoreFoundation
DiskArbitration: CoreFoundation
CoreData: Foundation
QuartzCore: OpenGL IOSurface ../PrivateFrameworks/SkyLight CoreVideo
CoreVideo: OpenGL Foundation
AppKit: CoreData QuartzCore ApplicationServices
ApplicationServices: CoreGraphics CoreText
Cocoa: AppKit CoreData
PreferencePanes: AppKit
../Libraries/swift/overlays: CoreFoundation CoreGraphics Foundation Combine
XCTest: ../Libraries/swift/overlays

.include <bsd.subdir.mk>
