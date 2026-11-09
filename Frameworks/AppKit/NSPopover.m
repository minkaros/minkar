/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A popover: a borderless child window of the positioning view's window,
 * drawn as a rounded bubble with an arrow at the rect it points to. A
 * transient one closes when its window stops being key (a click
 * elsewhere) or on Escape.
 */

#import <AppKit/NSPopover.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSView.h>
#import <AppKit/NSControl.h>
#import <AppKit/NSViewController.h>
#import <AppKit/NSWindow.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSNotification.h>

NSString *const NSPopoverCloseReasonKey = @"NSPopoverCloseReasonKey";
NSString *const NSPopoverCloseReasonStandard = @"NSPopoverCloseReasonStandard";
NSString *const NSPopoverCloseReasonDetachToWindow = @"NSPopoverCloseReasonDetachToWindow";
NSString *const NSPopoverWillShowNotification = @"NSPopoverWillShowNotification";
NSString *const NSPopoverDidShowNotification = @"NSPopoverDidShowNotification";
NSString *const NSPopoverWillCloseNotification = @"NSPopoverWillCloseNotification";
NSString *const NSPopoverDidCloseNotification = @"NSPopoverDidCloseNotification";

#define ARROW 10.0      /* the arrow's length */
#define ARROW_HALF 9.0  /* half its base */
#define RADIUS 6.0
#define INSET 1.0       /* the border's room around the content */

/* The bubble and its arrow, which points out of the edge the popover sits on. */
@interface NSPopoverFrameView : NSView {
@public
    NSRectEdge _arrowEdge;   /* the bubble's side the arrow leaves from */
    CGFloat _arrowPosition;  /* along that side, from its start */
}
@end

@implementation NSPopoverFrameView

- (BOOL)isOpaque {
    return NO;
}

- (NSRect)bubbleRect {
    NSRect r = [self bounds];

    switch(_arrowEdge) {
        case NSMinYEdge: r.origin.y += ARROW; r.size.height -= ARROW; break;
        case NSMaxYEdge: r.size.height -= ARROW; break;
        case NSMinXEdge: r.origin.x += ARROW; r.size.width -= ARROW; break;
        case NSMaxXEdge: r.size.width -= ARROW; break;
    }
    return NSInsetRect(r, 0.5, 0.5);
}

/* The arrow's two slanted sides, their base on the bubble's edge; with
   inset the base moves that far into the bubble (to fill over its
   outline) while the tip stays where it is. */
- (NSBezierPath *)arrowSidesInset:(CGFloat)inset {
    NSRect b = [self bubbleRect];
    NSBezierPath *path = [NSBezierPath bezierPath];
    CGFloat p = _arrowPosition;

    switch(_arrowEdge) {
        case NSMinYEdge:
            [path moveToPoint:NSMakePoint(NSMinX(b) + p - ARROW_HALF, NSMinY(b) + inset)];
            [path lineToPoint:NSMakePoint(NSMinX(b) + p, NSMinY(b) - ARROW + 0.5)];
            [path lineToPoint:NSMakePoint(NSMinX(b) + p + ARROW_HALF, NSMinY(b) + inset)];
            break;
        case NSMaxYEdge:
            [path moveToPoint:NSMakePoint(NSMinX(b) + p - ARROW_HALF, NSMaxY(b) - inset)];
            [path lineToPoint:NSMakePoint(NSMinX(b) + p, NSMaxY(b) + ARROW - 0.5)];
            [path lineToPoint:NSMakePoint(NSMinX(b) + p + ARROW_HALF, NSMaxY(b) - inset)];
            break;
        case NSMinXEdge:
            [path moveToPoint:NSMakePoint(NSMinX(b) + inset, NSMinY(b) + p - ARROW_HALF)];
            [path lineToPoint:NSMakePoint(NSMinX(b) - ARROW + 0.5, NSMinY(b) + p)];
            [path lineToPoint:NSMakePoint(NSMinX(b) + inset, NSMinY(b) + p + ARROW_HALF)];
            break;
        case NSMaxXEdge:
            [path moveToPoint:NSMakePoint(NSMaxX(b) - inset, NSMinY(b) + p - ARROW_HALF)];
            [path lineToPoint:NSMakePoint(NSMaxX(b) + ARROW - 0.5, NSMinY(b) + p)];
            [path lineToPoint:NSMakePoint(NSMaxX(b) - inset, NSMinY(b) + p + ARROW_HALF)];
            break;
    }
    return path;
}

