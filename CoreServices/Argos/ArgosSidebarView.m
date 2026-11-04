/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Argos's sidebar. Atlas's provisional look.
 */

#import "ArgosSidebarView.h"
#import "ArgosIcons.h"

static const CGFloat kRow = 24, kSection = 26;

@implementation ArgosSidebarView {
    NSArray<NSDictionary *> *_rows;   /* title, url, or section */
}

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) != nil) {
        NSString *home = NSHomeDirectory();
        NSMutableArray *rows = [NSMutableArray arrayWithObject:@{ @"section": NSLocalizedString(@"Favorites", nil) }];
        NSArray *favorites = @[ @[ NSLocalizedString(@"Applications", nil), @"/Applications" ],
                                @[ NSLocalizedString(@"Desktop", nil), [home stringByAppendingPathComponent:@"Desktop"] ],
                                @[ NSLocalizedString(@"Documents", nil), [home stringByAppendingPathComponent:@"Documents"] ],
                                @[ NSLocalizedString(@"Downloads", nil), [home stringByAppendingPathComponent:@"Downloads"] ],
                                @[ [home lastPathComponent], home ] ];
        for (NSArray *f in favorites)
            if ([[NSFileManager defaultManager] fileExistsAtPath:f[1]])
                [rows addObject:@{ @"title": f[0], @"url": [NSURL fileURLWithPath:f[1]] }];
        [rows addObject:@{ @"section": NSLocalizedString(@"Locations", nil) }];
        [rows addObject:@{ @"title": @"Minkar", @"url": [NSURL fileURLWithPath:@"/"] }];
        _rows = rows;
    }
    return self;
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)setCurrentURL:(NSURL *)url
{
    _currentURL = [url copy];
    [self setNeedsDisplay:YES];
}

- (NSRect)rectForRow:(NSUInteger)index
{
    CGFloat y = 8;

    for (NSUInteger i = 0; i < index; i++)
        y += _rows[i][@"section"] ? kSection : kRow;
    return NSMakeRect(0, y, NSWidth([self bounds]), _rows[index][@"section"] ? kSection : kRow);
}

- (void)drawRect:(NSRect)dirty
{
    NSDictionary *section = @{ NSFontAttributeName: [NSFont boldSystemFontOfSize:11],
                               NSForegroundColorAttributeName: [NSColor colorWithCalibratedWhite:0.5 alpha:1] };
    NSDictionary *plain = @{ NSFontAttributeName: [NSFont systemFontOfSize:13],
                             NSForegroundColorAttributeName: [NSColor colorWithCalibratedWhite:0.15 alpha:1] };

    [[NSColor colorWithCalibratedWhite:0.93 alpha:1] set];
    NSRectFill(dirty);
    [[NSColor colorWithCalibratedWhite:0.82 alpha:1] set];
    NSRectFill(NSMakeRect(NSWidth([self bounds]) - 1, 0, 1, NSHeight([self bounds])));

    for (NSUInteger i = 0; i < [_rows count]; i++) {
        NSDictionary *row = _rows[i];
        NSRect r = [self rectForRow:i];
        if (row[@"section"]) {
            [row[@"section"] drawAtPoint:NSMakePoint(12, NSMinY(r) + 8) withAttributes:section];
            continue;
        }
        if ([[row[@"url"] path] isEqualToString:[self.currentURL path]]) {
            [[NSColor colorWithCalibratedWhite:0.82 alpha:1] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(r, 8, 1) xRadius:5 yRadius:5] fill];
        }
        [ArgosFolderIcon() drawInRect:NSMakeRect(16, NSMinY(r) + 4, 16, 16) fromRect:NSZeroRect
                            operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
        [row[@"title"] drawAtPoint:NSMakePoint(38, NSMinY(r) + 4) withAttributes:plain];
    }
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    for (NSUInteger i = 0; i < [_rows count]; i++)
        if (_rows[i][@"url"] && NSPointInRect(p, [self rectForRow:i])) {
            [self.delegate sidebarView:self chooseURL:_rows[i][@"url"]];
            return;
        }
}

@end
