# ------------------------------------------------------------------------
#  MINKAR BUILD SYSTEM - Frameworks
# ------------------------------------------------------------------------

SUBDIR = CoreFoundation
# IOKit.framework: it links CoreFoundation, and SystemConfiguration links it.
SUBDIR += ../Kernel/IOKitUser
# CFNetwork: Minkar's subset (the proxy API); the APSL sources wait.
SUBDIR += CFNetwork/minkar
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
# Quick Look's generators (Quartz re-exports it).
SUBDIR += QuickLook
SUBDIR += QuartzCore
SUBDIR += AppKit
# Security: Minkar's own subset for now (the APSL sources beside it are
# to replace it).
SUBDIR += Security/minkar
# Small frameworks binaries for the reference platform link.
SUBDIR += UserNotifications
SUBDIR += ServiceManagement
SUBDIR += AddressBook
SUBDIR += AudioToolbox
SUBDIR += OSAKit
SUBDIR += Carbon
SUBDIR += Quartz
# Frameworks Firefox links (13 Nov 2026): what it needs to load, each with
# what Minkar can do today (see each Makefile).
SUBDIR += CoreAudio
SUBDIR += CoreMedia
SUBDIR += VideoToolbox
SUBDIR += CoreMIDI
SUBDIR += Metal
SUBDIR += LocalAuthentication
SUBDIR += CoreLocation
SUBDIR += Vision
SUBDIR += IOBluetooth
SUBDIR += MediaPlayer
SUBDIR += AuthenticationServices
# WebKit.framework is a stage of its own, after BSD (WPE WebKit): the
# top Makefile.
# Uniform types as objects (API 20).
SUBDIR += UniformTypeIdentifiers
# The umbrella apps import and link.
SUBDIR += Cocoa
# System Preferences' panes.
SUBDIR += PreferencePanes
# How a program logs and handles exceptions; Quick Look's generators.
SUBDIR += ExceptionHandling
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
CFNetwork/minkar: CoreFoundation
CoreServices/LaunchServices: CoreFoundation
Onyx2D: CoreFoundation Foundation
CoreGraphics: CoreFoundation IOSurface Onyx2D OpenGL ../PrivateFrameworks/SkyLight
CoreServices: CoreServices/LaunchServices CoreServices/FSEvents CoreServices/AE CFNetwork/minkar
CoreServices/AE: CoreFoundation
CoreServices/FSEvents: CoreFoundation
SystemConfiguration: CoreFoundation ../Kernel/IOKitUser
DiskArbitration: CoreFoundation ../Kernel/IOKitUser
CoreData: Foundation
QuartzCore: OpenGL IOSurface ../PrivateFrameworks/SkyLight CoreVideo CoreGraphics Onyx2D
CoreVideo: OpenGL Foundation
AppKit: CoreData QuartzCore ApplicationServices CoreServices
UserNotifications: Foundation
ServiceManagement: CoreFoundation Security/minkar
Security/minkar: CoreFoundation
AddressBook: Foundation
AudioToolbox: CoreFoundation
OSAKit: Foundation
Carbon: CoreServices ApplicationServices
Quartz: AppKit QuickLook
CoreAudio: CoreFoundation
CoreMedia: CoreFoundation
VideoToolbox: CoreMedia CoreVideo
CoreMIDI: CoreFoundation
Metal: Foundation
LocalAuthentication: Foundation
CoreLocation: Foundation
Vision: Foundation CoreGraphics
IOBluetooth: Foundation
MediaPlayer: Foundation CoreGraphics
AuthenticationServices: Foundation
UniformTypeIdentifiers: Foundation CoreServices
ApplicationServices: CoreGraphics CoreText
ImageIO: CoreGraphics
Cocoa: AppKit CoreData
PreferencePanes: AppKit
ExceptionHandling: Foundation
QuickLook: CoreGraphics CoreFoundation
../Libraries/swift: CoreFoundation CoreGraphics Foundation
../Libraries/swift/tests: ../Libraries/swift
Combine: ../Libraries/swift
../Libraries/swift/overlays: ../Libraries/swift CoreFoundation CoreGraphics Foundation Combine
XCTest: ../Libraries/swift/overlays

.include <bsd.subdir.mk>
