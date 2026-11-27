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
# OpenDirectory (APSL) and its CFOpenDirectory inside it, after
# CoreGraphics (Foundation's NSGeometry.h includes its headers): user records and
# passwords through opendirectoryd, for loginwindow and the Users pane.
SUBDIR += OpenDirectory/Core
SUBDIR += OpenDirectory
# Security: Minkar's own subset for now (the APSL sources beside it are
# to replace it). On CoreFoundation only; before CUPS, whose configure
# links its checks with it.
SUBDIR += Security/minkar
# SystemConfiguration's privileged helper, for SCPreferences opened with an
# authorization: it links Security, so after it.
SUBDIR += SystemConfiguration/SCHelper
# PrintCore and AppKit link libcups and include its headers; CUPS (with the
# OpenSSL it uses for TLS) is built here, as the BSD stage comes later.
# The stage builds these directories in the order listed here.
SUBDIR += ../BSD/usr.bin/openssl
SUBDIR += ../BSD/usr.bin/cups
SUBDIR += ApplicationServices/PrintCore
# The umbrella of CoreGraphics and CoreText (AppKit's headers import it).
SUBDIR += ApplicationServices
SUBDIR += ImageIO
# CoreData links libsqlite3 (BSD/lib, a stage later): built here first.
SUBDIR += ../BSD/lib/libsqlite3
SUBDIR += CoreData
# CoreVideo (CVDisplayLink, CVTimeStamp): QuartzCore's headers import it.
SUBDIR += CoreVideo
# Quick Look's generators (Quartz re-exports it).
SUBDIR += QuickLook
# Core Image (QuartzCore re-exports it).
SUBDIR += CoreImage
SUBDIR += QuartzCore
# AudioToolbox and VideoToolbox include FFmpeg's headers (they load its
# libraries at run time): FFmpeg, with the dav1d it decodes AV1 with, is
# built here, as the BSD stage comes later.
SUBDIR += ../BSD/lib/dav1d
SUBDIR += ../BSD/lib/ffmpeg
# AppKit links AudioToolbox (its sounds), which links CoreAudio: both
# before it.
SUBDIR += CoreAudio
SUBDIR += AudioToolbox
SUBDIR += AppKit
# Small frameworks binaries for the reference platform link.
SUBDIR += UserNotifications
SUBDIR += ServiceManagement
SUBDIR += AddressBook
SUBDIR += OSAKit
# pixman (BSD/lib): HIShape's regions; the window server links it too
# (CoreServices stage, after this one).
SUBDIR += ../BSD/lib/libpixman
SUBDIR += Carbon
# PDF documents (Quartz re-exports it).
SUBDIR += PDFKit
SUBDIR += Quartz
# Frameworks Firefox links (13 Nov 2026): what it needs to load, each with
# what Minkar can do today (see each Makefile).
SUBDIR += AudioUnit
SUBDIR += CoreMedia
SUBDIR += VideoToolbox
SUBDIR += CoreMIDI
SUBDIR += Accelerate
SUBDIR += Metal
SUBDIR += MetalKit
SUBDIR += LocalAuthentication
SUBDIR += CoreLocation
SUBDIR += Vision
SUBDIR += IOBluetooth
SUBDIR += MediaPlayer
SUBDIR += AuthenticationServices
SUBDIR += Accessibility
SUBDIR += AVFoundation
SUBDIR += AVKit
# Screen and window capture (Firefox's screen sharing, 27 Nov 2026).
SUBDIR += ScreenCaptureKit
# VLC's and UTM's interfaces link it (21 Nov 2026).
SUBDIR += ScriptingBridge
# WebKit.framework is a stage of its own, after BSD (WPE WebKit): the
# top Makefile.
# Uniform types as objects (API 20).
SUBDIR += UniformTypeIdentifiers
# The umbrella apps import and link.
SUBDIR += Cocoa
# System Preferences' panes; the lock their settings sit behind.
SUBDIR += PreferencePanes SecurityInterface
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

