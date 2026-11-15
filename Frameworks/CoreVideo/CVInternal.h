/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CoreVideo's own: a pixel format's layout, from its description (the
 * built-in ones and the ones an app registers), for CVPixelBuffer.
 */

#ifndef __CVINTERNAL__
#define __CVINTERNAL__

#include <CoreFoundation/CoreFoundation.h>

#define CV_MAX_PLANES 3

typedef struct {
    int bitsPerBlock, blockWidth, blockHeight;
    int horizontalSubsampling, verticalSubsampling;
} CVFormatPlane;

typedef struct {
    int planes;                 /* 1: not planar */
    CVFormatPlane plane[CV_MAX_PLANES];
} CVFormatLayout;

/* False for a format nothing describes. */
__attribute__((visibility("hidden"))) Boolean _CVPixelFormatGetLayout(OSType format, CVFormatLayout *layout);

#endif
