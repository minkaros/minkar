/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <CoreGraphics/CoreGraphicsExport.h>
#import <CoreGraphics/CGGeometry.h>
#import <CoreGraphics/CGColorSpace.h>
#import <CoreFoundation/CFArray.h>

/* CF's naming rules for what the functions return (Create, Copy). */
CF_IMPLICIT_BRIDGING_ENABLED

typedef struct CGGradient *CGGradientRef;

typedef CF_OPTIONS(uint32_t, CGGradientDrawingOptions) {
    kCGGradientDrawsBeforeStartLocation = (1 << 0),
    kCGGradientDrawsAfterEndLocation = (1 << 1)
};

COREGRAPHICS_EXPORT CFTypeID CGGradientGetTypeID(void);
COREGRAPHICS_EXPORT CGGradientRef CGGradientCreateWithColorComponents(CGColorSpaceRef space, const CGFloat *components, const CGFloat *locations, size_t count);
COREGRAPHICS_EXPORT CGGradientRef CGGradientCreateWithColors(CGColorSpaceRef space, CFArrayRef colors, const CGFloat *locations);
COREGRAPHICS_EXPORT CGGradientRef CGGradientRetain(CGGradientRef gradient);
COREGRAPHICS_EXPORT void CGGradientRelease(CGGradientRef gradient);

CF_IMPLICIT_BRIDGING_DISABLED
