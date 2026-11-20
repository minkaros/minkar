/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <CoreImage/CIFilter.h>

@class CIColor, CIVector;

/* A gradient from one color at a point to another at a second point,
   extended without end both ways. */
@interface CILinearGradient : CIFilter
@property (nonatomic, strong) CIVector *inputPoint0;
@property (nonatomic, strong) CIVector *inputPoint1;
@property (nonatomic, strong) CIColor *inputColor0;
@property (nonatomic, strong) CIColor *inputColor1;
@end