/* The bubble's outline, then the arrow filled over it (hiding the outline
   at its base) and its sides stroked: one shape. */
- (void)drawRect:(NSRect)rect {
    NSColor *fill = [NSColor colorWithCalibratedWhite:0.99 alpha:1];
    NSColor *border = [NSColor colorWithCalibratedWhite:0.68 alpha:1];
    NSBezierPath *bubble = [NSBezierPath bezierPathWithRoundedRect:[self bubbleRect] xRadius:RADIUS yRadius:RADIUS];
    NSBezierPath *sides = [self arrowSidesInset:0];
    /* Filled from 1.5 points inside the bubble: no outline left at its base. */
    NSBezierPath *arrow = [self arrowSidesInset:1.5];

    [[NSColor clearColor] setFill];
    NSRectFillUsingOperation(rect, NSCompositeCopy);
    [fill setFill];
    [bubble fill];
    [border setStroke];
    [bubble stroke];
    [arrow closePath];
    [fill setFill];
    [arrow fill];
    [border setStroke];
    [sides stroke];
}

@end

/* The popover's window: borderless, see-through outside the bubble, key
   so that it hears Escape and knows when a click went elsewhere. */
@interface NSPopoverWindow : NSPanel {
@public
    NSPopover *_popover;
}
@end

@implementation NSPopoverWindow

- (BOOL)canBecomeKeyWindow {
    return YES;
}

- (BOOL)canBecomeMainWindow {
    return NO;
}

- (void)cancelOperation:(id)sender {
    [_popover performClose:sender];
}

- (void)keyDown:(NSEvent *)event {
    if([[event charactersIgnoringModifiers] isEqualToString:@"\e"])
        [_popover performClose:self];
    else
        [super keyDown:event];
}

@end

@interface NSPopover ()
- (void)_layout;
@end

@implementation NSPopover

- (instancetype)init {
    if((self = [super init]) != nil) {
        _behavior = NSPopoverBehaviorApplicationDefined;
        _animates = YES;
    }
    return self;
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_window orderOut:nil];
    [_window release];
    [_contentViewController release];
    [super dealloc];
}

- (NSViewController *)contentViewController { return _contentViewController; }

- (void)setContentViewController:(NSViewController *)controller {
    [controller retain];
    [_contentViewController release];
    _contentViewController = controller;
}

/* Unless set, the content view's size. */
- (NSSize)contentSize {
    if(_contentSize.width > 0 && _contentSize.height > 0)
        return _contentSize;
    return [[_contentViewController view] frame].size;
}

- (void)setContentSize:(NSSize)size {
    _contentSize = size;
    if(_shown)
        [self _layout];
}

- (NSPopoverBehavior)behavior { return _behavior; }
- (void)setBehavior:(NSPopoverBehavior)behavior { _behavior = behavior; }
- (BOOL)animates { return _animates; }
- (void)setAnimates:(BOOL)animates { _animates = animates; }
- (NSPopoverAppearance)appearance { return (NSPopoverAppearance)_appearance; }
- (void)setAppearance:(NSPopoverAppearance)appearance { _appearance = appearance; }
- (id <NSPopoverDelegate>)delegate { return _delegate; }
- (void)setDelegate:(id <NSPopoverDelegate>)delegate { _delegate = delegate; }
- (BOOL)isShown { return _shown; }
- (BOOL)isDetached { return NO; }
- (NSRect)positioningRect { return _positioningRect; }

- (void)setPositioningRect:(NSRect)rect {
    _positioningRect = rect;
    if(_shown)
        [self _layout];
}

