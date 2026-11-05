/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Dock's view and window (Atlas.md, D1 and D2): a translucent bar
 * along the bottom, left or right edge of the main display with one tile
 * per app, a dot under the running ones and a separator before those not
 * kept. Pointing at a tile shows its name (and magnifies the tiles near it
 * with the magnification preference); a click opens or activates its app;
 * a right click, a Control-click or a long press opens its menu; a tile is
 * dragged along the Dock to move it, into the kept part to keep it, and
 * off the Dock to remove it. With autohide the Dock slides off its edge and
 * comes back when the pointer reaches that edge.
 *
 * It draws only when the model changes or the pointer acts on it.
 *
 * Atlas's provisional look: the layout follows the reference platform's
 * Dock; sizes and colors are Atlas's own and will change with its visual
 * language guide.
 */

#import "DockView.h"
#include <math.h>

static const CGFloat kEdgeGap = 4;          /* between the bar and the edge */
static const NSTimeInterval kLongPress = 0.6;
static const CGFloat kDragThreshold = 4;
static const NSTimeInterval kSlideTime = 0.2;
static const NSTimeInterval kAutohideDelay = 0.5;  /* autohide-delay's default */

/* ---- Geometry along the Dock (main axis) and away from its edge (cross axis) ---- */

typedef struct {
    CGFloat main, cross, mainLength, crossLength;
} DockSpan;

/* The bar's sizes for a tile size. */
static CGFloat padding(CGFloat tile) { return round(tile / 6); }
static CGFloat gapFor(CGFloat tile) { return round(tile / 6); }
static CGFloat indicatorSpace(CGFloat tile) { return round(tile / 6); }
static CGFloat separatorSpace(CGFloat tile) { return round(tile / 4); }

/* ---- The tile's name, in a small window beside it ---- */

@interface DockLabelView : NSView
@property (nonatomic, copy) NSString *text;
@end

@implementation DockLabelView

+ (NSDictionary *)attributes
{
    return @{ NSFontAttributeName: [NSFont systemFontOfSize:13],
              NSForegroundColorAttributeName: [NSColor whiteColor] };
}

- (BOOL)isOpaque
{
    return NO;
}

- (void)drawRect:(NSRect)dirty
{
    [[NSColor clearColor] set];
    NSRectFill([self bounds]);
    [[NSColor colorWithCalibratedWhite:0.12 alpha:0.85] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:[self bounds] xRadius:6 yRadius:6] fill];
    NSSize size = [self.text sizeWithAttributes:[DockLabelView attributes]];
    [self.text drawAtPoint:NSMakePoint(round((NSWidth([self bounds]) - size.width) / 2),
                                       round((NSHeight([self bounds]) - size.height) / 2))
            withAttributes:[DockLabelView attributes]];
}

@end

@interface DockLabelWindow : NSWindow
- (void)showText:(NSString *)text beside:(NSRect)tileOnScreen orientation:(DockOrientation)orientation;
@end

@implementation DockLabelWindow {
    DockLabelView *_view;
}

