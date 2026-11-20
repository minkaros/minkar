/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <CoreImage/CIImage.h>

@class CIFilter;

/* An image that is a filter's output: a context draws what the filter
   makes of its inputs. */
CIImage *CIImageWithFilter(CIFilter *filter);

/* A box blur's half width for its radius: its box is about the radius
   wide (5 -> 2), as the reference's. */
static inline int CIBoxBlurHalfWidth(double radius) {
    return radius > 0 ? (int)(radius / 2) : 0;
}

@interface CIImage (CIFilterOutput)
- (CIFilter *)filter;
- (void)setFilter:(CIFilter *)filter;
@end
