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
 * It draws only when the model changes or the pointer acts on it. With
 * magnification it redraws at most once a display refresh, only when the
 * tiles' sizes change and only over the bar, from icons kept ready at the
 * tile size and at the largest size.
 *
 * Atlas's provisional look: the layout follows the reference platform's
 * Dock; sizes and colors are Atlas's own and will change with its visual
 * language guide.
 */

#import "DockView.h"
#import "DockSwitcher.h"
#include <SkyLight/SkyLight.h>
#include <os/log.h>
#include <LaunchServices/LaunchServices.h>

static LSSharedFileListItemRef LoginItemForURL(NSURL *url);
#include <math.h>

static const CGFloat kEdgeGap = 4;          /* between the bar and the edge */
static const NSTimeInterval kLongPress = 0.6;
static const CGFloat kDragThreshold = 4;
static const NSTimeInterval kSlideTime = 0.2;
static const NSTimeInterval kAutohideDelay = 0.5;  /* autohide-delay's default */
static const NSTimeInterval kFrameTime = 1.0 / 60;  /* the fastest the Dock redraws */
static const CGFloat kMagnifyStep = 0.5;            /* pointer motion worth a redraw */

/* ---- Geometry along the Dock (main axis) and away from its edge (cross axis) ---- */

typedef struct {
    CGFloat main, cross, mainLength, crossLength;
} DockSpan;

/* The bar's sizes for a tile size. */
/* The icons centred across the bar, the same margin on either side; an
   open application's dot sits in the margin at the bottom, with no room
   of its own (the maintainer's reference, 23 Nov 2026: a 48-point icon in
   a bar about 66 points high). */
static CGFloat padding(CGFloat tile) { return round(tile * 3 / 16); }
static CGFloat gapFor(CGFloat tile) { return round(tile / 6); }
static CGFloat indicatorSpace(CGFloat tile) { (void)tile; return 0; }
static CGFloat separatorSpace(CGFloat tile) { return round(tile / 4); }

/* How high a tile bouncing for attention is now: half a tile at the top
   of each 0.6-second bounce (D3). */
static const CFAbsoluteTime kBouncePeriod = 0.6;

static CGFloat bounceHeight(DockTile *tile, CGFloat size)
{
    if (tile.bounceStart == 0)
        return 0;
    CFAbsoluteTime t = CFAbsoluteTimeGetCurrent() - tile.bounceStart;
    if (!tile.bounceCritical && !tile.bounceLaunch && t >= 2 * kBouncePeriod)
        return 0;
    return round(size * 0.5 * fabs(sin(M_PI * fmod(t, kBouncePeriod) / kBouncePeriod)));
}

/* A separator goes before the first running-only tile and before the
   minimized windows and the Trash. */
static BOOL separatorBefore(DockModel *model, NSUInteger i)
{
    NSUInteger first = model.firstRunningIndex, minimized = model.minimizedIndex;

    /* The minimized windows share the Trash's section. */
    if (i == 0)
        return NO;
    return (i == first && first < minimized) || (i == minimized && minimized < [model.tiles count]);
}

static NSUInteger separatorCount(DockModel *model)
{
    NSUInteger n = 0, count = [model.tiles count];

    for (NSUInteger i = 0; i < count; i++)
        n += separatorBefore(model, i);
    return n;
}

/* ---- The tile's name, in a small window beside it ---- */

@interface DockLabelView : NSView
@property (nonatomic, copy) NSString *text;
@end

@implementation DockLabelView

+ (NSDictionary *)attributes
{
    static NSDictionary *attributes;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        attributes = @{ NSFontAttributeName: [NSFont systemFontOfSize:13],
                        NSForegroundColorAttributeName: [NSColor whiteColor] };
    });
    return attributes;
}

- (BOOL)isOpaque
{
    return NO;
}

- (void)drawRect:(NSRect)dirty
{
    [[NSColor clearColor] set];
    NSRectFill([self bounds]);
    [[NSColor colorWithGenericGamma22White:0.12 alpha:0.85] setFill];
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
        [self setLevel:CGWindowLevelForKey(kCGDockWindowLevelKey)];
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
    BOOL same = [self isVisible] && [text isEqualToString:_view.text];
    NSSize size = same ? NSMakeSize(NSWidth([self frame]) - 20, NSHeight([self frame]) - 8)
                       : [text sizeWithAttributes:[DockLabelView attributes]];
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
    if (same) {
        /* The same name beside a tile that moved (magnified): moved, not drawn again. */
        if (!NSEqualPoints(frame.origin, [self frame].origin))
            [self setFrameOrigin:frame.origin];
        return;
    }
    _view.text = text;
    /* Drawn with its new frame: the server shows the new name at once,
       not the old one stretched into the new frame for a moment. */
    [_view setNeedsDisplay:YES];
    [self setFrame:frame display:YES];
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
        [self setLevel:CGWindowLevelForKey(kCGDraggingWindowLevelKey)];
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
- (void)forgetIcons;
- (void)forgetPointerUnlessInside;
/* The tiles bouncing now, each as high as it is in its bounce. */
- (void)drawBouncingTiles;
@end

@interface DockWindow ()
- (void)pointerInside:(BOOL)inside;
- (void)restoreMinimizedTile:(DockTile *)tile;
- (DockLabelWindow *)label;
@end

@implementation DockView {
    NSInteger _pressed;       /* the tile under a pressed button, or -1 */
    NSInteger _dropTile;      /* the tile files dragged from elsewhere would go to, or -1 */
    NSPoint _downPoint;
    BOOL _dragging;
    NSInteger _dropIndex;     /* where a dragged tile goes among the kept ones */
    BOOL _dropRemoves;        /* dragged off the Dock */
    NSPoint _dragPoint;
    NSTimer *_pressTimer;
    NSTimer *_pointerCheck;   /* while the pointer is on the Dock */
    NSTrackingArea *_tracking;
    DockDragWindow *_dragWindow;
    NSInteger _labelled;      /* the tile whose label shows, or -1 */
    NSMutableDictionary *_icons;  /* [icon, icon drawn at a size], by icon and pixels */
    CGFloat _drawnMain;       /* the pointer the last drawing magnified for */
    NSRect _drawnBar;         /* where the bar and its tiles were drawn */
    CFAbsoluteTime _lastFrame;
    BOOL _framePending;
    NSImage *_barImage;       /* a short bar, its ends and middle copied to draw it */
    BOOL _barDark;            /* the appearance it was drawn in */
    DockOrientation _barOrientation;
}

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) != nil) {
        _pressed = -1;
        _dropTile = -1;
        _labelled = -1;
        [self registerForDraggedTypes:@[ NSFilenamesPboardType, NSPasteboardTypeFileURL ]];
        _pointerMain = NAN;
        _drawnMain = NAN;
        _icons = [NSMutableDictionary dictionary];
        /* The system's colors changed (an appearance theme): the bar
           drawn again in them. */
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(systemColorsChanged:)
                                                     name:NSSystemColorsDidChangeNotification object:nil];
    }
    return self;
}

/* -- Icons, ready at the sizes drawn -- */

- (void)systemColorsChanged:(NSNotification *)note
{
    _barImage = nil;
    [self setNeedsDisplay:YES];
}

- (void)forgetIcons
{
    [_icons removeAllObjects];
}

/* The icon drawn once at size points, so that drawing it (or shrinking it
   a little, while magnified) does not scale its largest picture each time. */