- (instancetype)init
{
    self = [super initWithContentRect:NSMakeRect(0, 0, 10, 10) styleMask:NSBorderlessWindowMask
                              backing:NSBackingStoreBuffered defer:NO];
    if (self != nil) {
        _view = [[DockLabelView alloc] initWithFrame:NSMakeRect(0, 0, 10, 10)];
        [_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [self setContentView:_view];
        [self setLevel:kCGDockWindowLevelKey];
        [self setOpaque:NO];
        [self setBackgroundColor:[NSColor clearColor]];
        [self setHasShadow:NO];
        [self setIgnoresMouseEvents:YES];
        [self setHidesOnDeactivate:NO];
        [self setReleasedWhenClosed:NO];
    }
    return self;
}

- (BOOL)canBecomeKeyWindow
{
    return NO;
}

- (void)showText:(NSString *)text beside:(NSRect)tile orientation:(DockOrientation)orientation
{
    NSSize size = [text sizeWithAttributes:[DockLabelView attributes]];
    CGFloat width = ceil(size.width) + 20, height = ceil(size.height) + 8;
    NSRect frame;

    switch (orientation) {
    case DockOrientationLeft:
        frame = NSMakeRect(NSMaxX(tile) + 10, NSMidY(tile) - height / 2, width, height);
        break;
    case DockOrientationRight:
        frame = NSMakeRect(NSMinX(tile) - 10 - width, NSMidY(tile) - height / 2, width, height);
        break;
    default:
        frame = NSMakeRect(NSMidX(tile) - width / 2, NSMaxY(tile) + 10, width, height);
        break;
    }
    frame = NSIntegralRect(frame);
    _view.text = text;
    [self setFrame:frame display:NO];
    [_view setNeedsDisplay:YES];
    [self orderFront:nil];
}

@end

/* ---- A dragged tile, under the pointer anywhere on the screen ---- */

@interface DockDragView : NSView
@property (nonatomic, strong) NSImage *icon;
@property (nonatomic) CGFloat fraction;
@end

@implementation DockDragView

- (BOOL)isOpaque
{
    return NO;
}

- (void)drawRect:(NSRect)dirty
{
    [[NSColor clearColor] set];
    NSRectFill([self bounds]);
    [self.icon drawInRect:[self bounds] fromRect:NSZeroRect operation:NSCompositeSourceOver
                 fraction:self.fraction];
}

@end

@interface DockDragWindow : NSWindow
- (void)showIcon:(NSImage *)icon size:(CGFloat)size at:(NSPoint)screenPoint removing:(BOOL)removing;
@end

@implementation DockDragWindow {
    DockDragView *_view;
}

- (instancetype)init
{
    self = [super initWithContentRect:NSMakeRect(0, 0, 48, 48) styleMask:NSBorderlessWindowMask
                              backing:NSBackingStoreBuffered defer:NO];
    if (self != nil) {
        _view = [[DockDragView alloc] initWithFrame:NSMakeRect(0, 0, 48, 48)];
        [_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [self setContentView:_view];
        [self setLevel:kCGDraggingWindowLevelKey];
        [self setOpaque:NO];
        [self setBackgroundColor:[NSColor clearColor]];
        [self setHasShadow:NO];
        [self setIgnoresMouseEvents:YES];
        [self setHidesOnDeactivate:NO];
        [self setReleasedWhenClosed:NO];
    }
    return self;
}

- (BOOL)canBecomeKeyWindow
{
    return NO;
}

- (void)showIcon:(NSImage *)icon size:(CGFloat)size at:(NSPoint)p removing:(BOOL)removing
{
    CGFloat fraction = removing ? 0.4 : 0.9;
    NSRect frame = NSIntegralRect(NSMakeRect(p.x - size / 2, p.y - size / 2, size, size));

    if (_view.icon != icon || _view.fraction != fraction) {
        _view.icon = icon;
        _view.fraction = fraction;
        [_view setNeedsDisplay:YES];
    }
    [self setFrame:frame display:YES];
    if (![self isVisible])
        [self orderFront:nil];
}

@end

/* ---- The view ---- */

@interface DockView : NSView
@property (nonatomic, strong) DockModel *model;
@property (nonatomic, weak) DockWindow *dockWindow;
/* Where the pointer is along the Dock, or NAN when it is not on it. */
@property (nonatomic) CGFloat pointerMain;
@end

@interface DockWindow ()
- (void)pointerInside:(BOOL)inside;
- (DockLabelWindow *)label;
@end

@implementation DockView {
    NSInteger _pressed;       /* the tile under a pressed button, or -1 */
    NSPoint _downPoint;
    BOOL _dragging;
    NSInteger _dropIndex;     /* where a dragged tile goes among the kept ones */
    BOOL _dropRemoves;        /* dragged off the Dock */
    NSPoint _dragPoint;
    NSTimer *_pressTimer;
    NSTrackingArea *_tracking;
    DockDragWindow *_dragWindow;
    NSInteger _labelled;      /* the tile whose label shows, or -1 */
}

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) != nil) {
        _pressed = -1;
        _labelled = -1;
        _pointerMain = NAN;
    }
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

- (void)updateTrackingAreas
{
    if (_tracking != nil)
        [self removeTrackingArea:_tracking];
    _tracking = [[NSTrackingArea alloc] initWithRect:[self bounds]
        options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveAlways |
                NSTrackingInVisibleRect
          owner:self userInfo:nil];
    [self addTrackingArea:_tracking];
    [super updateTrackingAreas];
}

/* -- Geometry -- */

- (DockOrientation)orientation
{
    return self.model.orientation;
}