# What each one links (or includes). The stage builds the directories in
# the order SUBDIR lists them (bsd.subdir.mk reads dependencies only with
# SUBDIR_PARALLEL, which this Makefile does not set): these rules do not
# order the build, they say what the order must keep.
# tools/check/subdir-order.py checks that SUBDIR keeps them.
Foundation: CoreFoundation ../BSD/lib/libcompression CoreServices
../BSD/lib/libcompression: CoreText/xz
CoreText: CoreText/xz CoreGraphics Foundation Onyx2D
IOSurface: CoreFoundation ../Kernel/IOKitUser
../Kernel/IOKitUser: CoreFoundation
CFNetwork/minkar: CoreFoundation
CoreServices/LaunchServices: CoreFoundation
Onyx2D: CoreFoundation Foundation
CoreGraphics: CoreFoundation IOSurface Onyx2D OpenGL ../PrivateFrameworks/SkyLight
OpenGL: CoreFoundation IOSurface
CoreServices: CoreServices/LaunchServices CoreServices/FSEvents CoreServices/AE CFNetwork/minkar
CoreServices/AE: CoreFoundation
CoreServices/FSEvents: CoreFoundation
OpenDirectory/Core: CoreFoundation SystemConfiguration
OpenDirectory: OpenDirectory/Core Foundation CoreGraphics
SystemConfiguration: CoreFoundation ../Kernel/IOKitUser
DiskArbitration: CoreFoundation ../Kernel/IOKitUser
CoreData: Foundation ../BSD/lib/libsqlite3
CoreImage: CoreGraphics
QuartzCore: OpenGL IOSurface ../PrivateFrameworks/SkyLight CoreVideo CoreGraphics Onyx2D CoreImage
CoreVideo: OpenGL Foundation CoreGraphics IOSurface
AppKit: CoreData QuartzCore ApplicationServices CoreServices AudioToolbox
UserNotifications: Foundation
ServiceManagement: CoreFoundation Security/minkar
Security/minkar: CoreFoundation
AddressBook: Foundation
AudioToolbox: CoreFoundation CoreAudio ../BSD/lib/ffmpeg
OSAKit: Foundation
Carbon: CoreServices ApplicationServices ../BSD/lib/libpixman
PDFKit: AppKit CoreGraphics Onyx2D
Quartz: AppKit QuickLook PDFKit
CoreAudio: CoreFoundation ../Kernel/IOKitUser
AudioUnit: AudioToolbox
CoreMedia: CoreFoundation CoreVideo CoreAudio AudioToolbox
VideoToolbox: CoreMedia CoreVideo ../BSD/lib/ffmpeg
CoreMIDI: CoreFoundation
Accelerate: CoreFoundation
Metal: Foundation
MetalKit: Metal QuartzCore AppKit
LocalAuthentication: Foundation
CoreLocation: Foundation
Vision: Foundation CoreGraphics
IOBluetooth: Foundation
MediaPlayer: Foundation CoreGraphics
ScriptingBridge: Foundation
AuthenticationServices: Foundation
Accessibility: Foundation
AVFoundation: CoreMedia CoreVideo QuartzCore IOSurface CoreServices CoreGraphics AudioToolbox VideoToolbox
AVKit: AppKit AVFoundation
ScreenCaptureKit: AppKit CoreGraphics CoreMedia CoreVideo IOSurface
UniformTypeIdentifiers: Foundation CoreServices
# CUPS's configure links its checks with Security, SystemConfiguration,
# CoreFoundation, libiconv and libz; without one, the pthread check fails.
../BSD/usr.bin/cups: ../BSD/usr.bin/openssl Security/minkar SystemConfiguration CoreFoundation
ApplicationServices/PrintCore: CoreGraphics CoreServices ../BSD/usr.bin/cups
ApplicationServices: CoreGraphics CoreText ApplicationServices/PrintCore
ImageIO: CoreGraphics
Cocoa: AppKit CoreData
PreferencePanes: AppKit
SecurityInterface: AppKit Security/minkar
SystemConfiguration/SCHelper: SystemConfiguration Security/minkar
ExceptionHandling: Foundation
QuickLook: CoreGraphics CoreFoundation
../Libraries/swift: CoreFoundation CoreGraphics Foundation
../Libraries/swift/tests: ../Libraries/swift
Combine: ../Libraries/swift
../Libraries/swift/overlays: ../Libraries/swift CoreFoundation CoreGraphics Foundation Combine
XCTest: ../Libraries/swift/overlays
../BSD/lib/ffmpeg: ../BSD/lib/dav1d

.include <bsd.subdir.mk>
