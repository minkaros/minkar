/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Dock's view: a translucent rounded bar with one tile per app, a dot
 * under the running ones and a separator before the apps that are not
 * kept. It draws only when the model changes or a tile is pressed.
 *
 * Gienah's provisional look: the layout follows the reference platform's
 * Dock; sizes and colors are Gienah's own and will change with its visual
 * language guide.
 */

#import "DockView.h"

static const CGFloat kTileSize = 48;      /* the tilesize preference's default */
static const CGFloat kTileGap = 8;
static const CGFloat kBarPadding = 8;
static const CGFloat kIndicatorSpace = 8;
static const CGFloat kSeparatorSpace = 13;
static const CGFloat kBottomGap = 4;

@interface DockView : NSView
@property (nonatomic, strong) DockModel *model;
@end

@implementation DockView {
    NSInteger _pressed;
}

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) != nil)
        _pressed = -1;
    return self;
}

- (BOOL)isOpaque
{
    return NO;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return YES;
}

+ (CGFloat)widthForModel:(DockModel *)model
{
    NSUInteger count = [model.tiles count];
    CGFloat width = 2 * kBarPadding + count * kTileSize + (count ? count - 1 : 0) * kTileGap;

    if (model.firstRunningIndex > 0 && model.firstRunningIndex < count)
        width += kSeparatorSpace;
    return MAX(width, 2 * kBarPadding + kTileSize);
}

+ (CGFloat)height
{
    return 2 * kBarPadding + kTileSize + kIndicatorSpace;
}

- (NSRect)rectForTileAtIndex:(NSUInteger)index
{
    CGFloat x = kBarPadding + index * (kTileSize + kTileGap);

    if (self.model.firstRunningIndex > 0 && index >= self.model.firstRunningIndex)
        x += kSeparatorSpace;
    return NSMakeRect(x, kBarPadding + kIndicatorSpace, kTileSize, kTileSize);
}

- (NSInteger)tileIndexAtPoint:(NSPoint)point
{
    NSUInteger count = [self.model.tiles count];

    for (NSUInteger i = 0; i < count; i++)
        if (NSPointInRect(point, NSInsetRect([self rectForTileAtIndex:i], -kTileGap / 2, -kBarPadding)))
            return (NSInteger)i;
    return -1;
}

- (void)drawRect:(NSRect)dirty
{
    NSRect bar = NSInsetRect([self bounds], 0.5, 0.5);
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:bar xRadius:14 yRadius:14];

    [[NSColor clearColor] set];
    NSRectFill([self bounds]);
    [[NSColor colorWithCalibratedWhite:0.96 alpha:0.55] setFill];
    [path fill];
    [[NSColor colorWithCalibratedWhite:1.0 alpha:0.6] setStroke];
    [path setLineWidth:1];
    [path stroke];

    NSArray<DockTile *> *tiles = self.model.tiles;
    for (NSUInteger i = 0; i < [tiles count]; i++) {
        DockTile *tile = tiles[i];
        NSRect rect = [self rectForTileAtIndex:i];
        NSImage *icon = tile.icon;

        if (icon != nil)
            [icon drawInRect:rect fromRect:NSZeroRect operation:NSCompositeSourceOver
                    fraction:(NSInteger)i == _pressed ? 0.6 : 1.0];
        if (tile.application != nil) {
            NSRect dot = NSMakeRect(NSMidX(rect) - 2, kBarPadding / 2 + 2, 4, 4);
            [[NSColor colorWithCalibratedWhite:0.15 alpha:0.85] setFill];
            [[NSBezierPath bezierPathWithOvalInRect:dot] fill];
        }
    }

    NSUInteger first = self.model.firstRunningIndex;
    if (first > 0 && first < [tiles count]) {
        CGFloat x = NSMaxX([self rectForTileAtIndex:first - 1]) + (kTileGap + kSeparatorSpace) / 2;
        [[NSColor colorWithCalibratedWhite:0.2 alpha:0.35] setFill];
        NSRectFill(NSMakeRect(floor(x), kBarPadding, 1, NSHeight([self bounds]) - 2 * kBarPadding));
    }
}

- (void)mouseDown:(NSEvent *)event
{
    _pressed = [self tileIndexAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)event
{
    NSInteger index = [self tileIndexAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];

    if (index >= 0 && index == _pressed)
        [self.model openTile:self.model.tiles[(NSUInteger)index]];
    _pressed = -1;
    [self setNeedsDisplay:YES];
}

@end

@implementation DockWindow {
    DockModel *_model;
    DockView *_view;
}

- (instancetype)initWithModel:(DockModel *)model
{
    self = [super initWithContentRect:NSMakeRect(0, 0, 100, [DockView height])
                            styleMask:NSBorderlessWindowMask
                              backing:NSBackingStoreBuffered
                                defer:NO];
    if (self == nil)
        return nil;
    _model = model;
    _view = [[DockView alloc] initWithFrame:NSMakeRect(0, 0, 100, [DockView height])];
    _view.model = model;
    [_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self setContentView:_view];
    [self setLevel:kCGDockWindowLevelKey];
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setHasShadow:NO];
    [self setHidesOnDeactivate:NO];
    [self setExcludedFromWindowsMenu:YES];
    [self setReleasedWhenClosed:NO];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(modelChanged:)
                                                 name:DockModelDidChangeNotification object:model];
    [self layout];
    return self;
}

- (BOOL)canBecomeKeyWindow
{
    return NO;
}

- (BOOL)canBecomeMainWindow
{
    return NO;
}

- (void)layout
{
    NSRect screen = [[[NSScreen screens] firstObject] frame];
    CGFloat width = [DockView widthForModel:_model], height = [DockView height];
    NSRect frame = NSMakeRect(floor(NSMidX(screen) - width / 2), NSMinY(screen) + kBottomGap, width, height);

    if (!NSEqualRects(frame, [self frame]))
        [self setFrame:frame display:YES];
    [_view setNeedsDisplay:YES];
}

- (void)modelChanged:(NSNotification *)note
{
    [self layout];
}

@end
