/*
 * Copyright (c) 2006-2007 Christopher J. W. Lloyd
 * Copyright (C) 2024 Zoe Knox <zoe@ravynsoft.com>
 * Modifications Copyright (c) 2026 The Minkar contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#import <objc/runtime.h>
#import <AppKit/NSGraphicsStyle.h>
#import <AppKit/NSThemeFrame.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSWindow-Private.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSColor.h>

BOOL _NSSystemAppearanceIsDark(void);
#import <AppKit/NSImage.h>
#import <AppKit/NSToolbarView.h>
#import <AppKit/NSTitlebarAccessoryViewController.h>

/* A window's tab bar (NSWindowTabGroup.m), placed under the title bar and
   the toolbar, above the content. */
@interface NSWindowTabBarView : NSView
@end
extern const CGFloat NSWindowTabBarHeight;

/* Tiling (NSWindow.m). */
@interface NSWindow (NSWindowTiling)
-(void)_tileWindowToLeftOfScreen:(id)sender;
-(void)_tileWindowToRightOfScreen:(id)sender;
@end

/* The view the frame places for an accessory (NSTitlebarAccessoryViewController.m). */
@interface NSTitlebarAccessoryViewController (NSThemeFrame)
-(NSView *)_titlebarContainerView;
@end
#import <AppKit/NSAttributedString.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSMenuItem.h>
#import <Onyx2D/O2Context.h>
#import <AppKit/NSCursor.h>
#import <AppKit/NSEvent.h>


extern void CGNativeBorderFrameWidthsForStyle(unsigned styleMask, CGFloat *top,
    CGFloat *left, CGFloat *bottom, CGFloat *right);

/* Above a full-size content view (NSWindowStyleMaskFullSizeContentView):
 * the window's buttons and its title, drawn over the content; it takes
 * only the buttons' clicks. It covers the frame view, in its coordinates. */
// all measurements in pixels
/* The reference platform's measures: a 22-point title bar, no side or
   bottom border (the window's shadow outlines it). */
const float NSWindowTitleHeight = 22;
const float NSWindowEdgePad = 2;
const float NSWindowCornerRadius = 8;
const float NSWindowControlDiameter = 12;
const float NSWindowControlSpacing = 8;

@interface NSTitlebarOverlayView : NSView
@end

@implementation NSTitlebarOverlayView

-(BOOL)isOpaque {
    return NO;
}

-(BOOL)mouseDownCanMoveWindow {
    return YES;
}

-(NSView *)hitTest:(NSPoint)point {
    NSThemeFrame *frame = (NSThemeFrame *)[self superview];
    if(!([[self window] styleMask] & NSWindowStyleMaskFullSizeContentView))
        return nil;
    return NSPointInRect(point, NSInsetRect([frame _windowControlsRect], -2, -2)) ? self : nil;
}

-(void)mouseDown:(NSEvent *)event {
    [[self superview] mouseDown:event];
}

/* The content view covers the window's corners: they are made round
   again here (outside the curve, the window is clear; along it, the
   frame's border), all four, and a full-size content view gets the
   window's buttons and title drawn above it. */
-(void)drawRect:(NSRect)rect {
    NSThemeFrame *frame = (NSThemeFrame *)[self superview];
    O2Context *context = [[[self window] graphicsContext] graphicsPort];
    NSRect b = [self bounds];
    CGFloat r = NSWindowCornerRadius;

    for(int corner = 0; corner < 4; corner++) {
        BOOL right = (corner & 1) != 0, top = (corner & 2) != 0;
        CGFloat edgeX = right ? NSMaxX(b) : NSMinX(b), edgeY = top ? NSMaxY(b) : NSMinY(b);
        /* Only the corners the rect being drawn reaches. */
        NSRect cornerBox = NSMakeRect(right ? edgeX - r - NSWindowEdgePad : edgeX,
            top ? edgeY - r - NSWindowEdgePad : edgeY, r + NSWindowEdgePad, r + NSWindowEdgePad);
        if(!NSIntersectsRect(cornerBox, rect))
            continue;
        CGFloat cx = right ? edgeX - r : edgeX + r, cy = top ? edgeY - r : edgeY + r;
        /* The arc from the corner's vertical edge to its horizontal one. */
        CGFloat from = right ? 0 : 3.14159, to = top ? 1.5708 : 4.71239;
        BOOL clockwise = (right == top) ? NO : YES;

        O2ContextSaveGState(context);
        O2ContextSetBlendMode(context, kO2BlendModeCopy);
        O2ContextSetRGBFillColor(context, 0, 0, 0, 0);
        O2ContextBeginPath(context);
        O2ContextMoveToPoint(context, edgeX, cy);
        O2ContextAddLineToPoint(context, edgeX, edgeY);
        O2ContextAddLineToPoint(context, cx, edgeY);
        O2ContextAddArc(context, cx, cy, r, to, from, !clockwise);
        O2ContextClosePath(context);
        O2ContextFillPath(context);
        O2ContextRestoreGState(context);

        O2ContextSetGrayStrokeColor(context, _NSSystemAppearanceIsDark() ? 0.24 : 0.8, 1);
        O2ContextSetLineWidth(context, NSWindowEdgePad);
        O2ContextBeginPath(context);
        O2ContextAddArc(context, cx, cy, r - NSWindowEdgePad / 2, from, to, clockwise);
        O2ContextStrokePath(context);
    }
    if(([[self window] styleMask] & NSWindowStyleMaskFullSizeContentView) &&
       NSIntersectsRect(rect, NSMakeRect(0, NSMaxY(b) - NSWindowTitleHeight, NSWidth(b), NSWindowTitleHeight)))
        [frame _drawTitlebarControls];
}