/* A span in view coordinates: main along the Dock (left to right, or top
   to bottom on a side), cross away from the screen's edge. */
- (NSRect)rectForSpan:(DockSpan)s
{
    NSRect b = [self bounds];

    switch ([self orientation]) {
    case DockOrientationLeft:
        return NSMakeRect(s.cross, NSHeight(b) - s.main - s.mainLength, s.crossLength, s.mainLength);
    case DockOrientationRight:
        return NSMakeRect(NSWidth(b) - s.cross - s.crossLength, NSHeight(b) - s.main - s.mainLength,
                          s.crossLength, s.mainLength);
    default:
        return NSMakeRect(s.main, s.cross, s.mainLength, s.crossLength);
    }
}

- (CGFloat)mainOfPoint:(NSPoint)p
{
    return [self orientation] == DockOrientationBottom ? p.x : NSHeight([self bounds]) - p.y;
}

- (CGFloat)crossOfPoint:(NSPoint)p
{
    switch ([self orientation]) {
    case DockOrientationLeft: return p.x;
    case DockOrientationRight: return NSWidth([self bounds]) - p.x;
    default: return p.y;
    }
}

- (CGFloat)mainLength
{
    return [self orientation] == DockOrientationBottom ? NSWidth([self bounds]) : NSHeight([self bounds]);
}

/* The bar's length with every tile at its own size. */
+ (CGFloat)baseLengthForModel:(DockModel *)model
{
    CGFloat tile = model.tileSize;
    NSUInteger count = [model.tiles count];
    CGFloat length = 2 * padding(tile) + count * tile + (count ? count - 1 : 0) * gapFor(tile);

    if (model.firstRunningIndex > 0 && model.firstRunningIndex < count)
        length += separatorSpace(tile);
    return MAX(length, 2 * padding(tile) + tile);
}

/* The window's length: room for the magnified tiles. */
+ (CGFloat)windowLengthForModel:(DockModel *)model
{
    CGFloat extra = model.magnification ? 3.2 * MAX(model.largeSize - model.tileSize, 0) : 0;

    return ceil([self baseLengthForModel:model] + extra);
}

+ (CGFloat)barThicknessForModel:(DockModel *)model
{
    CGFloat tile = model.tileSize;

    return 2 * padding(tile) + tile + indicatorSpace(tile);
}

+ (CGFloat)windowThicknessForModel:(DockModel *)model
{
    CGFloat extra = model.magnification ? MAX(model.largeSize - model.tileSize, 0) : 0;

    return ceil([self barThicknessForModel:model] + extra);
}

/* Each tile's size: the tile size, or more near the pointer. */
- (CGFloat)sizeOfTileAtIndex:(NSUInteger)index baseCenter:(CGFloat)center
{
    CGFloat tile = self.model.tileSize, large = MAX(self.model.largeSize, tile);

    if (!self.model.magnification || isnan(self.pointerMain) || _dragging)
        return tile;
    CGFloat distance = fabs(self.pointerMain - center) / (tile + gapFor(tile));
    if (distance >= 3)
        return tile;
    return tile + (large - tile) * (cos(distance * M_PI / 3) + 1) / 2;
}

/* The tiles' places along the Dock, in the order shown (a dragged tile
   leaves a gap where it would go). */
- (void)layoutMains:(CGFloat *)mains sizes:(CGFloat *)sizes count:(NSUInteger)count
         barStart:(CGFloat *)barStart barLength:(CGFloat *)barLength
{
    DockModel *model = self.model;
    CGFloat tile = model.tileSize, gap = gapFor(tile), sep = separatorSpace(tile);
    CGFloat base = [DockView baseLengthForModel:model];
    CGFloat baseStart = round(([self mainLength] - base) / 2);
    NSUInteger first = model.firstRunningIndex;
    CGFloat x, total;

    /* The base centers, to measure the pointer against. */
    x = baseStart + padding(tile);
    total = 0;
    for (NSUInteger i = 0; i < count; i++) {
        if (i == first && first > 0)
            x += sep;
        sizes[i] = [self sizeOfTileAtIndex:i baseCenter:x + tile / 2];
        x += tile + gap;
        total += sizes[i];
    }
    total += 2 * padding(tile) + (count ? count - 1 : 0) * gap;
    if (first > 0 && first < count)
        total += sep;
    total = MAX(total, 2 * padding(tile) + tile);

    *barLength = total;
    *barStart = round(([self mainLength] - total) / 2);
    x = *barStart + padding(tile);
    for (NSUInteger i = 0; i < count; i++) {
        if (i == first && first > 0)
            x += sep;
        mains[i] = x;
        x += sizes[i] + gap;
    }
}

