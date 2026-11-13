/*
 * Copyright (c) 2006-2007 Christopher J. W. Lloyd
 * Copyright (C) 2024 Zoe Knox <zoe@ravynsoft.com>
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

#import <AppKit/NSThemeFrame.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSWindow-Private.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSToolbarView.h>
#import <AppKit/NSTitlebarAccessoryViewController.h>

/* The view the frame places for an accessory (NSTitlebarAccessoryViewController.m). */
@interface NSTitlebarAccessoryViewController (NSThemeFrame)
-(NSView *)_titlebarContainerView;
@end
#import <AppKit/NSAttributedString.h>
#import <Onyx2D/O2Context.h>

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

        O2ContextSetGrayStrokeColor(context, 0.8, 1);
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

@implementation NSThemeFrame


enum { kControlClose, kControlMini, kControlZoom };

/* Atlas's window controls, drawn (no artwork): grey discs with their
 * sign, close rightmost, then zoom and minimize; close turns red under
 * the pointer; a control the window lacks is faint and has no sign. */
static void drawControl(O2Context *context, NSRect rect, int kind, BOOL enabled, BOOL hover) {
    O2ContextSaveGState(context);
    if(!enabled)
        O2ContextSetGrayFillColor(context, 0.86, 1);
    else if(hover && kind == kControlClose)
        O2ContextSetRGBFillColor(context, 0.86, 0.33, 0.29, 1);
    else
        O2ContextSetGrayFillColor(context, hover ? 0.62 : 0.72, 1);
    O2ContextFillEllipseInRect(context, rect);
    if(enabled) {
        CGFloat inset = 3.5, midX = NSMidX(rect), midY = NSMidY(rect);
        CGFloat lo = NSMinX(rect) + inset, hi = NSMaxX(rect) - inset;
        CGFloat bottom = NSMinY(rect) + inset, top = NSMaxY(rect) - inset;
        O2Point lines[4];
        unsigned count = 0;

        O2ContextSetGrayStrokeColor(context, (hover && kind == kControlClose) ? 1 : 0.22, 1);
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

-(NSView *)hitTest:(NSPoint)point {
    /* The controls are drawn again when the one under the pointer changes. */
    if([self _controlAtPoint:point] != [self _controlAtPoint:pointer])
        [self setNeedsDisplayInRect:NSInsetRect(controls, -2, -2)];
    pointer = point;
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
        [[NSColor colorWithCalibratedWhite:0.70 alpha:1] set];
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

    if([[self window] styleMask] == NSBorderlessWindowMask)
        return;

    if([[self window] isSheet])
        bounds.size.height += cheatSheet;

    O2ContextSetGrayStrokeColor(_context, 0.8, 1);
    O2ContextSetGrayFillColor(_context, 0.8, 1);

    // let's round these corners
    float radius = NSWindowCornerRadius;
    NSRect titlebar = NSMakeRect(0, NSMaxY([self bounds]) - NSWindowTitleHeight, NSWidth([self bounds]), NSWindowTitleHeight);

    /* A rect inside the frame, away from its corners, is filled plainly and
       has no title bar to draw: a small redraw in the content need not fill
       the whole rounded frame and draw the title again. */
    if(NSContainsRect(NSInsetRect([self bounds], 0, radius), rect) ||
       NSContainsRect(NSInsetRect([self bounds], radius, 0), rect)) {
        O2ContextFillRect(_context, rect);
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
    O2ContextFillPath(_context);

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
    drawControl(_context, _closeButtonRect, kControlClose, (mask & NSClosableWindowMask) != 0,
        over && NSPointInRect(pointer, _closeButtonRect));
    drawControl(_context, _zoomButtonRect, kControlZoom, (mask & NSResizableWindowMask) != 0,
        over && NSPointInRect(pointer, _zoomButtonRect));
    drawControl(_context, _miniButtonRect, kControlMini, (mask & NSMiniaturizableWindowMask) != 0,
        over && NSPointInRect(pointer, _miniButtonRect));
    }

    NSString *t = [window title];
    if(t && [window titleVisibility] == NSWindowTitleVisible) {
        NSDictionary *attrs = @{
            NSFontAttributeName : [NSFont systemFontOfSize:13.0],
            NSForegroundColorAttributeName : [NSColor colorWithCalibratedWhite:0.25 alpha:1]
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
    BOOL framed = [[self window] styleMask] != NSBorderlessWindowMask && ![[self window] isSheet];

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
   NSView *contentView=nil;

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

/* At the left of the title bar: close, minimize, zoom, in the reference
   platform's order and spacing, which apps lay out their title bar
   contents around (their look stays Atlas's own). */
-(void)_layoutControls {
    NSRect bounds = [self bounds];
    NSWindow *window = [self window];
    CGFloat bar = NSWindowTitleHeight;

    /* A full-size content view may draw a taller bar: centered in it. */
    if(([window styleMask] & NSWindowStyleMaskFullSizeContentView) && [window _titlebarHeight] > 0)
        bar = [window _titlebarHeight];
    NSRect button = NSMakeRect(NSWindowControlSpacing,
        floor(NSHeight(bounds) - bar / 2 - NSWindowControlDiameter / 2),
        NSWindowControlDiameter, NSWindowControlDiameter);
    _closeButtonRect = button;
    button.origin.x += NSWindowControlSpacing + NSWindowControlDiameter;
    _miniButtonRect = button;
    button.origin.x += NSWindowControlSpacing + NSWindowControlDiameter;
    _zoomButtonRect = button;
    controls = NSUnionRect(NSUnionRect(_closeButtonRect, _miniButtonRect), _zoomButtonRect);
}

-(NSRect)_windowControlsRect {
    return NSUnionRect(NSUnionRect(_closeButtonRect, _miniButtonRect), _zoomButtonRect);
}

-(void)mouseDown:(NSEvent *)event {
    CGFloat top, left, right, bottom;
    CGNativeBorderFrameWidthsForStyle([[self window] styleMask], &top, &left, &bottom, &right);
    NSPoint pos = [event locationInWindow];
    NSUInteger mask = [[self window] styleMask];

    [self _layoutControls];
    if(NSPointInRect(pos, controls) && ![[self window] _hidesWindowControls]) {
        if(NSPointInRect(pos, _closeButtonRect) && (mask & NSClosableWindowMask))
            [[self window] performClose:self];
        else if(NSPointInRect(pos, _miniButtonRect) && (mask & NSMiniaturizableWindowMask))
            [[self window] performMiniaturize:self];
        else if(NSPointInRect(pos, _zoomButtonRect) && (mask & NSResizableWindowMask))
            [[self window] performZoom:self];
    }
    else if(pos.y > (NSMaxY(_frame) - top)) // in titlebar?
        [[self window] requestMove:event];
    else if([[self window] isMovableByWindowBackground])
        [[self window] requestMove:event];
}

@end
