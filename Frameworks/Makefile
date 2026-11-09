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
# libcompression (from BSD/lib) and the liblzma it links (CoreText's xz):
# Foundation's NSData includes <compression.h> and links it.
SUBDIR += CoreText/xz
SUBDIR += ../BSD/lib/libcompression
# CoreServices and the frameworks inside it (LaunchServices, FSEvents,
# AE), which it re-exports: C on CoreFoundation, before Foundation, whose
# Apple event classes are built on AE.
SUBDIR += CoreServices/AE
SUBDIR += CoreServices/LaunchServices
SUBDIR += CoreServices/FSEvents
SUBDIR += CoreServices
SUBDIR += Foundation
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
SUBDIR += ImageIO
SUBDIR += CoreData
# CoreVideo (CVDisplayLink, CVTimeStamp): QuartzCore's headers import it.
SUBDIR += CoreVideo
SUBDIR += QuartzCore
SUBDIR += AppKit
# Small frameworks binaries for the reference platform link.
SUBDIR += UserNotifications
SUBDIR += ServiceManagement
SUBDIR += AddressBook
SUBDIR += AudioToolbox
SUBDIR += OSAKit
SUBDIR += Carbon
SUBDIR += Quartz
# The umbrella apps import and link.
SUBDIR += Cocoa
# System Preferences' panes.
SUBDIR += PreferencePanes
# Swift's runtime (/usr/lib/swift) and its test program, from Libraries/swift:
# built here because its Objective-C parts include Foundation's headers
# (and through NSGeometry.h, CoreGraphics').
SUBDIR += ../Libraries/swift
SUBDIR += ../Libraries/swift/tests
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

# What each one links (or includes) is built before it: the stage runs
# them in parallel.
Foundation: CoreFoundation ../BSD/lib/libcompression CoreServices
../BSD/lib/libcompression: CoreText/xz
CoreText: CoreText/xz CoreGraphics Foundation Onyx2D
IOSurface: CoreFoundation ../Kernel/IOKitUser
../Kernel/IOKitUser: CoreFoundation
CoreServices/LaunchServices: CoreFoundation
Onyx2D: CoreFoundation Foundation
CoreGraphics: CoreFoundation IOSurface Onyx2D OpenGL ../PrivateFrameworks/SkyLight
CoreServices: CoreServices/LaunchServices CoreServices/FSEvents CoreServices/AE
CoreServices/AE: CoreFoundation
CoreServices/FSEvents: CoreFoundation
SystemConfiguration: CoreFoundation ../Kernel/IOKitUser
DiskArbitration: CoreFoundation ../Kernel/IOKitUser
CoreData: Foundation
QuartzCore: OpenGL IOSurface ../PrivateFrameworks/SkyLight CoreVideo CoreGraphics Onyx2D
CoreVideo: OpenGL Foundation
AppKit: CoreData QuartzCore ApplicationServices CoreServices
UserNotifications: Foundation
ServiceManagement: CoreFoundation
AddressBook: Foundation
AudioToolbox: CoreFoundation
OSAKit: Foundation
Carbon: CoreServices ApplicationServices
Quartz: AppKit
ApplicationServices: CoreGraphics CoreText
ImageIO: CoreGraphics
Cocoa: AppKit CoreData
PreferencePanes: AppKit
../Libraries/swift: CoreFoundation CoreGraphics Foundation
../Libraries/swift/tests: ../Libraries/swift
Combine: ../Libraries/swift
../Libraries/swift/overlays: ../Libraries/swift CoreFoundation CoreGraphics Foundation Combine
XCTest: ../Libraries/swift/overlays

.include <bsd.subdir.mk>