- (NSRect)rectForTileAtIndex:(NSUInteger)index
{
    NSUInteger count = [self.model.tiles count];
    CGFloat mains[count + 1], sizes[count + 1], start, length;
    CGFloat tile = self.model.tileSize;

    if (index >= count)
        return NSZeroRect;
    [self layoutMains:mains sizes:sizes count:count barStart:&start barLength:&length];
    DockSpan s = { mains[index], padding(tile) + indicatorSpace(tile), sizes[index], sizes[index] };
    return [self rectForSpan:s];
}

- (NSInteger)tileIndexAtPoint:(NSPoint)point
{
    NSUInteger count = [self.model.tiles count];

    for (NSUInteger i = 0; i < count; i++) {
        /* The icon along the Dock (not the gaps beside it), and across
         * it down to the screen's edge. */
        NSRect hit = [self rectForTileAtIndex:i];
        switch ([self orientation]) {
        case DockOrientationLeft: hit.size.width += hit.origin.x; hit.origin.x = 0; break;
        case DockOrientationRight: hit.size.width = NSWidth([self bounds]) - hit.origin.x; break;
        default: hit.size.height += hit.origin.y; hit.origin.y = 0; break;
        }
        if (NSPointInRect(point, hit))
            return (NSInteger)i;
    }
    return -1;
}

/* -- Drawing -- */

- (void)drawRect:(NSRect)dirty
{
    DockModel *model = self.model;
    NSArray<DockTile *> *tiles = model.tiles;
    NSUInteger count = [tiles count];
    CGFloat mains[count + 1], sizes[count + 1], start, length;
    CGFloat tile = model.tileSize, ind = indicatorSpace(tile), pad = padding(tile);

    [[NSColor clearColor] set];
    NSRectFill([self bounds]);
    [self layoutMains:mains sizes:sizes count:count barStart:&start barLength:&length];

    DockSpan barSpan = { start, 0, length, [DockView barThicknessForModel:model] };
    NSRect bar = NSInsetRect([self rectForSpan:barSpan], 0.5, 0.5);
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:bar xRadius:14 yRadius:14];
    [[NSColor colorWithCalibratedWhite:0.96 alpha:0.55] setFill];
    [path fill];
    [[NSColor colorWithCalibratedWhite:1.0 alpha:0.6] setStroke];
    [path setLineWidth:1];
    [path stroke];

    for (NSUInteger i = 0; i < count; i++) {
        DockTile *t = tiles[i];
        DockSpan s = { mains[i], pad + ind, sizes[i], sizes[i] };
        NSRect rect = [self rectForSpan:s];
        BOOL lifted = _dragging && (NSInteger)i == _pressed;

        if (!lifted && t.icon != nil)
            [t.icon drawInRect:rect fromRect:NSZeroRect operation:NSCompositeSourceOver
                      fraction:(NSInteger)i == _pressed ? 0.6 : 1.0];
        if (t.application != nil) {
            DockSpan d = { mains[i] + sizes[i] / 2 - 2, pad / 2 + 2, 4, 4 };
            [[NSColor colorWithCalibratedWhite:0.15 alpha:0.85] setFill];
            [[NSBezierPath bezierPathWithOvalInRect:[self rectForSpan:d]] fill];
        }
    }

    NSUInteger first = model.firstRunningIndex;
    if (first > 0 && first < count) {
        CGFloat at = mains[first - 1] + sizes[first - 1] + (gapFor(tile) + separatorSpace(tile)) / 2;
        DockSpan s = { floor(at), pad, 1, [DockView barThicknessForModel:model] - 2 * pad };
        [[NSColor colorWithCalibratedWhite:0.2 alpha:0.35] setFill];
        NSRectFill([self rectForSpan:s]);
    }
}

/* -- The pointer -- */

/* Whether point is in the gap between the tile at index and a neighbour,
 * along the Dock: there a label already shown stays, rather than going
 * and coming back as the pointer crosses. */