/* The anchor in screen coordinates. */
- (NSRect)_anchorOnScreen {
    NSRect rect = NSIsEmptyRect(_positioningRect) ? [_positioningView bounds] : _positioningRect;
    NSRect inWindow = [_positioningView convertRect:rect toView:nil];
    NSRect frame = [[_positioningView window] frame];

    inWindow.origin.x += frame.origin.x;
    inWindow.origin.y += frame.origin.y;
    return inWindow;
}

/* The preferred edge in the view's terms (NSMaxYEdge is below a flipped
   view's rect) to a side on screen; the other side when there is no room. */
- (NSRectEdge)_screenEdgeForSize:(NSSize)size anchor:(NSRect)anchor {
    NSRectEdge edge = _preferredEdge;
    NSRect visible = [[[_positioningView window] screen] visibleFrame];

    /* Controls are flipped on the reference platform, so a button's
       NSMaxYEdge is below it there: taken so for any control here too. */
    if([_positioningView isFlipped] || [_positioningView isKindOfClass:[NSControl class]]) {
        if(edge == NSMinYEdge)
            edge = NSMaxYEdge;
        else if(edge == NSMaxYEdge)
            edge = NSMinYEdge;
    }
    if(NSIsEmptyRect(visible))
        return edge;
    switch(edge) {
        case NSMaxYEdge: if(NSMaxY(anchor) + size.height > NSMaxY(visible)) edge = NSMinYEdge; break;
        case NSMinYEdge: if(NSMinY(anchor) - size.height < NSMinY(visible)) edge = NSMaxYEdge; break;
        case NSMaxXEdge: if(NSMaxX(anchor) + size.width > NSMaxX(visible)) edge = NSMinXEdge; break;
        case NSMinXEdge: if(NSMinX(anchor) - size.width < NSMinX(visible)) edge = NSMaxXEdge; break;
    }
    return edge;
}

- (void)_layout {
    NSSize content = [self contentSize];
    BOOL vertical;
    NSSize size;
    NSRect anchor = [self _anchorOnScreen];
    NSRectEdge edge;
    NSRect frame, inner;
    NSPopoverFrameView *frameView = (NSPopoverFrameView *)[_window contentView];
    NSView *view = [_contentViewController view];

    size = NSMakeSize(content.width + 2 * INSET, content.height + 2 * INSET);
    edge = [self _screenEdgeForSize:size anchor:anchor];
    vertical = (edge == NSMinYEdge || edge == NSMaxYEdge);
    if(vertical)
        size.height += ARROW;
    else
        size.width += ARROW;

    /* The window on the chosen side, centered on the anchor, kept on screen. */
    switch(edge) {
        case NSMaxYEdge: frame.origin = NSMakePoint(NSMidX(anchor) - size.width / 2, NSMaxY(anchor)); break;
        case NSMinYEdge: frame.origin = NSMakePoint(NSMidX(anchor) - size.width / 2, NSMinY(anchor) - size.height); break;
        case NSMaxXEdge: frame.origin = NSMakePoint(NSMaxX(anchor), NSMidY(anchor) - size.height / 2); break;
        default:         frame.origin = NSMakePoint(NSMinX(anchor) - size.width, NSMidY(anchor) - size.height / 2); break;
    }
    frame.size = size;
    NSRect visible = [[[_positioningView window] screen] visibleFrame];
    if(!NSIsEmptyRect(visible)) {
        if(vertical)
            frame.origin.x = MAX(NSMinX(visible), MIN(frame.origin.x, NSMaxX(visible) - size.width));
        else
            frame.origin.y = MAX(NSMinY(visible), MIN(frame.origin.y, NSMaxY(visible) - size.height));
    }
    frame = NSIntegralRect(frame);

    /* The arrow leaves the bubble on the side facing the anchor. */
    switch(edge) {
        case NSMaxYEdge: frameView->_arrowEdge = NSMinYEdge; break;
        case NSMinYEdge: frameView->_arrowEdge = NSMaxYEdge; break;
        case NSMaxXEdge: frameView->_arrowEdge = NSMinXEdge; break;
        default:         frameView->_arrowEdge = NSMaxXEdge; break;
    }
    if(vertical)
        frameView->_arrowPosition = MAX(RADIUS + ARROW_HALF, MIN(NSMidX(anchor) - frame.origin.x, size.width - RADIUS - ARROW_HALF));
    else
        frameView->_arrowPosition = MAX(RADIUS + ARROW_HALF, MIN(NSMidY(anchor) - frame.origin.y, size.height - RADIUS - ARROW_HALF));

    [_window setFrame:frame display:NO];
    inner = NSInsetRect([frameView bubbleRect], INSET - 0.5, INSET - 0.5);
    [view setFrame:NSIntegralRect(inner)];
    if([view superview] != frameView)
        [frameView addSubview:view];
    [frameView setNeedsDisplay:YES];
}