- (NSImage *)icon:(NSImage *)icon atSize:(CGFloat)size
{
    CGFloat scale = [[self window] backingScaleFactor] > 0 ? [[self window] backingScaleFactor] : 1;
    NSInteger pixels = (NSInteger)ceil(size * scale);
    NSString *key = [NSString stringWithFormat:@"%p-%ld", (__bridge void *)icon, (long)pixels];
    /* The entry holds the icon too: its address, in the key, is not reused
       while the entry lasts. */
    NSImage *ready = [_icons[key] lastObject];

    if (ready != nil || icon == nil || pixels <= 0)
        return ready;
    ready = [[NSImage alloc] initWithSize:NSMakeSize(size, size)];
    [ready lockFocus];
    [icon drawInRect:NSMakeRect(0, 0, size, size) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1];
    [ready unlockFocus];
    if ([_icons count] > 1024)  /* every size of every tile, magnified */
        [_icons removeAllObjects];
    _icons[key] = @[ icon, ready ];
    return ready;
}

/* What to draw a tile's icon from at a size: the icon drawn once at that
   very size, so that drawing it is a copy rather than a resampling. */
- (NSImage *)drawableIcon:(NSImage *)icon forSize:(CGFloat)size
{
    return icon != nil ? [self icon:icon atSize:size] : nil;
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

    length += separatorCount(model) * separatorSpace(tile);
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

/* Room above the bar while a tile bounces (D3): a window of its own
   beyond the Dock's (DockBounceWindow), so the Dock's never changes its
   size, and so never moves, while a tile bounces. */
static CGFloat bounceRoom;

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
    /* Whole points: each size's icon is drawn once and then copied. */
    return round(tile + (large - tile) * (cos(distance * M_PI / 3) + 1) / 2);
}

/* The tiles' places along the Dock, in the order shown (a dragged tile
   leaves a gap where it would go). */
- (void)layoutMains:(CGFloat *)mains sizes:(CGFloat *)sizes count:(NSUInteger)count
         barStart:(CGFloat *)barStart barLength:(CGFloat *)barLength
{
    DockModel *model = _model;
    CGFloat tile = model.tileSize, gap = gapFor(tile), sep = separatorSpace(tile);
    CGFloat base = [DockView baseLengthForModel:model];
    CGFloat baseStart = round(([self mainLength] - base) / 2);
    CGFloat x, total;

    /* The base centers, to measure the pointer against. */
    x = baseStart + padding(tile);
    total = 0;
    for (NSUInteger i = 0; i < count; i++) {
        if (separatorBefore(model, i))
            x += sep;
        sizes[i] = [self sizeOfTileAtIndex:i baseCenter:x + tile / 2];
        x += tile + gap;
        total += sizes[i];
    }
    total += 2 * padding(tile) + (count ? count - 1 : 0) * gap;
    total += separatorCount(model) * sep;
    total = MAX(total, 2 * padding(tile) + tile);

    *barLength = total;
    *barStart = round(([self mainLength] - total) / 2);
    x = *barStart + padding(tile);
    for (NSUInteger i = 0; i < count; i++) {
        if (separatorBefore(model, i))
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

static const CGFloat kBarRadius = 14;
static const CGFloat kBarCap = 16;      /* a rounded end, its edge included */
static const CGFloat kBarMiddle = 128;  /* a piece of the straight middle, repeated */

/* The appearance chosen in General (System Preferences): the Dock is light
   or dark with it, as the reference's is. */
static BOOL dockDark(void)
{
    return [[[NSApp effectiveAppearance] name] isEqualToString:NSAppearanceNameDarkAqua];
}

/* The bar's rounded rectangle at its colors: pale and see-through in the
   light, charcoal and see-through in the dark. */
static void drawBarPath(NSRect rect)
{
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(rect, 0.5, 0.5)
                                                         xRadius:kBarRadius yRadius:kBarRadius];
    BOOL dark = dockDark();
    extern BOOL _NSAppearanceThemeActive(void);
    /* An appearance theme: its window color, see-through, and its lines. */
    if (_NSAppearanceThemeActive()) {
        [[[NSColor windowBackgroundColor] colorWithAlphaComponent:0.78] setFill];
        [path fill];
        [[NSColor separatorColor] setStroke];
        [path setLineWidth:1];
        [path stroke];
        return;
    }
    [[NSColor colorWithGenericGamma22White:dark ? 0.30 : 0.96 alpha:dark ? 0.72 : 0.55] setFill];
    [path fill];
    [[NSColor colorWithGenericGamma22White:dark ? 0.12 : 1.0 alpha:dark ? 0.55 : 0.6] setStroke];
    [path setLineWidth:1];
    [path stroke];
}

/* A span in an image of the given size, as rectForSpan: places it in the
   view (the main axis runs down the image on a side). */
- (NSRect)rectForSpan:(DockSpan)s inSize:(NSSize)size
{
    if ([self orientation] == DockOrientationBottom)
        return NSMakeRect(s.main, s.cross, s.mainLength, s.crossLength);
    return NSMakeRect([self orientation] == DockOrientationLeft ? s.cross : size.width - s.cross - s.crossLength,
                      size.height - s.main - s.mainLength, s.crossLength, s.mainLength);
}

/* The bar, from a short one drawn once: its two ends and pieces of its
   middle copied as they are, rather than a rounded rectangle filled and
   stroked at every frame (the most of a magnified redraw). */
- (void)drawBarFrom:(CGFloat)start length:(CGFloat)length thickness:(CGFloat)thickness
{
    CGFloat shortLength = 2 * kBarCap + kBarMiddle;
    BOOL bottom = [self orientation] == DockOrientationBottom;
    NSSize size = bottom ? NSMakeSize(shortLength, thickness) : NSMakeSize(thickness, shortLength);

    if (length < shortLength) {
        drawBarPath([self rectForSpan:(DockSpan){ start, 0, length, thickness }]);
        return;
    }
    if (_barImage == nil || !NSEqualSizes([_barImage size], size) || _barOrientation != [self orientation] ||
        _barDark != dockDark()) {
        _barImage = [[NSImage alloc] initWithSize:size];
        _barOrientation = [self orientation];
        _barDark = dockDark();
        [_barImage lockFocus];
        drawBarPath(NSMakeRect(0, 0, size.width, size.height));
        [_barImage unlockFocus];
    }
    /* Ends and middle pieces at their own size: copies, with no resampling. */
    [self drawBarPiece:(DockSpan){ start, 0, kBarCap, thickness } from:0 size:size];
    for (CGFloat at = kBarCap; at < length - kBarCap; at += kBarMiddle) {
        CGFloat piece = MIN(kBarMiddle, length - kBarCap - at);
        [self drawBarPiece:(DockSpan){ start + at, 0, piece, thickness } from:kBarCap size:size];
    }
    [self drawBarPiece:(DockSpan){ start + length - kBarCap, 0, kBarCap, thickness }
                  from:kBarCap + kBarMiddle size:size];
}

- (void)drawBarPiece:(DockSpan)span from:(CGFloat)main size:(NSSize)size
{
    DockSpan source = { main, 0, span.mainLength, span.crossLength };

    [_barImage drawInRect:[self rectForSpan:span] fromRect:[self rectForSpan:source inSize:size]
                operation:NSCompositeSourceOver fraction:1];
}

/* An app's badge (Atlas.md, 5.6, N2): white on a red pill at the icon's
   top right, a third of the icon high. */
static void drawBadge(NSString *label, NSRect icon)
{
    CGFloat height = MAX(14, round(NSHeight(icon) * 0.36));
    NSFont *font = [NSFont boldSystemFontOfSize:round(height * 0.62)];
    NSDictionary *attributes = @{ NSFontAttributeName : font, NSForegroundColorAttributeName : [NSColor whiteColor] };
    NSSize text = [label sizeWithAttributes:attributes];
    CGFloat width = MAX(height, ceil(text.width) + height * 0.6);
    NSRect pill = NSMakeRect(NSMaxX(icon) - width + height * 0.25, NSMaxY(icon) - height + height * 0.15, width, height);
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:pill xRadius:height / 2 yRadius:height / 2];

    [[NSColor colorWithSRGBRed:0.86 green:0.16 blue:0.14 alpha:1] setFill];
    [path fill];
    [[NSColor colorWithGenericGamma22White:1 alpha:0.9] setStroke];
    [path setLineWidth:1];
    [path stroke];
    [label drawAtPoint:NSMakePoint(NSMidX(pill) - text.width / 2, NSMidY(pill) - text.height / 2) withAttributes:attributes];
}

