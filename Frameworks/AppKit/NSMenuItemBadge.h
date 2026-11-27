/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A menu item's badge (API 24): a count of updates, new items or alerts,
 * or a short text, shown at the item's right.
 */

#import <Foundation/Foundation.h>
#import <AppKit/NSMenuItem.h>

typedef NS_ENUM(NSInteger, NSMenuItemBadgeType) {
    NSMenuItemBadgeTypeNone = 0,
    NSMenuItemBadgeTypeUpdates,
    NSMenuItemBadgeTypeNewItems,
    NSMenuItemBadgeTypeAlerts
};

API_AVAILABLE(macos(14.0))
@interface NSMenuItemBadge : NSObject {
    NSInteger _itemCount;
    NSMenuItemBadgeType _type;
    NSString *_stringValue;
}

+ (instancetype)updatesWithCount:(NSInteger)itemCount;
+ (instancetype)newItemsWithCount:(NSInteger)itemCount;
+ (instancetype)alertsWithCount:(NSInteger)itemCount;
- (instancetype)initWithCount:(NSInteger)itemCount type:(NSMenuItemBadgeType)type;
- (instancetype)initWithCount:(NSInteger)itemCount;
- (instancetype)initWithString:(NSString *)string;

@property(readonly) NSInteger itemCount;
@property(readonly) NSMenuItemBadgeType type;
@property(readonly, copy) NSString *stringValue;
@end

@interface NSMenuItem (Badge)
@property(strong) NSMenuItemBadge *badge API_AVAILABLE(macos(14.0));
@end