- (void)_post:(NSString *)name selector:(SEL)selector userInfo:(NSDictionary *)info {
    NSNotification *note = [NSNotification notificationWithName:name object:self userInfo:info];

    if([_delegate respondsToSelector:selector])
        [_delegate performSelector:selector withObject:note];
    [[NSNotificationCenter defaultCenter] postNotification:note];
}

- (void)showRelativeToRect:(NSRect)positioningRect ofView:(NSView *)positioningView preferredEdge:(NSRectEdge)preferredEdge {
    if(positioningView == nil || [positioningView window] == nil)
        [NSException raise:NSInternalInconsistencyException format:@"NSPopover needs a view in a window to show relative to"];
    if(_contentViewController == nil)
        [NSException raise:NSInternalInconsistencyException format:@"NSPopover has no contentViewController"];

    _positioningView = positioningView;
    _positioningRect = positioningRect;
    _preferredEdge = preferredEdge;

    if(_window == nil) {
        NSPopoverWindow *window = [[NSPopoverWindow alloc] initWithContentRect:NSMakeRect(0, 0, 10, 10)
            styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
        NSPopoverFrameView *frameView = [[[NSPopoverFrameView alloc] initWithFrame:NSMakeRect(0, 0, 10, 10)] autorelease];

        window->_popover = self;
        [window setOpaque:NO];
        [window setBackgroundColor:[NSColor clearColor]];
        /* No window shadow: it would follow the whole rect, not the bubble. */
        [window setHasShadow:NO];
        [window setReleasedWhenClosed:NO];
        [window setContentView:frameView];
        _window = window;
    }

    if(!_shown)
        [self _post:NSPopoverWillShowNotification selector:@selector(popoverWillShow:) userInfo:nil];
    [self _layout];
    [[positioningView window] addChildWindow:_window ordered:NSWindowAbove];
    [_window makeKeyAndOrderFront:nil];
    [_window makeFirstResponder:[_contentViewController view]];
    if(!_shown) {
        _shown = YES;
        [self retain];   /* kept while shown, as the reference platform's is */
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowDidResignKey:)
            name:NSWindowDidResignKeyNotification object:_window];
        [self _post:NSPopoverDidShowNotification selector:@selector(popoverDidShow:) userInfo:nil];
    }
}

/* A click elsewhere: a transient popover closes. */
- (void)_windowDidResignKey:(NSNotification *)note {
    if(_behavior != NSPopoverBehaviorApplicationDefined)
        [self performClose:self];
}

- (void)performClose:(id)sender {
    if(!_shown)
        return;
    if([_delegate respondsToSelector:@selector(popoverShouldClose:)] && ![_delegate popoverShouldClose:self])
        return;
    [self close];
}

- (void)close {
    NSDictionary *info = [NSDictionary dictionaryWithObject:NSPopoverCloseReasonStandard forKey:NSPopoverCloseReasonKey];

    if(!_shown)
        return;
    [self _post:NSPopoverWillCloseNotification selector:@selector(popoverWillClose:) userInfo:info];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidResignKeyNotification object:_window];
    _shown = NO;
    [[_window parentWindow] removeChildWindow:_window];
    [_window orderOut:nil];
    [[_positioningView window] makeKeyWindow];
    [self _post:NSPopoverDidCloseNotification selector:@selector(popoverDidClose:) userInfo:info];
    [self autorelease];
}

@end