- (void)drawRect:(NSRect)dirty
{
    DockModel *model = _model;
    NSArray<DockTile *> *tiles = model.tiles;
    NSUInteger count = [tiles count];
    CGFloat mains[count + 1], sizes[count + 1], start, length;
    CGFloat tile = model.tileSize, ind = indicatorSpace(tile), pad = padding(tile);

    [[NSColor clearColor] set];
    NSRectFill(dirty);
    [self layoutMains:mains sizes:sizes count:count barStart:&start barLength:&length];
    _drawnMain = self.pointerMain;
    _lastFrame = CFAbsoluteTimeGetCurrent();
    {
        /* What is painted now: the bar drawn, and what of an earlier one
           this drawing did not cover (to clear on the next). */
        DockSpan all = { start, 0, length, [DockView windowThicknessForModel:model] };
        NSRect now = [self rectForSpan:all];
        _drawnBar = NSContainsRect(dirty, _drawnBar) ? now : NSUnionRect(_drawnBar, now);
    }

    [self drawBarFrom:start length:length thickness:[DockView barThicknessForModel:model]];

    for (NSUInteger i = 0; i < count; i++) {
        DockTile *t = tiles[i];
        DockSpan s = { mains[i], pad + ind + bounceHeight(t, sizes[i]), sizes[i], sizes[i] };
        NSRect rect = [self rectForSpan:s];
        BOOL lifted = _dragging && (NSInteger)i == _pressed;

        if (!NSIntersectsRect(rect, dirty))
            continue;
        if (!lifted && t.icon != nil)
            [[self drawableIcon:(t.contentImage ?: t.icon) forSize:sizes[i]] drawInRect:rect fromRect:NSZeroRect
                operation:NSCompositeSourceOver fraction:(NSInteger)i == _pressed || (NSInteger)i == _dropTile ? 0.6 : 1.0];
        NSString *badge = lifted ? nil : [model badgeForTile:t];
        if (badge != nil)
            drawBadge(badge, rect);
        if (t.application != nil && model.showIndicators) {
            DockSpan d = { mains[i] + sizes[i] / 2 - 2, pad / 2 - 2, 4, 4 };	/* in the bottom margin */
            extern BOOL _NSAppearanceThemeActive(void);
            [(_NSAppearanceThemeActive() ? [NSColor labelColor]
              : [NSColor colorWithGenericGamma22White:dockDark() ? 0.92 : 0.15 alpha:0.85]) setFill];
            [[NSBezierPath bezierPathWithOvalInRect:[self rectForSpan:d]] fill];
        }
    }

    for (NSUInteger i = 1; i < count; i++) {
        if (!separatorBefore(model, i))
            continue;
        CGFloat at = mains[i - 1] + sizes[i - 1] + (gapFor(tile) + separatorSpace(tile)) / 2;
        DockSpan s = { floor(at), pad, 1, [DockView barThicknessForModel:model] - 2 * pad };
        extern BOOL _NSAppearanceThemeActive(void);
        [(_NSAppearanceThemeActive() ? [[NSColor labelColor] colorWithAlphaComponent:0.35]
          : [NSColor colorWithGenericGamma22White:dockDark() ? 1.0 : 0.2 alpha:0.35]) setFill];
        NSRectFill([self rectForSpan:s]);
    }
}

