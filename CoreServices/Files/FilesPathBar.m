/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The path bar (FilesPathBar.h).
 */

#import "FilesPathBar.h"

static const CGFloat kIcon = 14, kGap = 4, kArrow = 14;

@implementation FilesPathBar {
    NSArray<NSURL *> *_folders;
    NSMutableArray<NSValue *> *_frames;
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)setURL:(NSURL *)url
{
    NSMutableArray<NSURL *> *folders = [NSMutableArray array];

    _URL = [url copy];
    for (NSString *path = [[url path] stringByStandardizingPath]; path != nil;) {
        [folders insertObject:[NSURL fileURLWithPath:path isDirectory:YES] atIndex:0];
        if ([path isEqualToString:@"/"] || [path length] == 0)
            break;
        path = [path stringByDeletingLastPathComponent];
    }
    _folders = folders;
    [self setNeedsDisplay:YES];
}

static NSString *nameOf(NSURL *folder)
{
    if ([[folder path] isEqualToString:@"/"])
        return @"Minkar";
    return [[NSFileManager defaultManager] displayNameAtPath:[folder path]];
}

- (void)drawRect:(NSRect)dirty
{
    NSDictionary *attrs = @{ NSFontAttributeName: [NSFont systemFontOfSize:11],
                             NSForegroundColorAttributeName: [NSColor secondaryLabelColor] };
    NSDictionary *last = @{ NSFontAttributeName: [NSFont systemFontOfSize:11],
                            NSForegroundColorAttributeName: [NSColor labelColor] };
    NSDictionary *arrow = @{ NSFontAttributeName: [NSFont systemFontOfSize:10],
                             NSForegroundColorAttributeName: [NSColor tertiaryLabelColor] };
    NSRect b = [self bounds];
    CGFloat x = 8;

    [[NSColor separatorColor] setFill];
    NSRectFill(NSMakeRect(0, 0, NSWidth(b), 1));
    _frames = [NSMutableArray array];
    for (NSUInteger i = 0; i < [_folders count]; i++) {
        NSURL *folder = _folders[i];
        NSString *name = nameOf(folder);
        NSDictionary *a = i + 1 == [_folders count] ? last : attrs;
        NSSize size = [name sizeWithAttributes:a];
        NSImage *icon = [[NSWorkspace sharedWorkspace] iconForFile:[folder path]];
        CGFloat start = x;

        [icon drawInRect:NSMakeRect(x, floor((NSHeight(b) - kIcon) / 2) + 1, kIcon, kIcon) fromRect:NSZeroRect
               operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:nil];
        x += kIcon + kGap;
        [name drawAtPoint:NSMakePoint(x, floor((NSHeight(b) - size.height) / 2) + 1) withAttributes:a];
        x += ceil(size.width);
        [_frames addObject:[NSValue valueWithRect:NSMakeRect(start, 0, x - start, NSHeight(b))]];
        if (i + 1 < [_folders count]) {
            [@"›" drawAtPoint:NSMakePoint(x + 4, floor((NSHeight(b) - 12) / 2)) withAttributes:arrow];
            x += kArrow;
        }
        if (x > NSWidth(b))
            break;
    }
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    for (NSUInteger i = 0; i < [_frames count]; i++)
        if (NSPointInRect(p, [_frames[i] rectValue])) {
            if (self.chooser != nil && (i + 1 < [_folders count] || [event clickCount] == 2))
                self.chooser(_folders[i]);
            return;
        }
}

@end
