// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>
#import <AppKit/AppKitExport.h>

@class NSString, NSColor, NSImage;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, NSTableViewRowActionStyle) {
    NSTableViewRowActionStyleRegular,
    NSTableViewRowActionStyleDestructive,
};

/* An action shown when a table row is swiped: its title, its style and
   what it does. */
@interface NSTableViewRowAction : NSObject <NSCopying>
+ (instancetype)rowActionWithStyle:(NSTableViewRowActionStyle)style title:(NSString *)title handler:(void (^)(NSTableViewRowAction *action, NSInteger row))handler;
@property (readonly) NSTableViewRowActionStyle style;
@property (copy) NSString *title;
@property (null_resettable, copy) NSColor *backgroundColor;
@property (nullable, strong) NSImage *image;
@end

NS_ASSUME_NONNULL_END
