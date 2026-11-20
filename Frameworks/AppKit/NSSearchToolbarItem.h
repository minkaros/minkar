// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <AppKit/NSToolbarItem.h>
#include <os/availability.h>

@class NSSearchField;

NS_ASSUME_NONNULL_BEGIN

/* A toolbar item holding a search field, which it can collapse to a
   button and expand when searching begins. */
API_AVAILABLE(macos(11.0))
@interface NSSearchToolbarItem : NSToolbarItem
@property (strong) NSSearchField *searchField;
@property BOOL resignsFirstResponderWithCancel;
@property CGFloat preferredWidthForSearchField;
- (void)beginSearchInteraction;
- (void)endSearchInteraction;
@end

NS_ASSUME_NONNULL_END
