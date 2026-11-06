/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The file manager's generic icons, drawn once at 128 points and scaled when drawn.
 */

#import "FilesIcons.h"

static NSImage *drawnIcon(void (^draw)(NSRect bounds))
{
    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(128, 128)];

    [image lockFocus];
    draw(NSMakeRect(0, 0, 128, 128));
    [image unlockFocus];
    return image;
}

/* An icon of Atlas's set, loaded once. */
static NSImage *systemIcon(NSString *group, NSString *name)
{
    static NSMutableDictionary<NSString *, NSImage *> *cache;
    NSString *path = [NSString stringWithFormat:@"/System/Library/Icons/%@/%@.png", group, name];
    NSImage *icon;

    if (cache == nil)
        cache = [NSMutableDictionary dictionary];
    icon = cache[path];
    if (icon == nil) {
        icon = [[NSImage alloc] initWithContentsOfFile:path];
        if (icon == nil)
            return nil;
        cache[path] = icon;
    }
    return icon;
}

NSImage *FilesFolderIconForURL(NSURL *url)
{
    /* Resolved both: /var is a link to /private/var. */
    NSString *home = [NSHomeDirectory() stringByResolvingSymlinksInPath];
    NSString *path = [[url path] stringByResolvingSymlinksInPath];
    NSDictionary<NSString *, NSString *> *known = @{
        @"Desktop": @"user-desktop", @"Documents": @"folder-documents", @"Downloads": @"folder-downloads",
        @"Music": @"folder-music", @"Pictures": @"folder-pictures", @"Movies": @"folder-videos",
        @"Public": @"folder-publicshare", @"Library": @"folder-system", @".Trash": @"user-trash",
    };
    NSString *name = nil, *group = @"places";

    if ([path isEqualToString:home])
        name = @"folder-home";
    else if ([[path stringByDeletingLastPathComponent] isEqualToString:home])
        name = known[[path lastPathComponent]];
    else if ([path isEqualToString:@"/System"] || [path isEqualToString:@"/Library"])
        name = @"folder-system";
    if ([name isEqualToString:@"user-desktop"] || [name isEqualToString:@"user-trash"])
        group = @"system";
    NSImage *icon = name ? systemIcon(group, name) : nil;
    return icon ? icon : FilesFolderIcon();
}

NSImage *FilesFolderIcon(void)
{
    static NSImage *icon;

    if (icon == nil)
        icon = systemIcon(@"places", @"folder");
    if (icon == nil)
        icon = drawnIcon(^(NSRect b) {
            (void)b;
            NSBezierPath *tab = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(10, 84, 46, 22) xRadius:6 yRadius:6];
            [[NSColor colorWithCalibratedRed:0.36 green:0.62 blue:0.84 alpha:1] setFill];
            [tab fill];
            NSBezierPath *back = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(10, 18, 108, 80) xRadius:8 yRadius:8];
            [back fill];
            NSBezierPath *front = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(10, 18, 108, 70) xRadius:8 yRadius:8];
            [[NSColor colorWithCalibratedRed:0.48 green:0.74 blue:0.93 alpha:1] setFill];
            [front fill];
        });
    return icon;
}

NSImage *FilesDocumentIcon(void)
{
    static NSImage *icon;

    if (icon == nil)
        icon = systemIcon(@"system", @"text-x-generic");
    if (icon == nil)
        icon = drawnIcon(^(NSRect b) {
            (void)b;
            NSBezierPath *page = [NSBezierPath bezierPath];
            [page moveToPoint:NSMakePoint(26, 8)];
            [page lineToPoint:NSMakePoint(102, 8)];
            [page lineToPoint:NSMakePoint(102, 94)];
            [page lineToPoint:NSMakePoint(76, 120)];
            [page lineToPoint:NSMakePoint(26, 120)];
            [page closePath];
            [[NSColor colorWithCalibratedWhite:0.98 alpha:1] setFill];
            [page fill];
            [[NSColor colorWithCalibratedWhite:0.62 alpha:1] setStroke];
            [page setLineWidth:2];
            [page stroke];
            NSBezierPath *fold = [NSBezierPath bezierPath];
            [fold moveToPoint:NSMakePoint(76, 120)];
            [fold lineToPoint:NSMakePoint(76, 94)];
            [fold lineToPoint:NSMakePoint(102, 94)];
            [fold setLineWidth:2];
            [fold stroke];
            [[NSColor colorWithCalibratedWhite:0.8 alpha:1] setFill];
            for (int i = 0; i < 5; i++)
                NSRectFill(NSMakeRect(38, 30 + i * 12, 52, 3));
        });
    return icon;
}
