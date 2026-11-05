/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The file manager's Get Info window. Atlas's provisional look.
 */

#import "FilesInfoWindow.h"
#include <os/log.h>

@interface FilesInfoView : NSView
@property (nonatomic, strong) FilesItem *item;
@property (nonatomic, copy) NSArray<NSArray<NSString *> *> *rows;
@end

@implementation FilesInfoView

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

@implementation FilesInfoWindow

static NSMutableArray<FilesInfoWindow *> *openWindows;

+ (void)showForItem:(FilesItem *)item
{
    if (openWindows == nil)
        openWindows = [NSMutableArray array];
    os_log_info(OS_LOG_DEFAULT, "info %{public}s", [[item.URL path] fileSystemRepresentation]);

    FilesInfoWindow *window = [[FilesInfoWindow alloc] initWithContentRect:NSMakeRect(220, 200, 320, 280)
        styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask
          backing:NSBackingStoreBuffered defer:NO];
    [window setTitle:[NSString stringWithFormat:NSLocalizedString(@"%@ Info", nil), item.name]];
    [window setReleasedWhenClosed:NO];

    NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:[item.URL path] error:NULL];
    NSDateFormatter *dates = [[NSDateFormatter alloc] init];
    [dates setLocale:FilesDisplayLocale()];
    [dates setDateFormat:NSLocalizedString(@"EEEE, d MMMM yyyy 'at' HH:mm", @"Get Info dates")];
    NSString *size;
    if (item.isFolder)
        size = [NSString stringWithFormat:NSLocalizedString(@"%lu items", nil),
                (unsigned long)[[[NSFileManager defaultManager] contentsOfDirectoryAtPath:[item.URL path] error:NULL] count]];
    else
        size = [NSString stringWithFormat:NSLocalizedString(@"%llu bytes (%@ on disk)", nil), item.size, FilesSizeString(item.size)];
    NSDate *created = [attributes fileCreationDate], *modified = [attributes fileModificationDate];

    FilesInfoView *view = [[FilesInfoView alloc] initWithFrame:NSMakeRect(0, 0, 320, 280)];
    view.item = item;
    view.rows = @[ @[ NSLocalizedString(@"Kind:", nil), item.kind ],
                   @[ NSLocalizedString(@"Size:", nil), size ],
                   @[ NSLocalizedString(@"Where:", nil), [[item.URL path] stringByDeletingLastPathComponent] ],
                   @[ NSLocalizedString(@"Created:", nil), created ? [dates stringFromDate:created] : @"--" ],
                   @[ NSLocalizedString(@"Modified:", nil), modified ? [dates stringFromDate:modified] : @"--" ],
                   @[ NSLocalizedString(@"Owner:", nil), [attributes fileOwnerAccountName] ?: @"--" ] ];
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
