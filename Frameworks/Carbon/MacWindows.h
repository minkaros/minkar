/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Window Manager's types and constants, as HIToolbox documents them:
 * window classes and attributes, the parts of a window, control sizes and
 * the menu glyphs' characters. Programs that map them onto AppKit's
 * windows (Tk) use them; Minkar has no Carbon windows.
 */

#ifndef __MACWINDOWS__
#define __MACWINDOWS__

#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef UInt32 WindowClass;
enum {
    kAlertWindowClass = 1,
    kMovableAlertWindowClass = 2,
    kModalWindowClass = 3,
    kMovableModalWindowClass = 4,
    kFloatingWindowClass = 5,
    kDocumentWindowClass = 6,
    kUtilityWindowClass = 8,
    kHelpWindowClass = 10,
    kSheetWindowClass = 11,
    kToolbarWindowClass = 12,
    kPlainWindowClass = 13,
    kOverlayWindowClass = 14,
    kSheetAlertWindowClass = 15,
    kAltPlainWindowClass = 16,
    kSimpleWindowClass = 18,
    kDrawerWindowClass = 20,
    kAllWindowClasses = 0xFFFFFFFF
};

typedef UInt64 WindowAttributes;
enum {
    kWindowNoAttributes = 0,
    kWindowCloseBoxAttribute = (1 << 0),
    kWindowHorizontalZoomAttribute = (1 << 1),
    kWindowVerticalZoomAttribute = (1 << 2),
    kWindowFullZoomAttribute = (kWindowVerticalZoomAttribute | kWindowHorizontalZoomAttribute),
    kWindowCollapseBoxAttribute = (1 << 3),
    kWindowResizableAttribute = (1 << 4),
    kWindowSideTitlebarAttribute = (1 << 5),
    kWindowToolbarButtonAttribute = (1 << 6),
    kWindowUnifiedTitleAndToolbarAttribute = (1 << 7),
    kWindowMetalAttribute = (1 << 8),
    kWindowNoTitleBarAttribute = (1 << 9),
    kWindowTexturedSquareCornersAttribute = (1 << 10),
    kWindowMetalNoContentSeparatorAttribute = (1 << 11),
    kWindowDoesNotCycleAttribute = (1 << 15),
    kWindowNoUpdatesAttribute = (1 << 16),
    kWindowNoActivatesAttribute = (1 << 17),
    kWindowOpaqueForEventsAttribute = (1 << 18),
    kWindowCompositingAttribute = (1 << 19),
    kWindowNoShadowAttribute = (1 << 21),
    kWindowHideOnSuspendAttribute = (1 << 24),
    kWindowStandardHandlerAttribute = (1 << 25),
    kWindowHideOnFullScreenAttribute = (1 << 26),
    kWindowInWindowMenuAttribute = (1 << 27),
    kWindowLiveResizeAttribute = (1 << 28),
    kWindowIgnoreClicksAttribute = (1 << 29),
    kWindowNoConstrainAttribute = (1U << 31),
    kWindowStandardDocumentAttributes = (kWindowCloseBoxAttribute | kWindowFullZoomAttribute |
                                         kWindowCollapseBoxAttribute | kWindowResizableAttribute),
    kWindowStandardFloatingAttributes = (kWindowCloseBoxAttribute | kWindowCollapseBoxAttribute)
};

/* The attributes' bit numbers (from 1): an attribute is 1 << (bit - 1). */
enum {
    kHIWindowBitCloseBox = 1,
    kHIWindowBitZoomBox = 2,
    kHIWindowBitCollapseBox = 4,
    kHIWindowBitResizable = 5,
    kHIWindowBitSideTitlebar = 6,
    kHIWindowBitToolbarButton = 7,
    kHIWindowBitUnifiedTitleAndToolbar = 8,
    kHIWindowBitTextured = 9,
    kHIWindowBitNoTitleBar = 10,
    kHIWindowBitTexturedSquareCorners = 11,
    kHIWindowBitNoTexturedContentSeparator = 12,
    kHIWindowBitRoundBottomBarCorners = 13,
    kHIWindowBitDoesNotCycle = 16,
    kHIWindowBitNoUpdates = 17,
    kHIWindowBitNoActivates = 18,
    kHIWindowBitOpaqueForEvents = 19,
    kHIWindowBitCompositing = 20,
    kHIWindowBitFrameworkScaled = 21,
    kHIWindowBitNoShadow = 22,
    kHIWindowBitCanBeVisibleWithoutLogin = 23,
    kHIWindowBitAsyncDrag = 24,
    kHIWindowBitHideOnSuspend = 25,
    kHIWindowBitStandardHandler = 26,
    kHIWindowBitHideOnFullScreen = 27,
    kHIWindowBitInWindowMenu = 28,
    kHIWindowBitLiveResize = 29,
    kHIWindowBitIgnoreClicks = 30,
    kHIWindowBitNoConstrain = 32,
    kHIWindowBitDoesNotHide = 33,
    kHIWindowBitAutoViewDragTracking = 34
};

typedef SInt16 WindowPartCode;
enum {
    inDesk = 0,
    inNoWindow = 0,
    inMenuBar = 1,
    inSysWindow = 2,
    inContent = 3,
    inDrag = 4,
    inGrow = 5,
    inGoAway = 6,
    inZoomIn = 7,
    inZoomOut = 8,
    inCollapseBox = 11,
    inProxyIcon = 12,
    inToolbarButton = 13,
    inStructure = 15
};

typedef UInt16 ControlSize;
enum {
    kControlSizeNormal = 0,
    kControlSizeSmall = 1,
    kControlSizeLarge = 2,
    kControlSizeMini = 3,
    kControlSizeAuto = 0xFFFF
};

/* The characters of the menu glyphs. */
enum {
    kCommandUnicode = 0x2318,
    kCheckUnicode = 0x2713,
    kDiamondUnicode = 0x25C6,
    kBulletUnicode = 0x2022,
    kAppleLogoUnicode = 0xF8FF,
    kOptionUnicode = 0x2325,
    kControlUnicode = 0x2303,
    kShiftUnicode = 0x21E7
};

#ifdef __cplusplus
}
#endif

#endif