- (BOOL)point:(NSPoint)point inGapBesideTile:(NSInteger)index
{
    NSUInteger count = [self.model.tiles count];

    if (index < 0 || (NSUInteger)index >= count)
        return NO;
    NSRect r = [self rectForTileAtIndex:(NSUInteger)index];
    BOOL vertical = [self orientation] != DockOrientationBottom;
    CGFloat p = vertical ? point.y : point.x;
    CGFloat lo = vertical ? NSMinY(r) : NSMinX(r), hi = vertical ? NSMaxY(r) : NSMaxX(r);
    for (int side = -1; side <= 1; side += 2) {
        NSInteger n = index + side;
        if (n < 0 || (NSUInteger)n >= count)
            continue;
        NSRect o = [self rectForTileAtIndex:(NSUInteger)n];
        CGFloat olo = vertical ? NSMinY(o) : NSMinX(o), ohi = vertical ? NSMaxY(o) : NSMaxX(o);
        CGFloat a = MIN(hi, ohi), b = MAX(lo, olo);	/* the gap between them */
        if (a < b && p >= a && p <= b)
            return YES;
    }
    return NO;
}

- (void)showLabelForPoint:(NSPoint)point
{
    NSInteger index = _dragging ? -1 : [self tileIndexAtPoint:point];
    DockLabelWindow *label = [self.dockWindow label];

    if (index < 0 && !_dragging && [self point:point inGapBesideTile:_labelled])
        return;
    if (index < 0) {
        _labelled = -1;
        [label orderOut:nil];
        return;
    }
    if (index == _labelled && [label isVisible])
        return;
    _labelled = index;
    NSRect rect = [self rectForTileAtIndex:(NSUInteger)index];
    NSRect onScreen = rect;
    onScreen.origin.x += NSMinX([[self window] frame]);
    onScreen.origin.y += NSMinY([[self window] frame]);
    [label showText:self.model.tiles[(NSUInteger)index].label beside:onScreen orientation:[self orientation]];
}

- (void)mouseMoved:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    if (!NSPointInRect(p, [self bounds])) {
        [self mouseExited:event];
        return;
    }
    self.pointerMain = [self mainOfPoint:p];
    [self.dockWindow pointerInside:YES];
    if (self.model.magnification)
        [self setNeedsDisplay:YES];
    [self showLabelForPoint:p];
}

- (void)mouseEntered:(NSEvent *)event
{
    [self.dockWindow pointerInside:YES];
}

