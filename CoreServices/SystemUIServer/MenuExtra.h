/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What SystemUIServer asks of a Menu Extra: the principal class of a
 * .menu bundle in /System/Library/CoreServices/Menu Extras (MB1).
 */

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MenuExtra <NSObject>
/* The extra fills its status item: title or image, and menu. */
- (instancetype)initWithStatusItem:(NSStatusItem *)item;
@end

NS_ASSUME_NONNULL_END
