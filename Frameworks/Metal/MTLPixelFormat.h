/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Metal's pixel formats (the values of the Cocoa API).
 */

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSUInteger, MTLPixelFormat) {
    MTLPixelFormatInvalid = 0,
    MTLPixelFormatA8Unorm = 1,
    MTLPixelFormatR8Unorm = 10,
    MTLPixelFormatR16Unorm = 20,
    MTLPixelFormatRG8Unorm = 30,
    MTLPixelFormatRGBA8Unorm = 70,
    MTLPixelFormatRGBA8Unorm_sRGB = 71,
    MTLPixelFormatBGRA8Unorm = 80,
    MTLPixelFormatBGRA8Unorm_sRGB = 81,
    MTLPixelFormatRGB10A2Unorm = 90,
    MTLPixelFormatBGR10A2Unorm = 94,
    MTLPixelFormatRGBA16Float = 115,
    MTLPixelFormatRGBA32Float = 125,
    MTLPixelFormatDepth32Float = 252,
    MTLPixelFormatStencil8 = 253,
    MTLPixelFormatDepth24Unorm_Stencil8 = 255,
    MTLPixelFormatDepth32Float_Stencil8 = 260,
};
