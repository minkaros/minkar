/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Argos's Get Info window. Atlas's provisional look.
 */

#import "ArgosInfoWindow.h"
#include <os/log.h>

@interface ArgosInfoView : NSView
@property (nonatomic, strong) ArgosItem *item;
@property (nonatomic, copy) NSArray<NSArray<NSString *> *> *rows;
@end

@implementation ArgosInfoView

- (BOOL)isFlipped
{
    return YES;
}

- (void)drawRect:(NSRect)dirty
{
    NSDictionary *name = @{ NSFontAttributeName: [NSFont boldSystemFontOfSize:14],
                            NSForegroundColorAttributeName: [NSColor blackColor] };
    NSDictionary *label = @{ NSFontAttributeName: [NSFont boldSystemFontOfSize:11],
                             NSForegroundColorAttributeName: [NSColor colorWithCalibratedWhite:0.35 alpha:1] };
    NSDictionary *value = @{ NSFontAttributeName: [NSFont systemFontOfSize:11],
                             NSForegroundColorAttributeName: [NSColor blackColor] };

    [[NSColor colorWithCalibratedWhite:0.95 alpha:1] set];
    NSRectFill(dirty);
    [self.item.icon drawInRect:NSMakeRect(16, 16, 48, 48) fromRect:NSZeroRect
                     operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
    [self.item.name drawAtPoint:NSMakePoint(76, 22) withAttributes:name];
    [self.item.kind drawAtPoint:NSMakePoint(76, 44) withAttributes:value];
    [[NSColor colorWithCalibratedWhite:0.82 alpha:1] set];
    NSRectFill(NSMakeRect(12, 78, NSWidth([self bounds]) - 24, 1));

    CGFloat y = 92;
    for (NSArray<NSString *> *row in self.rows) {
        NSSize size = [row[0] sizeWithAttributes:label];
        [row[0] drawAtPoint:NSMakePoint(100 - size.width, y) withAttributes:label];
        [row[1] drawInRect:NSMakeRect(108, y, NSWidth([self bounds]) - 120, 34) withAttributes:value];
        y += [row[1] length] > 34 ? 34 : 20;
    }
}

@end

@implementation ArgosInfoWindow

static NSMutableArray<ArgosInfoWindow *> *openWindows;

+ (void)showForItem:(ArgosItem *)item
{
    if (openWindows == nil)
        openWindows = [NSMutableArray array];
    os_log_info(OS_LOG_DEFAULT, "info %{public}s", [[item.URL path] fileSystemRepresentation]);

    ArgosInfoWindow *window = [[ArgosInfoWindow alloc] initWithContentRect:NSMakeRect(220, 200, 320, 280)
        styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask
          backing:NSBackingStoreBuffered defer:NO];
    [window setTitle:[NSString stringWithFormat:@"%@ Info", item.name]];
    [window setReleasedWhenClosed:NO];

    NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:[item.URL path] error:NULL];
    NSDateFormatter *dates = [[NSDateFormatter alloc] init];
    [dates setDateFormat:@"EEEE, d MMMM yyyy 'at' HH:mm"];
    NSString *size;
    if (item.isFolder)
        size = [NSString stringWithFormat:@"%lu items",
                (unsigned long)[[[NSFileManager defaultManager] contentsOfDirectoryAtPath:[item.URL path] error:NULL] count]];
    else
        size = [NSString stringWithFormat:@"%llu bytes (%@ on disk)", item.size, ArgosSizeString(item.size)];
    NSDate *created = [attributes fileCreationDate], *modified = [attributes fileModificationDate];

    ArgosInfoView *view = [[ArgosInfoView alloc] initWithFrame:NSMakeRect(0, 0, 320, 280)];
    view.item = item;
    view.rows = @[ @[ @"Kind:", item.kind ],
                   @[ @"Size:", size ],
                   @[ @"Where:", [[item.URL path] stringByDeletingLastPathComponent] ],
                   @[ @"Created:", created ? [dates stringFromDate:created] : @"--" ],
                   @[ @"Modified:", modified ? [dates stringFromDate:modified] : @"--" ],
                   @[ @"Owner:", [attributes fileOwnerAccountName] ?: @"--" ] ];
    [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [window setContentView:view];
    [openWindows addObject:window];
    [window makeKeyAndOrderFront:nil];
}

- (void)close
{
    [super close];
    [openWindows removeObject:self];
}

@end