- (void)mouseExited:(NSEvent *)event
{
    if (_pressed >= 0)
        return;
    self.pointerMain = NAN;
    [[self.dockWindow label] orderOut:nil];
    [self.dockWindow pointerInside:NO];
    if (self.model.magnification)
        [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    _pressed = [self tileIndexAtPoint:p];
    _downPoint = p;
    _dragging = NO;
    [[self.dockWindow label] orderOut:nil];
    if (_pressed >= 0 && ([event modifierFlags] & NSControlKeyMask)) {
        [self showMenuForTile:(NSUInteger)_pressed event:event];
        return;
    }
    if (_pressed >= 0) {
        NSEvent *down = event;
        _pressTimer = [NSTimer scheduledTimerWithTimeInterval:kLongPress repeats:NO block:^(NSTimer *timer) {
            (void)timer;
            if (!self->_dragging && self->_pressed >= 0)
                [self showMenuForTile:(NSUInteger)self->_pressed event:down];
        }];
    }
    [self setNeedsDisplay:YES];
}

- (void)rightMouseDown:(NSEvent *)event
{
    NSInteger index = [self tileIndexAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];

    [[self.dockWindow label] orderOut:nil];
    if (index >= 0) {
        _pressed = index;
        [self showMenuForTile:(NSUInteger)index event:event];
    }
}

- (void)mouseDragged:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    DockModel *model = self.model;

    if (_pressed < 0)
        return;
    if (!_dragging && hypot(p.x - _downPoint.x, p.y - _downPoint.y) < kDragThreshold)
        return;
    if (!_dragging) {
        _dragging = YES;
        [_pressTimer invalidate];
        _pressTimer = nil;
    }
    _dragPoint = p;
    /* Off the Dock (beyond a tile's size from it): removed, if kept. */
    CGFloat cross = [self crossOfPoint:p];
    _dropRemoves = cross > [DockView barThicknessForModel:model] + model.tileSize ||
                   cross < -model.tileSize;
    /* Where it would go among the kept tiles. */
    NSUInteger kept = model.firstRunningIndex, index = 0;
    CGFloat main = [self mainOfPoint:p];
    for (NSUInteger i = 0; i < kept; i++) {
        NSRect r = [self rectForTileAtIndex:i];
        CGFloat center = [self orientation] == DockOrientationBottom ? NSMidX(r) : NSHeight([self bounds]) - NSMidY(r);
        if (main > center)
            index = i + 1;
    }
    _dropIndex = (NSInteger)index;
    if (_dragWindow == nil)
        _dragWindow = [[DockDragWindow alloc] init];
    NSPoint onScreen = [[self window] convertBaseToScreen:[event locationInWindow]];
    [_dragWindow showIcon:model.tiles[(NSUInteger)_pressed].icon size:model.tileSize at:onScreen
                 removing:_dropRemoves && [model isKept:model.tiles[(NSUInteger)_pressed]]];
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    DockModel *model = self.model;
    NSArray<DockTile *> *tiles = model.tiles;

    [_pressTimer invalidate];
    _pressTimer = nil;
    if (_pressed >= 0 && (NSUInteger)_pressed < [tiles count]) {
        DockTile *tile = tiles[(NSUInteger)_pressed];
        if (_dragging) {
            if (_dropRemoves) {
                if ([model isKept:tile])
                    [model removeTile:tile];
            } else {
                /* Dropped in the kept part, or moved within it. */
                CGFloat main = [self mainOfPoint:p];
                BOOL inKept = model.firstRunningIndex == 0 || [tiles count] == model.firstRunningIndex;
                if (!inKept) {
                    NSRect sep = [self rectForTileAtIndex:model.firstRunningIndex];
                    CGFloat start = [self orientation] == DockOrientationBottom ? NSMinX(sep) : NSHeight([self bounds]) - NSMaxY(sep);
                    inKept = main < start;
                }
                if (inKept || [model isKept:tile])
                    [model keepTile:tile atIndex:(NSUInteger)_dropIndex];
            }
        } else if ([self tileIndexAtPoint:p] == _pressed) {
            [model openTile:tile];
        }
    }
    [_dragWindow orderOut:nil];
    _pressed = -1;
    _dragging = NO;
    _dropRemoves = NO;
    [self setNeedsDisplay:YES];
}

/* -- The tile's menu -- */

- (void)showMenuForTile:(NSUInteger)index event:(NSEvent *)event
{
    DockModel *model = self.model;
    DockTile *tile = model.tiles[index];
    NSMenu *menu = [[NSMenu alloc] initWithTitle:tile.label];
    NSMenu *options = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Options", nil)];
    NSMenuItem *item;

    [_pressTimer invalidate];
    _pressTimer = nil;
    [menu setAutoenablesItems:NO];
    [options setAutoenablesItems:NO];

    item = [options addItemWithTitle:NSLocalizedString(@"Keep in Dock", nil) action:@selector(toggleKeep:) keyEquivalent:@""];
    [item setTarget:self];
    [item setRepresentedObject:tile];
    [item setState:[model isKept:tile] ? NSOnState : NSOffState];
    item = [options addItemWithTitle:NSLocalizedString(@"Open at Login", nil) action:NULL keyEquivalent:@""];
    [item setEnabled:NO];           /* with the login items (MB3) */
    item = [options addItemWithTitle:NSLocalizedString(@"Show in Argos", nil) action:@selector(showInArgos:) keyEquivalent:@""];
    [item setTarget:self];
    [item setRepresentedObject:tile];
    [item setEnabled:tile.URL != nil];
    item = [menu addItemWithTitle:NSLocalizedString(@"Options", nil) action:NULL keyEquivalent:@""];
    [menu setSubmenu:options forItem:item];
    [menu addItem:[NSMenuItem separatorItem]];

    if (tile.application != nil) {
        item = [menu addItemWithTitle:NSLocalizedString(@"Show All Windows", nil) action:@selector(openTileItem:) keyEquivalent:@""];
        [item setTarget:self];
        [item setRepresentedObject:tile];
        [menu addItem:[NSMenuItem separatorItem]];
        item = [menu addItemWithTitle:NSLocalizedString(@"Quit", nil) action:@selector(quitTileItem:) keyEquivalent:@""];
        [item setTarget:self];
        [item setRepresentedObject:tile];
    } else {
        item = [menu addItemWithTitle:NSLocalizedString(@"Open", nil) action:@selector(openTileItem:) keyEquivalent:@""];
        [item setTarget:self];
        [item setRepresentedObject:tile];
    }
    [NSMenu popUpContextMenu:menu withEvent:event forView:self];
    _pressed = -1;
    [self setNeedsDisplay:YES];
}