- (void)drawBouncingTiles
{
    DockModel *model = _model;
    NSArray<DockTile *> *tiles = model.tiles;
    NSUInteger count = [tiles count];
    CGFloat mains[count + 1], sizes[count + 1], start, length;
    CGFloat pad = padding(model.tileSize), ind = indicatorSpace(model.tileSize);

    [self layoutMains:mains sizes:sizes count:count barStart:&start barLength:&length];
    for (NSUInteger i = 0; i < count; i++) {
        DockTile *t = tiles[i];
        CGFloat up = bounceHeight(t, sizes[i]);
        if (up <= 0 || t.icon == nil || (_dragging && (NSInteger)i == _pressed))
            continue;
        DockSpan s = { mains[i], pad + ind + up, sizes[i], sizes[i] };
        [[self drawableIcon:(t.contentImage ?: t.icon) forSize:sizes[i]] drawInRect:[self rectForSpan:s]
            fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0];
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
    _labelled = index;
    NSRect rect = [self rectForTileAtIndex:(NSUInteger)index];
    /* Magnified: the name stays above the largest a tile grows to, not on
       the tile as large as it is now (which it covered as it grew). */
    if (self.model.magnification) {
        CGFloat large = MAX(self.model.largeSize, self.model.tileSize);
        switch ([self orientation]) {
        case DockOrientationLeft: rect.size.width = MAX(NSWidth(rect), large); break;
        case DockOrientationRight: rect.origin.x = NSMaxX(rect) - MAX(NSWidth(rect), large);
                                   rect.size.width = MAX(NSWidth(rect), large); break;
        default: rect.size.height = MAX(NSHeight(rect), large); break;
        }
    }
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
        [self magnifiedNeedsDisplay];
    [self showLabelForPoint:p];
    /* An exit may never come (a pointer leaving fast while another app is
       active): where the pointer is, checked until it is off the Dock. */
    if (_pointerCheck == nil)
        _pointerCheck = [NSTimer scheduledTimerWithTimeInterval:0.1 repeats:YES block:^(NSTimer *t) {
            (void)t;
            [self checkPointerStillInside];
        }];
}

- (void)checkPointerStillInside
{
    NSPoint screen = [NSEvent mouseLocation];
    NSPoint p = [self convertPoint:[[self window] convertRectFromScreen:NSMakeRect(screen.x, screen.y, 0, 0)].origin
                          fromView:nil];

    if (isnan(self.pointerMain)) {
        [_pointerCheck invalidate];
        _pointerCheck = nil;
    } else if (!NSPointInRect(p, [self bounds]))
        [self mouseExited:nil];
}

/* The bar and its tiles where they will be drawn now. */
- (NSRect)barRectNow
{
    NSUInteger count = [self.model.tiles count];
    CGFloat mains[count + 1], sizes[count + 1], start, length;

    [self layoutMains:mains sizes:sizes count:count barStart:&start barLength:&length];
    DockSpan all = { start, 0, length, [DockView windowThicknessForModel:self.model] };
    return [self rectForSpan:all];
}

/* With magnification: a redraw when the pointer moved enough to change
   the tiles' sizes, over the bar as it was and as it will be (on whole
   pixels: a clip that is not costs a coverage mask), and no more often
   than the display refreshes. */
- (void)magnifiedNeedsDisplay
{
    CGFloat now = self.pointerMain;
    CFAbsoluteTime wait;

    if (isnan(now) != isnan(_drawnMain))
        ;   /* entering or leaving: always */
    else if (isnan(now) || fabs(now - _drawnMain) < kMagnifyStep)
        return;
    if (_framePending)
        return;
    wait = kFrameTime - (CFAbsoluteTimeGetCurrent() - _lastFrame);
    if (wait <= 0) {
        [self setNeedsDisplayInRect:NSIntegralRect(NSUnionRect(_drawnBar, [self barRectNow]))];
        return;
    }
    _framePending = YES;
    [self performSelector:@selector(magnifiedFrame) withObject:nil afterDelay:wait
                  inModes:@[NSRunLoopCommonModes]];
}

- (void)magnifiedFrame
{
    _framePending = NO;
    [self setNeedsDisplayInRect:NSIntegralRect(NSUnionRect(_drawnBar, [self barRectNow]))];
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
        [self magnifiedNeedsDisplay];
}

/* Where the pointer really is: a pointer that left fast, while another
   app was active, may have sent no exit, and the place it was last seen
   stayed (magnification turned on then magnified the tiles there). */
- (void)forgetPointerUnlessInside
{
    NSPoint screen = [NSEvent mouseLocation];
    NSPoint p = [self convertPoint:[[self window] convertRectFromScreen:NSMakeRect(screen.x, screen.y, 0, 0)].origin
                          fromView:nil];

    if (isnan(self.pointerMain) || NSPointInRect(p, [self bounds]))
        return;
    [self mouseExited:nil];
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
    DockModel *model = _model;

    /* The minimized windows and the Trash stay where they are. */
    if (_pressed < 0 || (NSUInteger)_pressed >= model.minimizedIndex)
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
    DockModel *model = _model;
    NSArray<DockTile *> *tiles = model.tiles;

    [_pressTimer invalidate];
    _pressTimer = nil;
    if (_pressed >= 0 && (NSUInteger)_pressed < [tiles count]) {
        DockTile *tile = tiles[(NSUInteger)_pressed];
        if (_dragging) {
            if (_dropRemoves || [self tileIndexAtPoint:p] == (NSInteger)model.trashIndex) {
                /* Off the Dock, or onto the Trash: no longer kept. */
                if ([model isKept:tile])
                    [model removeTile:tile];
            } else {
                /* Dropped in the kept part, or moved within it. */
                CGFloat main = [self mainOfPoint:p];
                BOOL inKept = model.firstRunningIndex == 0 || model.minimizedIndex == model.firstRunningIndex;
                if (!inKept) {
                    NSRect sep = [self rectForTileAtIndex:model.firstRunningIndex];
                    CGFloat start = [self orientation] == DockOrientationBottom ? NSMinX(sep) : NSHeight([self bounds]) - NSMaxY(sep);
                    inKept = main < start;
                }
                if (inKept || [model isKept:tile])
                    [model keepTile:tile atIndex:(NSUInteger)_dropIndex];
            }
        } else if ([self tileIndexAtPoint:p] == _pressed) {
            if (tile.kind == DockTileKindMinimizedWindow)
                [self.dockWindow restoreMinimizedTile:tile];
            else
                [model openTile:tile];
        }
    }
    [_dragWindow orderOut:nil];
    _pressed = -1;
    _dragging = NO;
    _dropRemoves = NO;
    [self setNeedsDisplay:YES];
}

/* -- Files dragged onto the Dock (from Files or another app) -- */

static NSArray<NSURL *> *DraggedFileURLs(id<NSDraggingInfo> info)
{
    NSMutableArray *urls = [NSMutableArray array];
    id paths = [[info draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    if ([paths isKindOfClass:[NSArray class]])
        for (id p in paths)
            if ([p isKindOfClass:[NSString class]])
                [urls addObject:[NSURL fileURLWithPath:p]];
    return urls;
}

static BOOL AllApplications(NSArray<NSURL *> *urls)
{
    for (NSURL *u in urls)
        if (![[u pathExtension] isEqualToString:@"app"])
            return NO;
    return [urls count] > 0;
}

/* What a drop at the pointer would do: onto the Trash, to the Trash; onto
 * an app's tile, open the files with it; applications onto the kept
 * part, kept in the Dock. The tile it would go to is shown dimmed. */
- (NSDragOperation)dropOperationFor:(id<NSDraggingInfo>)info
{
    DockModel *model = _model;
    NSArray<NSURL *> *urls = DraggedFileURLs(info);
    NSInteger index = [self tileIndexAtPoint:[self convertPoint:[info draggingLocation] fromView:nil]];
    NSDragOperation operation = NSDragOperationNone;
    NSInteger tile = -1;

    if ([urls count] > 0 && index >= 0) {
        DockTile *t = model.tiles[(NSUInteger)index];
        if (t.kind == DockTileKindTrash) {
            operation = NSDragOperationMove | NSDragOperationDelete;
            tile = index;
        } else if (t.kind != DockTileKindMinimizedWindow && !AllApplications(urls)) {
            operation = NSDragOperationGeneric;
            tile = index;
        } else if (AllApplications(urls) && (NSUInteger)index < model.firstRunningIndex)
            operation = NSDragOperationGeneric;
    } else if (AllApplications(urls))
        operation = NSDragOperationGeneric;     /* onto the bar */
    if (tile != _dropTile) {
        _dropTile = tile;
        [self setNeedsDisplay:YES];
    }
    return operation & [info draggingSourceOperationMask];
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info
{
    return [self dropOperationFor:info];
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info
{
    return [self dropOperationFor:info];
}

- (void)draggingExited:(id<NSDraggingInfo>)info
{
    _dropTile = -1;
    [self setNeedsDisplay:YES];
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info
{
    DockModel *model = _model;
    NSArray<NSURL *> *urls = DraggedFileURLs(info);
    NSInteger index = [self tileIndexAtPoint:[self convertPoint:[info draggingLocation] fromView:nil]];
    NSDragOperation operation = [self dropOperationFor:info];

    _dropTile = -1;
    [self setNeedsDisplay:YES];
    if (operation == NSDragOperationNone)
        return NO;
    DockTile *t = index >= 0 ? model.tiles[(NSUInteger)index] : nil;
    if (t.kind == DockTileKindTrash) {
        BOOL all = YES;
        /* A volume dropped on the Trash is ejected, as on the reference
           platform; anything else goes to the Trash. */
        for (NSURL *u in urls)
            if ([[[u URLByDeletingLastPathComponent] path] isEqualToString:@"/Volumes"])
                all &= [[NSWorkspace sharedWorkspace] unmountAndEjectDeviceAtURL:u error:NULL];
            else
                all &= [[NSFileManager defaultManager] trashItemAtURL:u resultingItemURL:NULL error:NULL];
        os_log_info(OS_LOG_DEFAULT, "drop to the Trash: %lu item(s)", (unsigned long)[urls count]);
        return all;
    }
    if (t != nil && !AllApplications(urls)) {
        os_log_info(OS_LOG_DEFAULT, "drop on %{public}s: open %lu item(s)", [t.label UTF8String], (unsigned long)[urls count]);
        return [[NSWorkspace sharedWorkspace] openURLs:urls withApplicationAtURL:t.URL
                                               options:NSWorkspaceLaunchDefault configuration:@{} error:NULL] != nil;
    }
    /* Applications: kept, where they were dropped among the kept ones. */
    NSUInteger at = index >= 0 ? (NSUInteger)index : model.firstRunningIndex;
    for (NSURL *u in urls) {
        DockTile *existing = nil;
        for (DockTile *k in model.tiles)
            if ([[k.URL URLByStandardizingPath] isEqual:[u URLByStandardizingPath]])
                existing = k;
        if (existing == nil)
            existing = [model tileForApplicationURL:u];
        [model keepTile:existing atIndex:MIN(at, model.firstRunningIndex)];
    }
    os_log_info(OS_LOG_DEFAULT, "drop of %lu application(s) kept", (unsigned long)[urls count]);
    return YES;
}

/* -- The tile's menu -- */

- (void)showMenuForTile:(NSUInteger)index event:(NSEvent *)event
{
    DockModel *model = _model;
    DockTile *tile = model.tiles[index];
    if (tile.kind == DockTileKindTrash) {
        [self showTrashMenuWithEvent:event];
        return;
    }
    if (tile.kind == DockTileKindMinimizedWindow)
        return;
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
    item = [options addItemWithTitle:NSLocalizedString(@"Open at Login", nil) action:@selector(toggleOpenAtLogin:) keyEquivalent:@""];
    [item setTarget:self];
    [item setRepresentedObject:tile];
    [item setEnabled:tile.URL != nil];
    [item setState:tile.URL != nil && LoginItemForURL(tile.URL) != NULL ? NSOnState : NSOffState];
    item = [options addItemWithTitle:NSLocalizedString(@"Show in Files", nil) action:@selector(showInFiles:) keyEquivalent:@""];
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

/* The Trash's menu: open it, or empty it. */
- (void)showTrashMenuWithEvent:(NSEvent *)event
{
    DockModel *model = _model;
    NSMenu *menu = [[NSMenu alloc] initWithTitle:model.trashTile.label];
    NSMenuItem *item;

    [_pressTimer invalidate];
    _pressTimer = nil;
    [menu setAutoenablesItems:NO];
    item = [menu addItemWithTitle:NSLocalizedString(@"Open", nil) action:@selector(openTileItem:) keyEquivalent:@""];
    [item setTarget:self];
    [item setRepresentedObject:model.trashTile];
    [menu addItem:[NSMenuItem separatorItem]];
    item = [menu addItemWithTitle:NSLocalizedString(@"Empty Trash", nil) action:@selector(emptyTrash:) keyEquivalent:@""];
    [item setTarget:self];
    [item setEnabled:model.trashIsFull];
    [NSMenu popUpContextMenu:menu withEvent:event forView:self];
    _pressed = -1;
    [self setNeedsDisplay:YES];
}

/* Asks first: what the Trash holds is erased for good. */
- (void)emptyTrash:(NSMenuItem *)sender
{
    NSAlert *alert = [[NSAlert alloc] init];

    [alert setMessageText:NSLocalizedString(@"Are you sure you want to permanently erase the items in the Trash?", nil)];
    [alert setInformativeText:NSLocalizedString(@"You can't undo this action.", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Empty Trash", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
    [NSApp activateIgnoringOtherApps:YES];
    if ([alert runModal] == NSAlertFirstButtonReturn)
        [self.model emptyTrash];
}

- (void)toggleKeep:(NSMenuItem *)sender
{
    DockTile *tile = [sender representedObject];

    if ([self.model isKept:tile])
        [self.model removeTile:tile];
    else
        [self.model keepTile:tile atIndex:self.model.firstRunningIndex];
}

/* The session's login items (loginwindow opens them when the session
 * starts): the item for an application, or NULL. Retained. */
static LSSharedFileListItemRef
LoginItemForURL(NSURL *url)
{
    LSSharedFileListRef list = LSSharedFileListCreate(NULL, kLSSharedFileListSessionLoginItems, NULL);
    LSSharedFileListItemRef found = NULL;

    if (list == NULL)
        return NULL;
    NSArray *items = CFBridgingRelease(LSSharedFileListCopySnapshot(list, NULL));
    for (id object in items) {
        LSSharedFileListItemRef item = (__bridge LSSharedFileListItemRef)object;
        NSURL *itemURL = CFBridgingRelease(LSSharedFileListItemCopyResolvedURL(item, 0, NULL));
        if (itemURL != nil && [[itemURL URLByStandardizingPath] isEqual:[url URLByStandardizingPath]]) {
            found = (LSSharedFileListItemRef)CFRetain(item);
            break;
        }
    }
    CFRelease(list);
    return found;
}

- (void)toggleOpenAtLogin:(NSMenuItem *)sender
{
    NSURL *url = [(DockTile *)[sender representedObject] URL];
    LSSharedFileListRef list;
    LSSharedFileListItemRef item;

    if (url == nil || (list = LSSharedFileListCreate(NULL, kLSSharedFileListSessionLoginItems, NULL)) == NULL)
        return;
    if ((item = LoginItemForURL(url)) != NULL) {
        LSSharedFileListItemRemove(list, item);
        CFRelease(item);
    } else {
        item = LSSharedFileListInsertItemURL(list, kLSSharedFileListItemLast, NULL, NULL,
            (__bridge CFURLRef)url, NULL, NULL);
        if (item != NULL)
            CFRelease(item);
    }
    CFRelease(list);
}

- (void)openTileItem:(NSMenuItem *)sender
{
    [self.model openTile:[sender representedObject]];
}

/* The application selected in a window on its folder (LSRevealInFiles). */
- (void)showInFiles:(NSMenuItem *)sender
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

/* ---- Where a tile bounces: beyond the Dock's window ---- */

/* It draws the bouncing tiles as the Dock's view would beyond its own
   bounds, so the part of a tile above the bar shows here and the rest in
   the Dock, at the same place. */
@interface DockBounceView : NSView
@property (nonatomic, weak) DockView *dockView;
@end

@implementation DockBounceView

- (BOOL)isOpaque
{
    return NO;
}

- (void)drawRect:(NSRect)dirty
{
    [[NSColor clearColor] set];
    NSRectFill(dirty);
    NSRect dock = [[self.dockView window] frame], mine = [[self window] frame];
    NSAffineTransform *shift = [NSAffineTransform transform];
    [shift translateXBy:NSMinX(dock) - NSMinX(mine) yBy:NSMinY(dock) - NSMinY(mine)];
    [NSGraphicsContext saveGraphicsState];
    [shift concat];
    [self.dockView drawBouncingTiles];
    [NSGraphicsContext restoreGraphicsState];
}

@end

@interface DockBounceWindow : NSWindow
@end

@implementation DockBounceWindow

- (BOOL)canBecomeKeyWindow
{
    return NO;
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

/* ---- Minimizing and restoring: a window's image moving to its tile ---- */

/* The minimize effect (mineffect), in kEffectTime. "scale": the image
 * scaled from the window's frame to the tile's, easing out. "genie": the
 * image drawn in strips across the Dock's axis, sliding down a funnel with
 * curved sides from the window to the tile. Restoring plays it backwards. */
static const NSTimeInterval kEffectTime = 0.3;
static const NSTimeInterval kGenieTime = 0.45;
static const int kGenieStrips = 48;

@interface DockEffectView : NSView {
    CGImageRef _picture;    /* the window's image */
    CGImageRef *_strips;    /* genie: its strips, from the Dock's side */
    int _stripCount;
}
@property (nonatomic, strong) NSImage *image;
@property (nonatomic) DockMinimizeEffect effect;
@property (nonatomic) DockOrientation orientation;
@property (nonatomic) NSRect windowRect, tileRect;  /* in the view */
@property (nonatomic) double progress;      /* 0: the window, 1: the tile */
@end

@implementation DockEffectView
- (BOOL)isOpaque
{
    return NO;
}

- (void)dealloc
{
    for (int i = 0; i < _stripCount; i++)
        CGImageRelease(_strips[i]);
    free(_strips);
    CGImageRelease(_picture);
}

/* The window's image, and for the genie its strips across the axis toward
   the Dock, cut once: each frame then draws only each strip's pixels (the
   whole image drawn once a strip was far too slow on a large screen). */
- (void)prepare
{
    _picture = CGImageRetain([self.image CGImageForProposedRect:NULL context:nil hints:nil]);
    if (_picture == NULL || self.effect != DockMinimizeEffectGenie)
        return;
    BOOL vertical = self.orientation != DockOrientationBottom;
    BOOL left = self.orientation == DockOrientationLeft;
    size_t W = CGImageGetWidth(_picture), H = CGImageGetHeight(_picture);
    CGFloat depth = vertical ? NSWidth(self.windowRect) : NSHeight(self.windowRect);
    _stripCount = (int)MIN(160, MAX(kGenieStrips, depth / 5));
    _strips = calloc(_stripCount, sizeof(CGImageRef));
    for (int i = 0; i < _stripCount; i++) {
        double a = (double)i / _stripCount, b = (double)(i + 1) / _stripCount;
        CGRect cut;  /* in the image's pixels, from its top-left corner */
        if (!vertical) {
            size_t y0 = (size_t)floor(H * (1 - b)), y1 = (size_t)ceil(H * (1 - a));
            cut = CGRectMake(0, y0, W, MAX(1, y1 - y0));
        } else if (left) {
            size_t x0 = (size_t)floor(W * a), x1 = (size_t)ceil(W * b);
            cut = CGRectMake(x0, 0, MAX(1, x1 - x0), H);
        } else {
            size_t x0 = (size_t)floor(W * (1 - b)), x1 = (size_t)ceil(W * (1 - a));
            cut = CGRectMake(x0, 0, MAX(1, x1 - x0), H);
        }
        _strips[i] = CGImageCreateWithImageInRect(_picture, cut);
    }
}

static double easeInOut(double t)
{
    return t < 0.5 ? 2 * t * t : 1 - 2 * (1 - t) * (1 - t);
}

- (void)drawRect:(NSRect)dirty
{
    CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

    [[NSColor clearColor] set];
    NSRectFill(dirty);
    if (_picture == NULL)
        return;
    if (self.effect == DockMinimizeEffectScale) {
        double k = 1 - (1 - self.progress) * (1 - self.progress);
        NSRect w = self.windowRect, t = self.tileRect;
        NSRect r = NSMakeRect(NSMinX(w) + (NSMinX(t) - NSMinX(w)) * k, NSMinY(w) + (NSMinY(t) - NSMinY(w)) * k,
                              MAX(1, NSWidth(w) + (NSWidth(t) - NSWidth(w)) * k),
                              MAX(1, NSHeight(w) + (NSHeight(t) - NSHeight(w)) * k));
        CGContextDrawImage(context, NSRectToCGRect(r), _picture);
        return;
    }
    /* Genie, as a funnel from the window to the tile. Along the axis
       toward the Dock ("depth": 0 at the tile's near side, growing away
       from the Dock), the funnel's sides are smooth curves from the tile's
       width at the tile to the window's width at the window's far side.
       First the funnel forms (the window's sides bend toward the tile),
       then the image slides down it: its near edge reaches the tile first
       and its far edge last, each row as wide as the funnel where it is. */
    BOOL vertical = self.orientation != DockOrientationBottom;
    BOOL left = self.orientation == DockOrientationLeft;
    NSRect w = self.windowRect, t = self.tileRect;
    double p = self.progress;
    /* In depth: the tile's near and far sides, the window's near and far. */
    double tNear = 0, tFar, wNear, wFar;
    /* Across: the tile's and the window's low and high sides. */
    double tLo, tHi, wLo, wHi;
    if (!vertical) {
        tFar = NSHeight(t); wNear = NSMinY(w) - NSMinY(t); wFar = NSMaxY(w) - NSMinY(t);
        tLo = NSMinX(t); tHi = NSMaxX(t); wLo = NSMinX(w); wHi = NSMaxX(w);
    } else if (left) {
        tFar = NSWidth(t); wNear = NSMinX(w) - NSMinX(t); wFar = NSMaxX(w) - NSMinX(t);
        tLo = NSMinY(t); tHi = NSMaxY(t); wLo = NSMinY(w); wHi = NSMaxY(w);
    } else {
        tFar = NSWidth(t); wNear = NSMaxX(t) - NSMaxX(w); wFar = NSMaxX(t) - NSMinX(w);
        tLo = NSMinY(t); tHi = NSMaxY(t); wLo = NSMinY(w); wHi = NSMaxY(w);
    }
    double bend = easeInOut(MIN(1, p / 0.4));                   /* the funnel forming */
    double nearMove = easeInOut(MIN(1, MAX(0, (p - 0.1) / 0.6)));  /* the image's near edge */
    double farMove = easeInOut(MIN(1, MAX(0, (p - 0.3) / 0.7)));   /* and its far edge */
    double nearAt = wNear + (tNear - wNear) * nearMove;
    double farAt = wFar + (tFar - wFar) * farMove;
    int strips = _stripCount;
    for (int i = 0; i < strips && _strips != NULL; i++) {
        double a = (double)i / strips, b = (double)(i + 1) / strips;   /* from the image's near edge */
        double d0 = nearAt + (farAt - nearAt) * a, d1 = nearAt + (farAt - nearAt) * b;
        double dm = (d0 + d1) / 2;
        /* The funnel's sides at this depth: the tile's below its far side,
           then a smooth curve up to the window's at the window's far side. */
        double u = (dm - tFar) / MAX(1, wFar - tFar);
        u = MIN(1, MAX(0, u));
        double s = u * u * (3 - 2 * u);
        double lo = tLo + (wLo - tLo) * s, hi = tHi + (wHi - tHi) * s;
        lo = wLo + (lo - wLo) * bend;
        hi = wHi + (hi - wHi) * bend;
        /* Along the axis in whole pixels, a pixel more so that strips
           meet without a seam. */
        double e0 = floor(d0), e1 = MAX(e0 + 1, ceil(d1) + 1);
        CGRect dst;
        if (!vertical)
            dst = CGRectMake(lo, NSMinY(t) + e0, MAX(1, hi - lo), e1 - e0);
        else if (left)
            dst = CGRectMake(NSMinX(t) + e0, lo, e1 - e0, MAX(1, hi - lo));
        else
            dst = CGRectMake(NSMaxX(t) - e1, lo, e1 - e0, MAX(1, hi - lo));
        if (_strips[i] != NULL)
            CGContextDrawImage(context, dst, _strips[i]);
    }
}
@end

@interface DockEffectWindow : NSWindow
+ (void)animateImage:(NSImage *)image window:(NSRect)window tile:(NSRect)tile minimizing:(BOOL)minimizing
              effect:(DockMinimizeEffect)effect orientation:(DockOrientation)orientation
                then:(nullable void (^)(void))done;
@end

@implementation DockEffectWindow
+ (void)animateImage:(NSImage *)image window:(NSRect)window tile:(NSRect)tile minimizing:(BOOL)minimizing
              effect:(DockMinimizeEffect)effect orientation:(DockOrientation)orientation
                then:(void (^)(void))done
{
    /* One window over the whole way, still; the view draws each frame. */
    NSRect all = NSIntegralRect(NSUnionRect(window, tile));
    DockEffectWindow *w = [[DockEffectWindow alloc] initWithContentRect:all styleMask:NSBorderlessWindowMask
                                                                backing:NSBackingStoreBuffered defer:NO];
    DockEffectView *view = [[DockEffectView alloc] initWithFrame:NSMakeRect(0, 0, NSWidth(all), NSHeight(all))];
    NSTimeInterval duration = effect == DockMinimizeEffectGenie ? kGenieTime : kEffectTime;
    /* Shift held: in slow motion, as on the reference platform. */
    if ([NSEvent modifierFlags] & NSShiftKeyMask)
        duration *= 10;
    CFAbsoluteTime start = CFAbsoluteTimeGetCurrent();

    view.image = image;
    view.effect = effect;
    view.orientation = orientation;
    view.windowRect = NSOffsetRect(window, -NSMinX(all), -NSMinY(all));
    view.tileRect = NSOffsetRect(tile, -NSMinX(all), -NSMinY(all));
    view.progress = minimizing ? 0 : 1;
    [view prepare];
    [w setContentView:view];
    [w setOpaque:NO];
    [w setBackgroundColor:[NSColor clearColor]];
    [w setHasShadow:NO];
    [w setIgnoresMouseEvents:YES];
    [w setReleasedWhenClosed:NO];
    [w setLevel:CGWindowLevelForKey(kCGDockWindowLevelKey) + 1];
    [w orderFront:nil];
    os_log_info(OS_LOG_DEFAULT, "%{public}s effect %{public}s", minimizing ? "minimize" : "restore",
                effect == DockMinimizeEffectGenie ? "genie" : "scale");
    [NSTimer scheduledTimerWithTimeInterval:1.0 / 60 repeats:YES block:^(NSTimer *timer) {
        double t = MIN(1.0, (CFAbsoluteTimeGetCurrent() - start) / duration);
        view.progress = minimizing ? t : 1 - t;
        [view display];
        if (t >= 1) {
            [timer invalidate];
            [w orderOut:nil];
            if (done != nil)
                done();
        }
    }];
}

- (BOOL)canBecomeKeyWindow
{
    return NO;
}
@end

/* A server window's image (premultiplied BGRA), or nil. */
static NSImage *ImageOfServerWindow(uint32_t windowID)
{
    uint8_t *pixels = NULL;
    uint32_t width = 0, height = 0, rowBytes = 0;

    if (SLSCopyWindowImage(windowID, &pixels, &width, &height, &rowBytes) != kSLSErrorSuccess || pixels == NULL)
        return nil;
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(pixels, width, height, 8, rowBytes, space,
                                                 kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGImageRef cgImage = context != NULL ? CGBitmapContextCreateImage(context) : NULL;
    NSImage *image = cgImage != NULL ? [[NSImage alloc] initWithCGImage:cgImage size:NSMakeSize(width, height)] : nil;

    if (cgImage != NULL)
        CGImageRelease(cgImage);
    if (context != NULL)
        CGContextRelease(context);
    CGColorSpaceRelease(space);
    free(pixels);
    return image;
}

/* A minimized window's tile: its image fitted in a square, its app's icon
 * in the corner, as on the reference platform. */
static NSImage *ThumbnailFor(NSImage *window, NSImage *_Nullable appIcon)
{
    const CGFloat side = 128;

    return [NSImage imageWithSize:NSMakeSize(side, side) flipped:NO drawingHandler:^BOOL(NSRect rect) {
        NSSize size = [window size];
        CGFloat scale = MIN(side / MAX(size.width, 1), side / MAX(size.height, 1)) * 0.92;
        NSRect fit = NSMakeRect((side - size.width * scale) / 2, (side - size.height * scale) / 2,
                                size.width * scale, size.height * scale);
        [window drawInRect:NSIntegralRect(fit) fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0];
        if (appIcon != nil)
            [appIcon drawInRect:NSMakeRect(side * 0.6, 0, side * 0.4, side * 0.4) fromRect:NSZeroRect
                      operation:NSCompositeSourceOver fraction:1.0];
        (void)rect;
        return YES;
    }];
}

/* ---- The window ---- */

@implementation DockWindow {
    DockModel *_model;
    DockView *_view;
    DockLabelWindow *_label;
    DockTriggerWindow *_trigger;
    BOOL _shown;              /* autohide: on screen now */
    NSTimer *_slideTimer, *_hideTimer;
    NSTimer *_bounceTimer;    /* while a tile bounces for attention */
    DockBounceWindow *_bounceWindow;  /* where it bounces, beyond the bar */
    DockSwitcher *_switcher;  /* Command-Tab */
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
    [self setLevel:CGWindowLevelForKey(kCGDockWindowLevelKey)];
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
    /* A display changed (mode, arrangement): placed again. Its own area
       published again is no change, so no notice comes back. */
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(modelChanged:)
                                                 name:NSApplicationDidChangeScreenParametersNotification
                                               object:nil];
    /* Windows minimized into the Dock and restored (AppKit posts the
       server's notices; SLSRequestWindowNotifications asks for them). */
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowMinimized:)
                                                 name:@"_NSSkyLightWindowMinimizedNotification" object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowRestored:)
                                                 name:@"_NSSkyLightWindowRestoredNotification" object:nil];
    SLSRequestWindowNotifications(1);
    _switcher = [[DockSwitcher alloc] init];
    /* Apps' attention requests and tile pictures (NSApplication's
       requestUserAttention:, NSDockTile's display), and their activation,
       which ends a bounce. */
    NSDistributedNotificationCenter *center = [NSDistributedNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(attentionRequested:) name:@"org.minkar.dock.attention" object:nil
     suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
    [center addObserver:self selector:@selector(attentionCancelled:) name:@"org.minkar.dock.attention.cancel" object:nil
     suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
    [center addObserver:self selector:@selector(tileDrawn:) name:@"org.minkar.dock.tile" object:nil
     suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
    /* Files' Add to Dock: the application, its path the object, kept after
       the kept ones (where it is when it is kept already). */
    [center addObserver:self selector:@selector(keepRequested:) name:@"org.minkar.dock.keep" object:nil
     suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(appActivated:)
        name:NSWorkspaceDidActivateApplicationNotification object:nil];
    [self layout];
    return self;
}

- (DockTile *)tileForPID:(pid_t)pid
{
    for (DockTile *tile in _model.tiles)
        if (tile.kind != DockTileKindMinimizedWindow && tile.application != nil &&
            [tile.application processIdentifier] == pid)
            return tile;
    return nil;
}

- (void)keepRequested:(NSNotification *)note
{
    NSString *path = [note object];
    DockModel *model = _model;

    if (![path isKindOfClass:[NSString class]] || ![[[path pathExtension] lowercaseString] isEqualToString:@"app"] ||
        ![[NSFileManager defaultManager] fileExistsAtPath:path])
        return;
    NSURL *url = [[NSURL fileURLWithPath:path] URLByStandardizingPath];
    DockTile *tile = nil;
    for (DockTile *k in model.tiles)
        if ([[k.URL URLByStandardizingPath] isEqual:url])
            tile = k;
    if (tile != nil && [model isKept:tile])
        return;
    if (tile == nil)
        tile = [model tileForApplicationURL:url];
    [model keepTile:tile atIndex:model.firstRunningIndex];
    os_log_info(OS_LOG_DEFAULT, "kept %{public}s (Add to Dock)", [path fileSystemRepresentation]);
}

- (void)attentionRequested:(NSNotification *)note
{
    DockTile *tile = [self tileForPID:[note.userInfo[@"pid"] intValue]];
    if (tile == nil)
        return;
    tile.bounceStart = CFAbsoluteTimeGetCurrent();
    tile.bounceCritical = [note.userInfo[@"critical"] boolValue];
    os_log_info(OS_LOG_DEFAULT, "attention %{public}s%{public}s", [tile.label UTF8String], tile.bounceCritical ? " critical" : "");
    [self bounce];
}

- (void)attentionCancelled:(NSNotification *)note
{
    DockTile *tile = [self tileForPID:[note.userInfo[@"pid"] intValue]];
    tile.bounceStart = 0;
    [self bounce];
}

- (void)appActivated:(NSNotification *)note
{
    NSRunningApplication *app = note.userInfo[NSWorkspaceApplicationKey];
    DockTile *tile = [self tileForPID:[app processIdentifier]];
    if (tile.bounceStart != 0) {
        tile.bounceStart = 0;
        [self bounce];
    }
}

/* While a tile bounces: the window has room above the bar and redraws at
   the display's rate; when none does, it shrinks back. */
- (void)bounce
{
    BOOL any = NO;
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    for (DockTile *tile in _model.tiles) {
        /* Bouncing while it starts: until its app has checked in (or 15 s). */
        if (tile.bounceLaunch && ((tile.application != nil && ![tile.application isTerminated]) ||
                                  now - tile.bounceStart >= 15)) {
            tile.bounceLaunch = NO;
            tile.bounceStart = 0;
        }
        if (tile.bounceStart != 0 && !tile.bounceCritical && !tile.bounceLaunch &&
            now - tile.bounceStart >= 2 * kBouncePeriod)
            tile.bounceStart = 0;
        any |= tile.bounceStart != 0;
    }
    CGFloat room = any ? ceil(_model.tileSize * 0.5) + 2 : 0;
    bounceRoom = room;
    [self placeBounceWindow];
    [_view setNeedsDisplay:YES];
    if (any && _bounceTimer == nil)
        _bounceTimer = [NSTimer scheduledTimerWithTimeInterval:1.0 / 60 target:self selector:@selector(bounceTick:)
                                                      userInfo:nil repeats:YES];
    else if (!any) {
        [_bounceTimer invalidate];
        _bounceTimer = nil;
    }
}

- (void)bounceTick:(NSTimer *)timer
{
    [self bounce];
}

/* The bouncing tiles' window: beyond the Dock's, on the side away from
   the screen's edge, while one bounces; else away. */
- (void)placeBounceWindow
{
    if (bounceRoom <= 0 || !_shown) {
        [_bounceWindow orderOut:nil];
        return;
    }
    if (_bounceWindow == nil) {
        _bounceWindow = [[DockBounceWindow alloc] initWithContentRect:NSMakeRect(0, 0, 10, 10)
            styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
        DockBounceView *view = [[DockBounceView alloc] initWithFrame:NSMakeRect(0, 0, 10, 10)];
        view.dockView = _view;
        [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [_bounceWindow setContentView:view];
        [_bounceWindow setLevel:CGWindowLevelForKey(kCGDockWindowLevelKey)];
        [_bounceWindow setOpaque:NO];
        [_bounceWindow setBackgroundColor:[NSColor clearColor]];
        [_bounceWindow setHasShadow:NO];
        [_bounceWindow setHidesOnDeactivate:NO];
        [_bounceWindow setExcludedFromWindowsMenu:YES];
        [_bounceWindow setReleasedWhenClosed:NO];
        [_bounceWindow setIgnoresMouseEvents:YES];
    }
    NSRect dock = [self frame], frame;
    switch (_model.orientation) {
    case DockOrientationLeft: frame = NSMakeRect(NSMaxX(dock), NSMinY(dock), bounceRoom, NSHeight(dock)); break;
    case DockOrientationRight: frame = NSMakeRect(NSMinX(dock) - bounceRoom, NSMinY(dock), bounceRoom, NSHeight(dock)); break;
    default: frame = NSMakeRect(NSMinX(dock), NSMaxY(dock), NSWidth(dock), bounceRoom); break;
    }
    if (!NSEqualRects(frame, [_bounceWindow frame]))
        [_bounceWindow setFrame:frame display:NO];
    if (![_bounceWindow isVisible])
        [_bounceWindow orderFront:nil];
    [[_bounceWindow contentView] setNeedsDisplay:YES];
}

/* An app's tile drawn by itself (NSDockTile's content view), or its icon
   back. */
- (void)tileDrawn:(NSNotification *)note
{
    DockTile *tile = [self tileForPID:[note.userInfo[@"pid"] intValue]];
    NSData *png = note.userInfo[@"png"];
    if (tile == nil)
        return;
    tile.contentImage = png != nil ? [[NSImage alloc] initWithData:png] : nil;
    os_log_info(OS_LOG_DEFAULT, "tile %{public}s %{public}s", [tile.label UTF8String], png != nil ? "drawn by the app" : "back to its icon");
    [_view setNeedsDisplay:YES];
}

/* A server window's frame in AppKit's screen coordinates. */
static NSRect ScreenFrameOfServerWindow(const SLSWindowInfo *info)
{
    CGFloat top = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]);

    return NSMakeRect(info->x, top - info->y - info->height, info->width, info->height);
}

- (NSRect)screenRectOfTileAtIndex:(NSUInteger)index
{
    NSRect r = [_view convertRect:[_view rectForTileAtIndex:index] toView:nil];

    return [self convertRectToScreen:r];
}

- (void)windowMinimized:(NSNotification *)note
{
    uint32_t windowID = [note.userInfo[@"window"] unsignedIntValue];
    SLSWindowInfo info;

    if (SLSGetWindowInfo(windowID, &info) != kSLSErrorSuccess)
        return;
    NSImage *image = ImageOfServerWindow(windowID);
    NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:info.ownerPID];
    /* The effect starts at once (the server has hidden the window); its
       tile's picture, slower to make, follows. */
    NSUInteger index = [_model addMinimizedWindow:windowID owner:info.ownerPID image:nil];
    [self layout];
    if (image == nil)
        return;
    [DockEffectWindow animateImage:image window:ScreenFrameOfServerWindow(&info)
                              tile:[self screenRectOfTileAtIndex:index] minimizing:YES
                            effect:_model.minimizeEffect orientation:_model.orientation then:nil];
    DockTile *tile = [_model tileForMinimizedWindow:windowID];
    if (tile != nil) {
        tile.contentImage = ThumbnailFor(image, [app icon]);
        [_view setNeedsDisplay:YES];
    }
}

- (void)windowRestored:(NSNotification *)note
{
    [_model removeMinimizedWindow:[note.userInfo[@"window"] unsignedIntValue]];
}

/* A click on a minimized window's tile: its image back to its frame, then
   the server restores it and its app brings it back. */
- (void)restoreMinimizedTile:(DockTile *)tile
{
    SLSWindowInfo info;
    NSUInteger index = [_model.tiles indexOfObject:tile];
    uint32_t windowID = tile.windowID;

    if (index == NSNotFound || SLSGetWindowInfo(windowID, &info) != kSLSErrorSuccess) {
        [_model removeMinimizedWindow:windowID];
        return;
    }
    NSImage *image = ImageOfServerWindow(windowID);
    NSRect from = [self screenRectOfTileAtIndex:index];
    DockModel *model = _model;

    [model removeMinimizedWindow:windowID];
    if (image == nil) {
        SLSSetWindowMinimized(windowID, 0);
        return;
    }
    [DockEffectWindow animateImage:image window:ScreenFrameOfServerWindow(&info) tile:from minimizing:NO
                            effect:model.minimizeEffect orientation:model.orientation then:^{
        SLSSetWindowMinimized(windowID, 0);
    }];
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

/* Tells the window server the part of the screen the Dock keeps: from its
   edge to the bar's far side (not the room for magnified tiles), or none
   when it hides itself. Apps' visible frame leaves it out. */
- (void)publishReservedArea
{
    NSScreen *screen = [[NSScreen screens] firstObject];
    NSRect full = [screen frame];
    CGFloat depth = kEdgeGap + [DockView barThicknessForModel:_model];
    NSRect shown = [self frameShown:YES];
    NSRect area = NSZeroRect;

    if (!_model.autohide) {
        switch (_model.orientation) {
        case DockOrientationLeft:
            area = NSMakeRect(NSMinX(full), NSMinY(shown), depth, NSHeight(shown));
            break;
        case DockOrientationRight:
            area = NSMakeRect(NSMaxX(full) - depth, NSMinY(shown), depth, NSHeight(shown));
            break;
        default:
            area = NSMakeRect(NSMinX(shown), NSMinY(full), NSWidth(shown), depth);
            break;
        }
    }
    /* Global points from the top left of the main display. */
    SLSIntRect rect = { 0, 0, 0, 0 };
    if (!NSIsEmptyRect(area)) {
        rect.x = (int32_t)floor(NSMinX(area));
        rect.y = (int32_t)floor(NSMaxY(full) - NSMaxY(area));
        rect.width = (int32_t)ceil(NSWidth(area));
        rect.height = (int32_t)ceil(NSHeight(area));
    }
    SLSSetDisplayDockRect(CGMainDisplayID(), rect);
}

- (void)layout
{
    if (!_model.autohide)
        _shown = YES;
    NSRect frame = [self frameShown:_shown];
    if (!NSEqualRects(frame, [self frame]))
        [self setFrame:frame display:YES];
    [_view setNeedsDisplay:YES];
    [self placeBounceWindow];
    [self publishReservedArea];

    if (_model.autohide) {
        if (_trigger == nil) {
            _trigger = [[DockTriggerWindow alloc] initWithContentRect:[self triggerFrame]
                styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
            DockTriggerView *view = [[DockTriggerView alloc] initWithFrame:NSMakeRect(0, 0, 10, 10)];
            view.dockWindow = self;
            [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
            [_trigger setContentView:view];
            [_trigger setLevel:CGWindowLevelForKey(kCGDockWindowLevelKey)];
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
    [_view forgetPointerUnlessInside];
    [_view forgetIcons];    /* tiles or sizes may have gone */
    [self layout];
    [self bounce];          /* an app that starts bounces (launchanim) */
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
