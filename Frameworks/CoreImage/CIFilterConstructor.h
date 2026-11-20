/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>

@class CIFilter;

NS_ASSUME_NONNULL_BEGIN

/* What +[CIFilter registerFilterName:constructor:classAttributes:] makes
   a registered filter with. */
@protocol CIFilterConstructor
- (nullable CIFilter *)filterWithName:(NSString *)name;
@end

NS_ASSUME_NONNULL_END