- (void)toggleKeep:(NSMenuItem *)sender
{
    DockTile *tile = [sender representedObject];

    if ([self.model isKept:tile])
        [self.model removeTile:tile];
    else
        [self.model keepTile:tile atIndex:self.model.firstRunningIndex];
}

- (void)openTileItem:(NSMenuItem *)sender
{
    [self.model openTile:[sender representedObject]];
}

/* The application selected in a window on its folder (LSRevealInArgos). */
- (void)showInArgos:(NSMenuItem *)sender
{
    NSURL *url = [(DockTile *)[sender representedObject] URL];

    if (url != nil)
        [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[ url ]];
}

- (void)quitTileItem:(NSMenuItem *)sender
{
    [[[sender representedObject] application] terminate];
}

@end

/* ---- The edge the pointer reaches to show a hidden Dock ---- */

@interface DockTriggerWindow : NSWindow
@property (nonatomic, weak) DockWindow *dockWindow;
@end

@interface DockTriggerView : NSView
@property (nonatomic, weak) DockWindow *dockWindow;
@end

@implementation DockTriggerView

- (BOOL)isOpaque
{
    return NO;
}

- (void)drawRect:(NSRect)dirty
{
    [[NSColor clearColor] set];
    NSRectFill([self bounds]);
}

- (void)mouseMoved:(NSEvent *)event
{
    [self.dockWindow pointerInside:YES];
}

- (void)mouseEntered:(NSEvent *)event
{
    [self.dockWindow pointerInside:YES];
}

- (void)updateTrackingAreas
{
    for (NSTrackingArea *area in [[self trackingAreas] copy])
        [self removeTrackingArea:area];
    [self addTrackingArea:[[NSTrackingArea alloc] initWithRect:[self bounds]
        options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveAlways |
                NSTrackingInVisibleRect owner:self userInfo:nil]];
    [super updateTrackingAreas];
}

@end

@implementation DockTriggerWindow

- (BOOL)canBecomeKeyWindow
{
    return NO;
}

@end

/* ---- The window ---- */

@implementation DockWindow {
    DockModel *_model;
    DockView *_view;
    DockLabelWindow *_label;
    DockTriggerWindow *_trigger;
    BOOL _shown;              /* autohide: on screen now */
    NSTimer *_slideTimer, *_hideTimer;
}

- (instancetype)initWithModel:(DockModel *)model
{
    self = [super initWithContentRect:NSMakeRect(0, 0, 100, 72)
                            styleMask:NSBorderlessWindowMask
                              backing:NSBackingStoreBuffered
                                defer:NO];
    if (self == nil)
        return nil;
    _model = model;
    _view = [[DockView alloc] initWithFrame:NSMakeRect(0, 0, 100, 72)];
    _view.model = model;
    _view.dockWindow = self;
    [_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self setContentView:_view];
    [self setLevel:kCGDockWindowLevelKey];
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setHasShadow:NO];
    [self setHidesOnDeactivate:NO];
    [self setExcludedFromWindowsMenu:YES];
    [self setReleasedWhenClosed:NO];
    [self setAcceptsMouseMovedEvents:YES];
    _label = [[DockLabelWindow alloc] init];
    _shown = YES;
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

- (DockLabelWindow *)label
{
    return _label;
}