@end

BOOL _NSAppearanceThemeActive(void);

/* The title bar's fill: the frame's grey, or an appearance theme's
   windowFrameColor. */
static void setTitlebarFill(O2ContextRef context, CGFloat gray) {
    if(_NSAppearanceThemeActive()) {
        /* sRGB's components, as the context fills in device RGB (the
           calibrated ones, gamma 1.8, came out darker). */
        NSColor *c = [[NSColor windowFrameColor] colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
        if(c != nil) {
            O2ContextSetRGBFillColor(context, [c redComponent], [c greenComponent], [c blueComponent], 1);
            return;
        }
    }
    O2ContextSetGrayFillColor(context, gray, 1);
}

@implementation NSThemeFrame


enum { kControlClose, kControlMini, kControlZoom };

/* An NSColor's components into the context's fill or stroke color. */
static void setColor(O2Context *context, NSColor *color, BOOL stroke) {
    CGFloat r = 0, g = 0, b = 0, a = 1;
    NSColor *rgb = [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];

    if(rgb == nil)
        rgb = [color colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    [rgb getRed:&r green:&g blue:&b alpha:&a];
    if(stroke)
        O2ContextSetRGBStrokeColor(context, r, g, b, a);
    else
        O2ContextSetRGBFillColor(context, r, g, b, a);
}

/* Atlas's window controls, drawn (no artwork): discs in the theme's
 * control face with its darker border, as its buttons (light or dark),
 * their sign in the controls' text color; under the pointer the pressed
 * button's face, close red; a control the window lacks is faint and has
 * no sign. */
static void drawControl(O2Context *context, NSRect rect, int kind, BOOL enabled, BOOL hover) {
    O2ContextSaveGState(context);
    if(!enabled)
        setColor(context, _NSControlGray(0.90), NO);
    else if(hover && kind == kControlClose)
        O2ContextSetRGBFillColor(context, 0.86, 0.33, 0.29, 1);
    else
        setColor(context, _NSControlGray(hover ? 0.84 : 1.0), NO);
    O2ContextFillEllipseInRect(context, rect);
    /* The border, inside the disc: the theme's control border. */
    if(hover && enabled && kind == kControlClose)
        O2ContextSetRGBStrokeColor(context, 0.70, 0.22, 0.19, 1);
    else
        setColor(context, _NSControlGray(enabled ? 0.55 : 0.75), YES);
    O2ContextSetLineWidth(context, 1);
    O2ContextStrokeEllipseInRect(context, NSInsetRect(rect, 0.5, 0.5));
    if(enabled) {
        CGFloat inset = 3.5, midX = NSMidX(rect), midY = NSMidY(rect);
        CGFloat lo = NSMinX(rect) + inset, hi = NSMaxX(rect) - inset;
        CGFloat bottom = NSMinY(rect) + inset, top = NSMaxY(rect) - inset;
        O2Point lines[4];
        unsigned count = 0;

        if(hover && kind == kControlClose)
            O2ContextSetGrayStrokeColor(context, 1, 1);
        else
            setColor(context, [NSColor controlTextColor], YES);
        O2ContextSetLineWidth(context, 1.5);
        O2ContextSetLineCap(context, kO2LineCapRound);
        switch(kind) {
            case kControlClose:
                lines[0] = O2PointMake(lo, bottom); lines[1] = O2PointMake(hi, top);
                lines[2] = O2PointMake(lo, top); lines[3] = O2PointMake(hi, bottom);
                count = 4;
                break;
            case kControlMini:
                lines[0] = O2PointMake(lo, midY); lines[1] = O2PointMake(hi, midY);
                count = 2;
                break;
            case kControlZoom:
                lines[0] = O2PointMake(lo, midY); lines[1] = O2PointMake(hi, midY);
                lines[2] = O2PointMake(midX, bottom); lines[3] = O2PointMake(midX, top);
                count = 4;
                break;
        }
        O2ContextStrokeLineSegments(context, lines, count);
    }
    O2ContextRestoreGState(context);
}

-initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    pointer = NSZeroPoint;

    return self;
}

-(BOOL)isOpaque {
   return YES;
}

/* Which control is under point: 1 close, 2 zoom, 3 minimize, 0 none. */
-(NSInteger)_controlAtPoint:(NSPoint)point {
    if([[self window] _hidesWindowControls]) return 0;
    if(NSPointInRect(point, _closeButtonRect)) return 1;
    if(NSPointInRect(point, _zoomButtonRect)) return 2;
    if(NSPointInRect(point, _miniButtonRect)) return 3;
    return 0;
}

/* Minkar: a resizable window is resized from its edges and corners, as on
   the reference: a band of kResizeBand points inside the frame on each
   side (the corners reach kResizeCorner along the edges), its cursor the
   frame's resize cursor. Not in full screen, a sheet or a tile. */
static const CGFloat kResizeBand = 5, kResizeCorner = 14;


-(NSCursorFrameResizePosition)_resizePositionAtPoint:(NSPoint)point {
    NSWindow  *window = [self window];
    NSUInteger mask = [window styleMask];
    NSRect     b = [self bounds];
    NSCursorFrameResizePosition position = 0;

    if(!(mask & NSResizableWindowMask) || (mask & NSFullScreenWindowMask) || [window isSheet] ||
       !NSPointInRect(point, b))
        return 0;
    BOOL left = point.x < NSMinX(b) + kResizeBand, right = point.x >= NSMaxX(b) - kResizeBand;
    BOOL bottom = point.y < NSMinY(b) + kResizeBand, top = point.y >= NSMaxY(b) - kResizeBand;
    if(left || right) {
        if(point.y < NSMinY(b) + kResizeCorner) bottom = YES;
        if(point.y >= NSMaxY(b) - kResizeCorner) top = YES;
    }
    if(top || bottom) {
        if(point.x < NSMinX(b) + kResizeCorner) left = YES;
        if(point.x >= NSMaxX(b) - kResizeCorner) right = YES;
    }
    if(left) position |= NSCursorFrameResizePositionLeft;
    if(right) position |= NSCursorFrameResizePositionRight;
    if(bottom) position |= NSCursorFrameResizePositionBottom;
    if(top) position |= NSCursorFrameResizePositionTop;
    return position;
}

-(void)resetCursorRects {
    [super resetCursorRects];
    if([self _resizePositionAtPoint:NSMakePoint(NSMidX([self bounds]), NSMinY([self bounds]))] == 0)
        return;
    NSRect b = [self bounds];
    CGFloat w = NSWidth(b), h = NSHeight(b);
    struct { NSRect r; NSCursorFrameResizePosition p; } bands[] = {
        { NSMakeRect(kResizeCorner, 0, w - 2 * kResizeCorner, kResizeBand), NSCursorFrameResizePositionBottom },
        { NSMakeRect(kResizeCorner, h - kResizeBand, w - 2 * kResizeCorner, kResizeBand), NSCursorFrameResizePositionTop },
        { NSMakeRect(0, kResizeCorner, kResizeBand, h - 2 * kResizeCorner), NSCursorFrameResizePositionLeft },
        { NSMakeRect(w - kResizeBand, kResizeCorner, kResizeBand, h - 2 * kResizeCorner), NSCursorFrameResizePositionRight },
        { NSMakeRect(0, 0, kResizeCorner, kResizeCorner), NSCursorFrameResizePositionBottomLeft },
        { NSMakeRect(w - kResizeCorner, 0, kResizeCorner, kResizeCorner), NSCursorFrameResizePositionBottomRight },
        { NSMakeRect(0, h - kResizeCorner, kResizeCorner, kResizeCorner), NSCursorFrameResizePositionTopLeft },
        { NSMakeRect(w - kResizeCorner, h - kResizeCorner, kResizeCorner, kResizeCorner), NSCursorFrameResizePositionTopRight },
    };
    /* The corners' rects are only their bands, not their insides. */
    for(unsigned i = 0; i < sizeof(bands) / sizeof(bands[0]); i++) {
        NSRect r = bands[i].r;
        if(i >= 4) {
            NSRect horizontal = NSMakeRect(NSMinX(r), (bands[i].p & NSCursorFrameResizePositionBottom) ? 0 : h - kResizeBand,
                                           kResizeCorner, kResizeBand);
            NSRect vertical = NSMakeRect((bands[i].p & NSCursorFrameResizePositionLeft) ? 0 : w - kResizeBand, NSMinY(r),
                                         kResizeBand, kResizeCorner);
            NSCursor *c = [NSCursor frameResizeCursorFromPosition:bands[i].p inDirections:NSCursorFrameResizeDirectionsAll];
            [self addCursorRect:horizontal cursor:c];
            [self addCursorRect:vertical cursor:c];
            continue;
        }
        [self addCursorRect:r cursor:[NSCursor frameResizeCursorFromPosition:bands[i].p inDirections:NSCursorFrameResizeDirectionsAll]];
    }
}

/* Dragging an edge or a corner: the frame follows the pointer on the
   screen, within the window's minimum and maximum sizes, the opposite
   edges kept in place. */
-(void)_resizeFrom:(NSCursorFrameResizePosition)position withEvent:(NSEvent *)event {
    NSWindow *window = [self window];
    NSRect    start = [window frame];
    NSPoint   origin = [NSEvent mouseLocation];
    NSSize    minSize = [window minSize], maxSize = [window maxSize];

    if(minSize.width < 1) minSize.width = 1;
    if(minSize.height < 1) minSize.height = 1;
    if(maxSize.width <= 0) maxSize.width = CGFLOAT_MAX;
    if(maxSize.height <= 0) maxSize.height = CGFLOAT_MAX;
    [[NSCursor frameResizeCursorFromPosition:position inDirections:NSCursorFrameResizeDirectionsAll] push];
    for(;;) {
        NSEvent *next = [NSApp nextEventMatchingMask:NSLeftMouseDraggedMask | NSLeftMouseUpMask
                                           untilDate:[NSDate distantFuture] inMode:NSEventTrackingRunLoopMode dequeue:YES];
        /* Only where the pointer is now: the drags queued meanwhile are
           passed over (each one would lay the window out again). */
        while(next != nil && [next type] == NSLeftMouseDragged) {
            NSEvent *later = [NSApp nextEventMatchingMask:NSLeftMouseDraggedMask | NSLeftMouseUpMask
                                                untilDate:[NSDate distantPast] inMode:NSEventTrackingRunLoopMode dequeue:YES];
            if(later == nil)
                break;
            next = later;
        }
        NSPoint at = [NSEvent mouseLocation];
        CGFloat dx = at.x - origin.x, dy = at.y - origin.y;
        NSRect  frame = start;

        if(position & NSCursorFrameResizePositionRight)
            frame.size.width = MIN(MAX(start.size.width + dx, minSize.width), maxSize.width);
        if(position & NSCursorFrameResizePositionLeft) {
            frame.size.width = MIN(MAX(start.size.width - dx, minSize.width), maxSize.width);
            frame.origin.x = NSMaxX(start) - frame.size.width;
        }
        if(position & NSCursorFrameResizePositionTop)
            frame.size.height = MIN(MAX(start.size.height + dy, minSize.height), maxSize.height);
        if(position & NSCursorFrameResizePositionBottom) {
            frame.size.height = MIN(MAX(start.size.height - dy, minSize.height), maxSize.height);
            frame.origin.y = NSMaxY(start) - frame.size.height;
        }
        if(!NSEqualRects(frame, [window frame]))
            [window setFrame:frame display:YES];
        if(next == nil || [next type] == NSLeftMouseUp)
            break;
    }
    [NSCursor pop];
    [window invalidateCursorRectsForView:self];
}

-(NSView *)hitTest:(NSPoint)point {
    /* The controls are drawn again when the one under the pointer changes. */
    if([self _controlAtPoint:point] != [self _controlAtPoint:pointer])
        [self setNeedsDisplayInRect:NSInsetRect(controls, -2, -2)];
    pointer = point;
    /* The edges and corners are the frame's, over the content. */
    if([self _resizePositionAtPoint:[self convertPoint:point fromView:[self superview]]] != 0)
        return self;
    return [super hitTest:point];
}

-(NSWindowBorderType)windowBorderType {
   return _borderType;
}

-(void)setWindowBorderType:(NSWindowBorderType)borderType {
   _borderType = borderType;
   [self setNeedsDisplay:YES];
}

-(void)drawRect:(NSRect)rect {
    /* A sheet: its background and a thin border, no title bar (it hangs
       under its window's). */
    if([[self window] isSheet]) {
        [[[self window] backgroundColor] setFill];
        NSRectFill(rect);
        [_NSControlGray(0.70) set];
        NSFrameRect([self bounds]);
        return;
    }

    NSRect bounds = [NSWindow contentRectForFrameRect:[self bounds]
        styleMask:[[self window] styleMask]];
    float cheatSheet = 0;

    switch(_borderType){
        case NSNoBorder:
            break;

        case NSWindowToolTipBorderType:
            [[NSColor blackColor] setStroke];
            NSFrameRect(bounds);
            bounds = NSInsetRect(bounds, 1, 1);
            cheatSheet = 1;
            break;

        case NSWindowSheetBorderType:
            NSDrawButton(bounds,bounds);
            bounds = NSInsetRect(bounds, 2, 2);
            cheatSheet = 2;
            break;
    }

    NSWindow *window = [self window];
    O2Context *_context = [[window graphicsContext] graphicsPort];
    CGFloat r, g, b, a;
    [[[self window] backgroundColor] getRed:&r green:&g blue:&b alpha:&a];
    O2ContextSetRGBStrokeColor(_context, r, g, b, a);
    O2ContextSetRGBFillColor(_context, r, g, b, a);
    /* A see-through background replaces what was drawn before (copy),
       or an updated status item showed its old title under the new one. */
    if(a < 1) {
        O2ContextSaveGState(_context);
        O2ContextSetBlendMode(_context, kO2BlendModeCopy);
        O2ContextFillRect(_context, bounds);
        O2ContextRestoreGState(_context);
    } else
        O2ContextFillRect(_context, bounds);

    if(!([[self window] styleMask] & NSWindowStyleMaskTitled))
        return;

    if([[self window] isSheet])
        bounds.size.height += cheatSheet;

    /* The frame's grey: Atlas's light one, or its dark one. */
    CGFloat frameGray = _NSSystemAppearanceIsDark() ? 0.176 : 0.8;	/* dark: the windows' grey (23 Nov 2026) */
    O2ContextSetGrayStrokeColor(_context, frameGray, 1);
    O2ContextSetGrayFillColor(_context, frameGray, 1);

    // let's round these corners
    float radius = NSWindowCornerRadius;
    NSRect titlebar = NSMakeRect(0, NSMaxY([self bounds]) - NSWindowTitleHeight, NSWidth([self bounds]), NSWindowTitleHeight);

    /* A rect inside the frame, away from its corners, is filled plainly and
       has no title bar to draw: a small redraw in the content need not fill
       the whole rounded frame and draw the title again. */
    if(NSContainsRect(NSInsetRect([self bounds], 0, radius), rect) ||
       NSContainsRect(NSInsetRect([self bounds], radius, 0), rect)) {
        /* The window's background below the title bar, as the reference
           draws it; the title bar in the frame's grey. */
        O2ContextSetRGBFillColor(_context, r, g, b, a);
        O2ContextFillRect(_context, rect);
        setTitlebarFill(_context, frameGray);
        O2ContextFillRect(_context, NSIntersectionRect(rect, titlebar));
        if(!NSIntersectsRect(rect, titlebar))
            return;
        if(!([window styleMask] & NSWindowStyleMaskFullSizeContentView))
            [self _drawTitlebarControls];
        return;
    }

    O2ContextBeginPath(_context);
    O2ContextMoveToPoint(_context, _frame.origin.x+radius, NSMaxY(_frame));
    O2ContextAddArc(_context, _frame.origin.x + _frame.size.width - radius,
        _frame.origin.y + _frame.size.height - radius, radius, 1.5708 /*radians*/,
        0 /*radians*/, YES);
    O2ContextAddLineToPoint(_context, _frame.origin.x + _frame.size.width,
        _frame.origin.y);
    O2ContextAddArc(_context, _frame.origin.x + _frame.size.width - radius,
        _frame.origin.y + radius, radius, 6.28319 /*radians*/, 4.71239 /*radians*/,
        YES);
    O2ContextAddLineToPoint(_context, _frame.origin.x, _frame.origin.y);
    O2ContextAddArc(_context, _frame.origin.x + radius, _frame.origin.y + radius,
        radius, 4.71239, 3.14159, YES);
    O2ContextAddLineToPoint(_context, _frame.origin.x,
        _frame.origin.y + _frame.size.height);
    O2ContextAddArc(_context, _frame.origin.x + radius, _frame.origin.y +
        _frame.size.height - radius, radius, 3.14159, 1.5708, YES);
    O2ContextAddLineToPoint(_context, _frame.origin.x, NSMaxY(_frame));
    O2ContextClosePath(_context);
    /* Inside the rounded frame: the window's background, and its title bar
       in the frame's grey. */
    O2ContextSaveGState(_context);
    O2ContextClip(_context);
    O2ContextSetRGBFillColor(_context, r, g, b, a);
    O2ContextFillRect(_context, [self bounds]);
    setTitlebarFill(_context, frameGray);
    O2ContextFillRect(_context, titlebar);
    O2ContextRestoreGState(_context);

    // the window's buttons and title, unless drawn above the content
    if(!([window styleMask] & NSWindowStyleMaskFullSizeContentView) && NSIntersectsRect(rect, titlebar))
        [self _drawTitlebarControls];
}

/* The window's buttons, at the left of the title bar, and its title in
   the middle (unless hidden). */
-(void)_drawTitlebarControls {
    NSWindow *window = [self window];
    O2Context *_context = [[window graphicsContext] graphicsPort];

    [self _layoutControls];
    NSUInteger mask = [window styleMask];
    BOOL over = NSPointInRect(pointer, controls);
    if(![window _hidesWindowControls]) {
    if(!NSIsEmptyRect(_closeButtonRect))
    drawControl(_context, _closeButtonRect, kControlClose, (mask & NSClosableWindowMask) != 0,
        over && NSPointInRect(pointer, _closeButtonRect));
    if(!NSIsEmptyRect(_zoomButtonRect))
    drawControl(_context, _zoomButtonRect, kControlZoom, (mask & NSResizableWindowMask) != 0,
        over && NSPointInRect(pointer, _zoomButtonRect));
    if(!NSIsEmptyRect(_miniButtonRect))
    drawControl(_context, _miniButtonRect, kControlMini, (mask & NSMiniaturizableWindowMask) != 0,
        over && NSPointInRect(pointer, _miniButtonRect));
    }

    NSString *t = [window title];
    if(t && [window titleVisibility] == NSWindowTitleVisible) {
        NSDictionary *attrs = @{
            NSFontAttributeName : [NSFont systemFontOfSize:13.0],
            NSForegroundColorAttributeName : _NSSystemAppearanceIsDark() || _NSAppearanceThemeActive()
                                                 ? [NSColor windowFrameTextColor]
                                                 : [NSColor colorWithGenericGamma22White:0.25 alpha:1]
        };
        NSAttributedString *title = [[[NSAttributedString alloc] initWithString:t attributes:attrs] autorelease];
        NSSize size = [title size];
        NSRect titleRect = NSMakeRect(
            _frame.size.width / 2 - size.width / 2,
            floor(_frame.size.height - NSWindowTitleHeight / 2 - size.height / 2),
            size.width, size.height);
        [title drawInRect:titleRect];
    }
}

/* An overlay kept in front of the frame's other views rounds the
   window's corners over the content view, and draws the window's
   buttons and title above a full-size content view. */
-(void)_updateTitlebarOverlay {
    /* Every window with a frame (its corners), not only full-size ones. */
    BOOL framed = ([[self window] styleMask] & NSWindowStyleMaskTitled) && ![[self window] isSheet];

    if(framed && _titlebarOverlay == nil) {
        _titlebarOverlay = [[NSTitlebarOverlayView alloc] initWithFrame:[self bounds]];
        [_titlebarOverlay setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [self addSubview:_titlebarOverlay];
        [_titlebarOverlay setNeedsDisplay:YES];
    } else if(!framed && _titlebarOverlay != nil) {
        [_titlebarOverlay removeFromSuperview];
        [_titlebarOverlay release];
        _titlebarOverlay = nil;
    }
    [_titlebarOverlay setFrame:[self bounds]];
}

-(void)didAddSubview:(NSView *)subview {
    [super didAddSubview:subview];
    if(subview != _titlebarOverlay)
        [self _updateTitlebarOverlay];
    if(_titlebarOverlay != nil && subview != _titlebarOverlay &&
       [[self subviews] lastObject] != _titlebarOverlay) {
        NSView *overlay = _titlebarOverlay;
        [overlay removeFromSuperview];
        [self addSubview:overlay];
    }
}

-(void)resizeSubviewsWithOldSize:(NSSize)oldSize {
   NSToolbarView *toolbarView=nil;
   NSView *contentView=nil,*tabBar=nil;

// tile the subviews, when/if we add titlebars and such do it here
   NSArray *accessories=[[self window] respondsToSelector:@selector(titlebarAccessoryViewControllers)]?
      [[self window] titlebarAccessoryViewControllers]:nil;
   NSMutableArray *accessoryViews=[NSMutableArray array];

   for(NSTitlebarAccessoryViewController *accessory in accessories)
    [accessoryViews addObject:[accessory _titlebarContainerView]];
   [self _updateTitlebarOverlay];
   for(NSView *view in _subviews){
    if(view == _titlebarOverlay || [accessoryViews indexOfObjectIdenticalTo:view]!=NSNotFound)
     continue;
    if([view isKindOfClass:[NSToolbarView class]])
     toolbarView=(NSToolbarView *)view;
    else if([view isKindOfClass:[NSWindowTabBarView class]])
     tabBar=view;
    else if(contentView==nil || view==[[self window] contentView])
     contentView=view;
   }

   NSRect toolbarFrame=(toolbarView!=nil)?[toolbarView frame]:NSZeroRect;
   NSRect contentFrame=[[[self window] class] contentRectForFrameRect:[self bounds] styleMask:[[self window] styleMask]];
   toolbarFrame.origin.y=NSMaxY(contentFrame)-toolbarFrame.size.height;
   toolbarFrame.origin.x=contentFrame.origin.x;
   toolbarFrame.size.width=contentFrame.size.width;

   [toolbarView setFrame:toolbarFrame];
   [toolbarView layoutViews];

   contentFrame.size.height-=toolbarFrame.size.height;
   if(tabBar!=nil){
    contentFrame.size.height-=NSWindowTabBarHeight;
    [tabBar setFrame:NSMakeRect(contentFrame.origin.x,NSMaxY(contentFrame),contentFrame.size.width,NSWindowTabBarHeight)];
   }

   /* Title bar accessories: bottom ones in a band under the toolbar (the
      content below them), left and right ones in the title bar. */
   CGFloat top=NSMaxY(contentFrame),titleTop=NSMaxY([self bounds]),left=NSMinX([self bounds])+8;
   CGFloat right=NSMaxX([self bounds])-80;

   for(NSTitlebarAccessoryViewController *accessory in accessories){
    NSView *view=[accessory _titlebarContainerView];
    NSRect  frame=[view frame];

    if([accessory isHidden]){
     [view setHidden:YES];
     continue;
    }
    [view setHidden:NO];
    switch([accessory layoutAttribute]){
     case NSLayoutAttributeLeft:
     case NSLayoutAttributeLeading:
      frame=NSMakeRect(left,titleTop-NSWindowTitleHeight,NSWidth(frame),NSWindowTitleHeight);
      left+=NSWidth(frame);
      break;
     case NSLayoutAttributeRight:
     case NSLayoutAttributeTrailing:
      right-=NSWidth(frame);
      frame=NSMakeRect(right,titleTop-NSWindowTitleHeight,NSWidth(frame),NSWindowTitleHeight);
      break;
     default:
      frame=NSMakeRect(contentFrame.origin.x,top-NSHeight(frame),contentFrame.size.width,NSHeight(frame));
      top-=NSHeight(frame);
      contentFrame.size.height-=NSHeight(frame);
      break;
    }
    [view setFrame:frame];
   }
   [contentView setFrame:contentFrame];
}

/* The title bar's height: the window's, or a full-size content view's own
   taller bar (_setTitlebarHeight:). An app may change it (as on the
   reference platform, where it is this frame view's). */
-(CGFloat)_titlebarHeight {
    NSWindow *window = [self window];

    if(([window styleMask] & NSWindowStyleMaskFullSizeContentView) && [window _titlebarHeight] > 0)
        return [window _titlebarHeight];
    return NSWindowTitleHeight;
}

/* Where the close button's frame starts (its bottom left, in this view),
   centered in the title bar: 7 points from the left in a plain title bar,
   20 in a taller unified one (a toolbar's, as the reference platform lays
   them out). The others follow 20 points apart. Apps that draw into the
   title bar move the three by giving their own (Firefox centers them in
   its tab strip). */
-(NSPoint)_closeButtonOrigin {
    NSSize  size = [NSWindow _standardWindowButtonSize];
    CGFloat bar = [self _titlebarHeight];

    return NSMakePoint(bar > NSWindowTitleHeight ? 20 : NSWindowControlSpacing - 1,
        floor(NSHeight([self bounds]) - bar / 2 - size.height / 2));
}

/* At the left of the title bar: close, minimize, zoom, in the reference
   platform's order, sizes and spacing, which apps lay out their title bar
   contents around (their look stays Atlas's own). Each button's frame is
   14 x 16 points with the 12-point control in its middle. */
-(void)_layoutControls {
    NSWindow *window = [self window];
    NSSize size = [NSWindow _standardWindowButtonSize];
    NSPoint origin = [self _closeButtonOrigin];
    CGFloat step = NSWindowControlSpacing + NSWindowControlDiameter;
    NSRect frames[3];

    for(int i = 0; i < 3; i++) {
        frames[i] = NSMakeRect(origin.x + i * step, origin.y, size.width, size.height);
        [window _setStandardButtonFrame:frames[i] kind:i];
    }
    NSRect (^circle)(NSRect) = ^NSRect(NSRect f) {
        return NSMakeRect(floor(NSMidX(f) - NSWindowControlDiameter / 2),
            floor(NSMidY(f) - NSWindowControlDiameter / 2), NSWindowControlDiameter, NSWindowControlDiameter);
    };
    /* A button the app hid takes no room and no clicks. */
    _closeButtonRect = [window _isStandardButtonHidden:NSWindowCloseButton] ? NSZeroRect : circle(frames[0]);
    _miniButtonRect = [window _isStandardButtonHidden:NSWindowMiniaturizeButton] ? NSZeroRect : circle(frames[1]);
    _zoomButtonRect = [window _isStandardButtonHidden:NSWindowZoomButton] ? NSZeroRect : circle(frames[2]);
    controls = NSUnionRect(NSUnionRect(_closeButtonRect, _miniButtonRect), _zoomButtonRect);
}

-(NSRect)_windowControlsRect {
    return NSUnionRect(NSUnionRect(_closeButtonRect, _miniButtonRect), _zoomButtonRect);
}

/* The title bar's double-click action, done when the button comes up. */
/* The zoom button's menu: Enter (Exit) Full Screen, Tile Window to Left
   of Screen, Tile Window to Right of Screen, for the window. */
-(void)_popUpZoomMenu {
    NSWindow *window = [self window];
    NSBundle *appKit = [NSBundle bundleForClass:[NSThemeFrame class]];
    NSMenu   *menu = [[[NSMenu alloc] initWithTitle:@""] autorelease];
    struct { NSString *title; SEL action; } items[] = {
        { @"Enter Full Screen", @selector(toggleFullScreen:) },
        { @"Tile Window to Left of Screen", @selector(_tileWindowToLeftOfScreen:) },
        { @"Tile Window to Right of Screen", @selector(_tileWindowToRightOfScreen:) },
    };

    for(size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
        NSMenuItem *item = [menu addItemWithTitle:[appKit localizedStringForKey:items[i].title value:items[i].title table:nil]
            action:items[i].action keyEquivalent:@""];
        [item setTarget:window];
    }
    [menu popUpMenuPositioningItem:nil atLocation:NSMakePoint(NSMinX(_zoomButtonRect), NSMinY(_zoomButtonRect) - 2) inView:self];
}

static char titleBarActionKey;

-(void)mouseUp:(NSEvent *)event {
    NSString *action = objc_getAssociatedObject(self, &titleBarActionKey);

    if(action == nil) {
        [super mouseUp:event];
        return;
    }
    objc_setAssociatedObject(self, &titleBarActionKey, nil, OBJC_ASSOCIATION_RETAIN);
    if([action isEqualToString:@"Minimize"])
        [[self window] performMiniaturize:self];
    else
        [[self window] performZoom:self];
}

-(void)mouseDown:(NSEvent *)event {
    CGFloat top, left, right, bottom;
    CGNativeBorderFrameWidthsForStyle([[self window] styleMask], &top, &left, &bottom, &right);
    NSPoint pos = [event locationInWindow];
    NSUInteger mask = [[self window] styleMask];
    NSCursorFrameResizePosition edge = [self _resizePositionAtPoint:[self convertPoint:pos fromView:nil]];

    if(edge != 0) {
        [self _resizeFrom:edge withEvent:event];
        return;
    }

    [self _layoutControls];
    if(NSPointInRect(pos, controls) && ![[self window] _hidesWindowControls]) {
        if(NSPointInRect(pos, _closeButtonRect) && (mask & NSClosableWindowMask))
            [[self window] performClose:self];
        else if(NSPointInRect(pos, _miniButtonRect) && (mask & NSMiniaturizableWindowMask))
            [[self window] performMiniaturize:self];
        else if(NSPointInRect(pos, _zoomButtonRect) && (mask & NSResizableWindowMask)) {
            /* Held (half a second): Enter Full Screen and the tiles, in a
               menu under the button, as on the reference; else a zoom. */
            NSEvent *up = [NSApp nextEventMatchingMask:NSLeftMouseUpMask
                untilDate:[NSDate dateWithTimeIntervalSinceNow:0.5] inMode:NSEventTrackingRunLoopMode dequeue:YES];
            if(up != nil || [[self window] isSheet])
                [[self window] performZoom:self];
            else
                [self _popUpZoomMenu];
        }
    }
    else if(pos.y > (NSMaxY(_frame) - top) && [event clickCount] == 2) {
        /* Minkar: a double click on the title bar does what the user chose
           (AppleActionOnDoubleClick: Maximize, the default, Minimize or None;
           before it, AppleMiniaturizeOnDoubleClick), as on the reference,
           when the button comes up: the title bar is a drag region, so the
           server is moving the window until then and keeps its frame. */
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        NSString *action;

        [defaults synchronize];     /* as changed in System Preferences meanwhile */
        action = [defaults stringForKey:@"AppleActionOnDoubleClick"];

        if(action == nil && [defaults objectForKey:@"AppleMiniaturizeOnDoubleClick"] != nil)
            action = [defaults boolForKey:@"AppleMiniaturizeOnDoubleClick"] ? @"Minimize" : @"Maximize";
        if([action isEqualToString:@"Minimize"] && (mask & NSMiniaturizableWindowMask))
            objc_setAssociatedObject(self, &titleBarActionKey, @"Minimize", OBJC_ASSOCIATION_RETAIN);
        else if(action != nil && ![action isEqualToString:@"Maximize"])
            ;
        else if(mask & NSResizableWindowMask)
            objc_setAssociatedObject(self, &titleBarActionKey, @"Maximize", OBJC_ASSOCIATION_RETAIN);
    }
    else if(pos.y > (NSMaxY(_frame) - top)) // in titlebar?
        [[self window] requestMove:event];
    else if([[self window] isMovableByWindowBackground])
        [[self window] requestMove:event];
}

@end
