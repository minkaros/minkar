/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <CoreImage/CIFilter.h>

@class CIImage;

/* A box blur of an image: each pixel the mean of the square around it. */
@interface CIBoxBlur : CIFilter
@property (nonatomic, strong) CIImage *inputImage;
@property (nonatomic, copy) NSNumber *inputRadius;
@end