/* Its place on screen, shown or slid off its edge. */
- (NSRect)frameShown:(BOOL)shown
{
    NSScreen *screen = [[NSScreen screens] firstObject];
    NSRect full = [screen frame], visible = [screen visibleFrame];
    CGFloat length = [DockView windowLengthForModel:_model];
    CGFloat thickness = [DockView windowThicknessForModel:_model];
    CGFloat hidden = shown ? 0 : thickness + kEdgeGap;
    NSRect frame;

    switch (_model.orientation) {
    case DockOrientationLeft:
        frame = NSMakeRect(NSMinX(full) + kEdgeGap - hidden, NSMidY(visible) - length / 2, thickness, length);
        break;
    case DockOrientationRight:
        frame = NSMakeRect(NSMaxX(full) - kEdgeGap - thickness + hidden, NSMidY(visible) - length / 2,
                           thickness, length);
        break;
    default:
        frame = NSMakeRect(NSMidX(full) - length / 2, NSMinY(full) + kEdgeGap - hidden, length, thickness);
        break;
    }
    return NSIntegralRect(frame);
}

/* The strip along the edge that brings a hidden Dock back. */
- (NSRect)triggerFrame
{
    NSRect shown = [self frameShown:YES], full = [[[NSScreen screens] firstObject] frame];

    switch (_model.orientation) {
    case DockOrientationLeft: return NSMakeRect(NSMinX(full), NSMinY(shown), 2, NSHeight(shown));
    case DockOrientationRight: return NSMakeRect(NSMaxX(full) - 2, NSMinY(shown), 2, NSHeight(shown));
    default: return NSMakeRect(NSMinX(shown), NSMinY(full), NSWidth(shown), 2);
    }
}

- (void)layout
{
    if (!_model.autohide)
        _shown = YES;
    NSRect frame = [self frameShown:_shown];
    if (!NSEqualRects(frame, [self frame]))
        [self setFrame:frame display:YES];
    [_view setNeedsDisplay:YES];

    if (_model.autohide) {
        if (_trigger == nil) {
            _trigger = [[DockTriggerWindow alloc] initWithContentRect:[self triggerFrame]
                styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
            DockTriggerView *view = [[DockTriggerView alloc] initWithFrame:NSMakeRect(0, 0, 10, 10)];
            view.dockWindow = self;
            [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
            [_trigger setContentView:view];
            [_trigger setLevel:kCGDockWindowLevelKey];
            [_trigger setOpaque:NO];
            [_trigger setBackgroundColor:[NSColor clearColor]];
            [_trigger setHasShadow:NO];
            [_trigger setHidesOnDeactivate:NO];
            [_trigger setReleasedWhenClosed:NO];
            [_trigger setAcceptsMouseMovedEvents:YES];
        }
        [_trigger setFrame:[self triggerFrame] display:NO];
        [_trigger orderFront:nil];
        /* Turned on with the pointer elsewhere: it hides now. */
        if (_shown && !NSPointInRect([NSEvent mouseLocation], [self frame]))
            [self pointerInside:NO];
    } else {
        [_trigger orderOut:nil];
    }
}

- (void)modelChanged:(NSNotification *)note
{
    [self layout];
}

/* Autohide: the pointer on the Dock or its edge shows it; leaving hides
   it after a moment. */
- (void)pointerInside:(BOOL)inside
{
    if (!_model.autohide)
        return;
    [_hideTimer invalidate];
    _hideTimer = nil;
    if (inside) {
        if (!_shown)
            [self slideShown:YES];
    } else if (_shown) {
        _hideTimer = [NSTimer scheduledTimerWithTimeInterval:kAutohideDelay repeats:NO block:^(NSTimer *t) {
            (void)t;
            [self slideShown:NO];
        }];
    }
}

- (void)slideShown:(BOOL)shown
{
    NSRect from = [self frame], to = [self frameShown:shown];
    NSDate *start = [NSDate date];

    _shown = shown;
    [_slideTimer invalidate];
    if (!shown)
        [_label orderOut:nil];
    _slideTimer = [NSTimer scheduledTimerWithTimeInterval:1.0 / 60 repeats:YES block:^(NSTimer *timer) {
        CGFloat t = MIN([[NSDate date] timeIntervalSinceDate:start] / kSlideTime, 1);
        CGFloat e = t * t * (3 - 2 * t);
        NSRect f = NSMakeRect(NSMinX(from) + (NSMinX(to) - NSMinX(from)) * e,
                              NSMinY(from) + (NSMinY(to) - NSMinY(from)) * e,
                              NSWidth(to), NSHeight(to));
        [self setFrame:NSIntegralRect(f) display:YES];
        if (t >= 1) {
            [timer invalidate];
            self->_slideTimer = nil;
        }
    }];
}

@end
