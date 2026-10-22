/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGWindowLevel.h>
#import <CoreGraphics/CGGeometry.h>
#import <CoreFoundation/CFArray.h>
#import <CoreFoundation/CFDictionary.h>

#ifdef __OBJC__
@class O2Context, CGEvent;
#endif

typedef enum {
    CGSBackingStoreRetained = 0,
    CGSBackingStoreNonretained = 1,
    CGSBackingStoreBuffered = 2
} CGSBackingStoreType;

typedef uint32_t CGWindowID;

/*
 * Window list API, source compatible with macOS (written from the public
 * Quartz Window Services documentation).
 */
#define kCGNullWindowID ((CGWindowID)0)

typedef uint32_t CGWindowListOption;
enum {
    kCGWindowListOptionAll                 = 0,
    kCGWindowListOptionOnScreenOnly        = (1 << 0),
    kCGWindowListOptionOnScreenAboveWindow = (1 << 1),
    kCGWindowListOptionOnScreenBelowWindow = (1 << 2),
    kCGWindowListOptionIncludingWindow     = (1 << 3),
    kCGWindowListExcludeDesktopElements    = (1 << 4),
};

typedef uint32_t CGWindowSharingType;
enum {
    kCGWindowSharingNone      = 0,
    kCGWindowSharingReadOnly  = 1,
    kCGWindowSharingReadWrite = 2,
};

typedef uint32_t CGWindowBackingType;
enum {
    kCGBackingStoreRetained    = 0,
    kCGBackingStoreNonretained = 1,
    kCGBackingStoreBuffered    = 2,
};

/* Keys of the dictionaries returned by CGWindowListCopyWindowInfo. */
COREGRAPHICS_EXPORT const CFStringRef kCGWindowNumber;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowStoreType;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowLayer;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowBounds;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowSharingState;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowAlpha;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowOwnerPID;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowMemoryUsage;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowOwnerName;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowName;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowIsOnscreen;
COREGRAPHICS_EXPORT const CFStringRef kCGWindowBackingLocationVideoMemory;

/* Window IDs (stored directly as the array values), front-most first. */
COREGRAPHICS_EXPORT CFArrayRef CGWindowListCreate(CGWindowListOption option,
    CGWindowID relativeToWindow);

/* One CFDictionary per window, front-most first. */
COREGRAPHICS_EXPORT CFArrayRef CGWindowListCopyWindowInfo(CGWindowListOption option,
    CGWindowID relativeToWindow);

/* Descriptions for the windows in an array from CGWindowListCreate. */
COREGRAPHICS_EXPORT CFArrayRef CGWindowListCreateDescriptionFromArray(CFArrayRef windowArray);

CGRect CGInsetRectForNativeWindowBorder(CGRect frame, unsigned styleMask);
CGRect CGOutsetRectForNativeWindowBorder(CGRect frame, unsigned styleMask);
