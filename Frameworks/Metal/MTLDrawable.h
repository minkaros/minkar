/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Something to draw into and show: a layer's drawable.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLDrawable <NSObject>
- (void)present;
@property (nonatomic, readonly) NSUInteger drawableID;
@end

NS_ASSUME_NONNULL_END
