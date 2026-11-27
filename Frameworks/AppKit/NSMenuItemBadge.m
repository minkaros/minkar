/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Menu item badges (NSMenuItemBadge.h). The menu shows the badge's text
 * at the item's right, before its key equivalent.
 */

#import <AppKit/NSMenuItemBadge.h>
#import <objc/runtime.h>

@implementation NSMenuItemBadge

+ (instancetype)updatesWithCount:(NSInteger)itemCount {
    return [[[self alloc] initWithCount:itemCount type:NSMenuItemBadgeTypeUpdates] autorelease];
}

+ (instancetype)newItemsWithCount:(NSInteger)itemCount {
    return [[[self alloc] initWithCount:itemCount type:NSMenuItemBadgeTypeNewItems] autorelease];
}

+ (instancetype)alertsWithCount:(NSInteger)itemCount {
    return [[[self alloc] initWithCount:itemCount type:NSMenuItemBadgeTypeAlerts] autorelease];
}

/* The count and what it counts, in words ("3 updates"). */
- (instancetype)initWithCount:(NSInteger)itemCount type:(NSMenuItemBadgeType)type {
    if ((self = [super init]) != nil) {
        NSBundle *bundle = [NSBundle bundleForClass:[NSMenuItemBadge class]];
        NSString *format = nil;
        _itemCount = itemCount;
        _type = type;
        switch (type) {
        case NSMenuItemBadgeTypeUpdates:
            format = itemCount == 1 ? @"%ld update" : @"%ld updates";
            break;
        case NSMenuItemBadgeTypeNewItems:
            format = itemCount == 1 ? @"%ld new item" : @"%ld new items";
            break;
        case NSMenuItemBadgeTypeAlerts:
            format = itemCount == 1 ? @"%ld alert" : @"%ld alerts";
            break;
        default:
            format = @"%ld";
            break;
        }
        format = [bundle localizedStringForKey:format value:format table:nil];
        _stringValue = [[NSString alloc] initWithFormat:format, (long)itemCount];
    }
    return self;
}

- (instancetype)initWithCount:(NSInteger)itemCount {
    return [self initWithCount:itemCount type:NSMenuItemBadgeTypeNone];
}

- (instancetype)initWithString:(NSString *)string {
    if ((self = [super init]) != nil) {
        _type = NSMenuItemBadgeTypeNone;
        _stringValue = [string copy];
    }
    return self;
}

- (void)dealloc {
    [_stringValue release];
    [super dealloc];
}

- (NSInteger)itemCount { return _itemCount; }
- (NSMenuItemBadgeType)type { return _type; }
- (NSString *)stringValue { return _stringValue; }

@end

static char badgeKey;

@implementation NSMenuItem (Badge)

- (NSMenuItemBadge *)badge {
    return objc_getAssociatedObject(self, &badgeKey);
}

- (void)setBadge:(NSMenuItemBadge *)badge {
    objc_setAssociatedObject(self, &badgeKey, badge, OBJC_ASSOCIATION_RETAIN);
    [[self menu] itemChanged:self];
}

@end
