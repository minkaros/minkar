/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Argos's generic icons, drawn once at 128 points and scaled when drawn.
 */

#import "ArgosIcons.h"

static NSImage *drawnIcon(void (^draw)(NSRect bounds))
{
    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(128, 128)];

    [image lockFocus];
    draw(NSMakeRect(0, 0, 128, 128));
    [image unlockFocus];
    return image;
}

NSImage *ArgosFolderIcon(void)
{
    static NSImage *icon;

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

NSImage *ArgosDocumentIcon(void)
{
    static NSImage *icon;

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
