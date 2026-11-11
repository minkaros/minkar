/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Quick Look for applications: a document's thumbnail as an image.
 */

#ifndef __QLBASE__
#define __QLBASE__

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* A thumbnail of url no bigger than maxThumbnailSize, from the generator
   of its type; NULL without one. */
CF_EXPORT CGImageRef QLThumbnailImageCreate(CFAllocatorRef allocator, CFURLRef url, CGSize maxThumbnailSize,
                                            CFDictionaryRef options);

#if defined(__cplusplus)
}
#endif

#endif /* __QLBASE__ */
