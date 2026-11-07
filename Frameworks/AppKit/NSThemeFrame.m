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
#import <AppKit/NSAttributedString.h>
#import <Onyx2D/O2Context.h>

extern void CGNativeBorderFrameWidthsForStyle(unsigned styleMask, CGFloat *top,
    CGFloat *left, CGFloat *bottom, CGFloat *right);

/* Above a full-size content view (NSWindowStyleMaskFullSizeContentView):
 * the window's buttons and its title, drawn over the content; it takes
 * only the buttons' clicks. It covers the frame view, in its coordinates. */
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
    return NSPointInRect(point, NSInsetRect([frame _windowControlsRect], -2, -2)) ? self : nil;
}

-(void)mouseDown:(NSEvent *)event {
    [[self superview] mouseDown:event];
}

-(void)drawRect:(NSRect)rect {
    [(NSThemeFrame *)[self superview] _drawTitlebarControls];
}

@end

@implementation NSThemeFrame

// all measurements in pixels
const float NSWindowTitleHeight = 32;
const float NSWindowEdgePad = 2;
const float NSWindowCornerRadius = 8;
const float NSWindowControlDiameter = 14;
const float NSWindowControlSpacing = 10;

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
        CGFloat inset = 4, midX = NSMidX(rect), midY = NSMidY(rect);
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
-(int)_controlAtPoint:(NSPoint)point {
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
    if(!([window styleMask] & NSWindowStyleMaskFullSizeContentView))
        [self _drawTitlebarControls];
}

/* The window's buttons, at the right of the title bar, and its title in
   the middle (unless hidden). */
-(void)_drawTitlebarControls {
    NSWindow *window = [self window];
    O2Context *_context = [[window graphicsContext] graphicsPort];

    [self _layoutControls];
    NSUInteger mask = [window styleMask];
    BOOL over = NSPointInRect(pointer, controls);
    drawControl(_context, _closeButtonRect, kControlClose, (mask & NSClosableWindowMask) != 0,
        over && NSPointInRect(pointer, _closeButtonRect));
    drawControl(_context, _zoomButtonRect, kControlZoom, (mask & NSResizableWindowMask) != 0,
        over && NSPointInRect(pointer, _zoomButtonRect));
    drawControl(_context, _miniButtonRect, kControlMini, (mask & NSMiniaturizableWindowMask) != 0,
        over && NSPointInRect(pointer, _miniButtonRect));

    NSString *t = [window title];
    if(t && [window titleVisibility] == NSWindowTitleVisible) {
        NSDictionary *attrs = @{
            NSFontAttributeName : [NSFont titleBarFontOfSize:15.0],
            NSForegroundColorAttributeName : [NSColor blackColor]
        };
        NSAttributedString *title = [[[NSAttributedString alloc] initWithString:t attributes:attrs] autorelease];
        NSSize size = [title size];
        NSRect titleRect = NSMakeRect(
            _frame.size.width / 2 - size.width / 2,
            _frame.size.height - NSWindowTitleHeight / 3 - size.height / 2,
            size.width, size.height);
        [title drawInRect:titleRect];
    }
}

/* A full-size content view has the window's buttons and title drawn above
   it, by an overlay kept in front of the frame's other views. */
-(void)_updateTitlebarOverlay {
    BOOL full = ([[self window] styleMask] & NSWindowStyleMaskFullSizeContentView) != 0;

    if(full && _titlebarOverlay == nil) {
        _titlebarOverlay = [[NSTitlebarOverlayView alloc] initWithFrame:[self bounds]];
        [_titlebarOverlay setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [self addSubview:_titlebarOverlay];
        [_titlebarOverlay setNeedsDisplay:YES];
    } else if(!full && _titlebarOverlay != nil) {
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
   [self _updateTitlebarOverlay];
   for(NSView *view in _subviews){
    if(view == _titlebarOverlay)
     continue;
    if([view isKindOfClass:[NSToolbarView class]])
     toolbarView=(NSToolbarView *)view;
    else
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
   [contentView setFrame:contentFrame];
}

/* Close at the right of the title bar, zoom and minimize left of it. */
-(void)_layoutControls {
    NSRect bounds = [self bounds];
    NSRect button = NSMakeRect(NSWidth(bounds) - NSWindowControlSpacing - NSWindowControlDiameter,
        NSHeight(bounds) - NSWindowTitleHeight / 2 - NSWindowControlDiameter / 2,
        NSWindowControlDiameter, NSWindowControlDiameter);
    _closeButtonRect = button;
    button.origin.x -= NSWindowControlSpacing + NSWindowControlDiameter;
    _zoomButtonRect = button;
    button.origin.x -= NSWindowControlSpacing + NSWindowControlDiameter;
    _miniButtonRect = button;
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
    if(NSPointInRect(pos, controls)) {
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
