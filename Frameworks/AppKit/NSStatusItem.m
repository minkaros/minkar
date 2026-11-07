/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#import <AppKit/NSStatusItem.h>
#import <AppKit/NSStatusBar.h>
#import <AppKit/NSStatusBarButton.h>
#import <AppKit/NSButtonCell.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSPanel.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSWindow-Private.h>
#import <AppKit/NSGraphicsStyle.h>
#import <AppKit/NSParagraphStyle.h>
#import <AppKit/NSAttributedString.h>
#import "NSMenu.subproj/NSMenuView.h"
#include <SkyLight/SkyLight.h>

extern int _CGUsesSkyLight(void);

/* Space around a variable-length item's title or image. */
static const CGFloat kItemPadding = 8;

@interface NSStatusItem ()
- (NSInteger)_actionMask;
- (void)_updateLength;
- (void)_showMenu:(NSMenu *)menu;
@end

/* The window holding a status item: the window server places it. */
@interface NSStatusBarWindow : NSPanel
@end

@implementation NSStatusBarWindow

- (BOOL)canBecomeKeyWindow {
    return NO;
}

- (BOOL)canBecomeMainWindow {
    return NO;
}

/* Placed in the menu bar by the window server, not kept below it. */
- (NSRect)constrainFrameRect:(NSRect)rect toScreen:(NSScreen *)screen {
    return rect;
}

@end

@implementation NSStatusBarButton {
    NSStatusItem *_statusItem;	/* not retained: the item owns the button */
}

@synthesize appearsDisabled = _appearsDisabled;

- (void)_setStatusItem:(NSStatusItem *)item {
    _statusItem = item;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    return YES;
}

/* With a menu, a press opens it below the item; otherwise the button
   tracks and sends its action. */
- (void)mouseDown:(NSEvent *)event {
    if([_statusItem menu] != nil)
        [_statusItem _showMenu:[_statusItem menu]];
    else if([_statusItem _actionMask] & NSLeftMouseDownMask)
        [self sendAction:[self action] to:[self target]];
    else
        [super mouseDown:event];
}

- (void)rightMouseDown:(NSEvent *)event {
    if([_statusItem menu] != nil)
        [_statusItem _showMenu:[_statusItem menu]];
    else
        [super rightMouseDown:event];
}

/* Highlighted (pressed, or its menu open): the selection behind it. */
- (void)drawRect:(NSRect)rect {
    if([[self cell] isHighlighted]) {
        [[NSColor selectedMenuItemColor] setFill];
        [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect([self bounds], 0, 1) xRadius:4 yRadius:4] fill];
    }
    [super drawRect:rect];
}

/* A plain title in the menu bar's text color (the bar is dark). */
- (void)setTitle:(NSString *)title {
    NSMutableParagraphStyle *style = [[[NSParagraphStyle defaultParagraphStyle] mutableCopy] autorelease];
    NSMutableDictionary *attributes = [NSMutableDictionary dictionary];

    [style setAlignment:NSCenterTextAlignment];
    if([self font] != nil)
        [attributes setObject:[self font] forKey:NSFontAttributeName];
    [attributes setObject:style forKey:NSParagraphStyleAttributeName];
    [attributes setObject:_NSMenuBarTextColor() forKey:NSForegroundColorAttributeName];
    [super setAttributedTitle:[[[NSAttributedString alloc] initWithString:title ? title : @""
        attributes:attributes] autorelease]];
    [_statusItem _updateLength];
}

- (void)setAttributedTitle:(NSAttributedString *)title {
    [super setAttributedTitle:title];
    [_statusItem _updateLength];
}

- (void)setImage:(NSImage *)image {
    [super setImage:image];
    [_statusItem _updateLength];
}

@end

@implementation NSStatusItem {
    NSStatusBar *_statusBar;
    CGFloat _length;
    NSMenu *_menu;
    NSStatusBarButton *_button;
    NSStatusBarWindow *_window;
    NSView *_view;
    NSString *_autosaveName;
    NSStatusItemBehavior _behavior;
    BOOL _visible;
    BOOL _highlightMode;
    SEL _doubleAction;
    NSInteger _actionMask;
}

@synthesize statusBar = _statusBar, behavior = _behavior, autosaveName = _autosaveName;
@synthesize highlightMode = _highlightMode, doubleAction = _doubleAction;

- (instancetype)_initWithStatusBar:(NSStatusBar *)bar length:(CGFloat)length {
    if((self = [super init]) == nil)
        return nil;
    _statusBar = bar;
    _length = length;
    _visible = YES;
    _actionMask = NSLeftMouseUpMask;

    CGFloat thickness = [bar thickness];
    _window = [[NSStatusBarWindow alloc] initWithContentRect:NSMakeRect(0, 0, thickness, thickness)
        styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    [_window setLevel:NSStatusWindowLevel];
    [_window setOpaque:NO];
    [_window setBackgroundColor:[NSColor clearColor]];
    [_window setHasShadow:NO];
    [_window setHidesOnDeactivate:NO];
    [_window setExcludedFromWindowsMenu:YES];
    [_window setReleasedWhenClosed:NO];

    _button = [[NSStatusBarButton alloc] initWithFrame:NSMakeRect(0, 0, thickness, thickness)];
    [_button _setStatusItem:self];
    [_button setBordered:NO];
    [_button setButtonType:NSMomentaryChangeButton];
    [_button setFont:[NSFont menuBarFontOfSize:0]];
    [_button setImagePosition:NSImageLeft];
    [_button setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [_window setContentView:_button];
    [self _updateLength];
    [_window orderFront:nil];
    /* The window server lays it out in the menu bar from now on. */
    if(_CGUsesSkyLight())
        SLSSetWindowStatusItem([_window _skyLightWindowID], 1);
    return self;
}

- (void)_remove {
    if(_CGUsesSkyLight())
        SLSSetWindowStatusItem([_window _skyLightWindowID], 0);
    [_window orderOut:nil];
}

- (void)dealloc {
    [_window close];
    [_window release];
    [_button _setStatusItem:nil];
    [_button release];
    [_view release];
    [_menu release];
    [_autosaveName release];
    [super dealloc];
}

/* The window's width: the length, the bar's thickness for a square item,
   or what the button needs for a variable one. */
- (void)_updateLength {
    CGFloat thickness = [_statusBar thickness], width = _length;

    if(_length == NSSquareStatusItemLength)
        width = thickness;
    else if(_length == NSVariableStatusItemLength) {
        if(_view != nil)
            width = [_view frame].size.width;
        else
            width = ceil([[_button cell] cellSize].width) + kItemPadding;
        width = MAX(width, thickness);
    }
    NSRect frame = [_window frame];
    if(frame.size.width != width || frame.size.height != thickness) {
        frame.size = NSMakeSize(width, thickness);
        [_window setFrame:frame display:YES];
    }
}

- (CGFloat)length {
    return _length;
}

- (void)setLength:(CGFloat)length {
    _length = length;
    [self _updateLength];
}

- (NSStatusBarButton *)button {
    return _view != nil ? nil : _button;
}

- (NSMenu *)menu {
    return _menu;
}

- (void)setMenu:(NSMenu *)menu {
    [menu retain];
    [_menu release];
    _menu = menu;
}

- (BOOL)isVisible {
    return _visible;
}

- (void)setVisible:(BOOL)visible {
    _visible = visible;
    if(visible)
        [_window orderFront:nil];
    else
        [_window orderOut:nil];
}

/* The menu below the item, the button highlighted while it is open. */
- (void)_showMenu:(NSMenu *)menu {
    NSRect frame = [_window frame];

    [[_button cell] setHighlighted:YES];
    [_button display];
    [menu _popUpAtScreenPoint:NSMakePoint(NSMinX(frame), NSMinY(frame)) positioning:nil
        event:[NSApp currentEvent] startedOpen:NO minimumWidth:0 flips:NO perform:YES];
    [[_button cell] setHighlighted:NO];
    [_button display];
}

- (void)popUpStatusItemMenu:(NSMenu *)menu {
    [self _showMenu:menu];
}

/* Only a press or a release (the default) as the button's moment. */
- (NSInteger)sendActionOn:(NSInteger)mask {
    NSInteger previous = _actionMask;

    _actionMask = mask;
    return previous;
}

- (NSInteger)_actionMask {
    return _actionMask;
}

- (void)drawStatusBarBackgroundInRect:(NSRect)rect withHighlight:(BOOL)highlight {
    if(highlight) {
        [[NSColor selectedMenuItemColor] setFill];
        NSRectFillUsingOperation(rect, NSCompositeSourceOver);
    }
}

/* The earlier API: the button's properties. */
- (SEL)action { return [_button action]; }
- (void)setAction:(SEL)action { [_button setAction:action]; }
- (id)target { return [_button target]; }
- (void)setTarget:(id)target { [_button setTarget:target]; }
- (NSString *)title { return [_button title]; }
- (void)setTitle:(NSString *)title { [_button setTitle:title]; }
- (NSAttributedString *)attributedTitle { return [_button attributedTitle]; }
- (void)setAttributedTitle:(NSAttributedString *)title { [_button setAttributedTitle:title]; }
- (NSImage *)image { return [_button image]; }
- (void)setImage:(NSImage *)image { [_button setImage:image]; }
- (NSImage *)alternateImage { return [_button alternateImage]; }
- (void)setAlternateImage:(NSImage *)image { [_button setAlternateImage:image]; }
- (BOOL)isEnabled { return [_button isEnabled]; }
- (void)setEnabled:(BOOL)enabled { [_button setEnabled:enabled]; }
- (NSString *)toolTip { return [_button toolTip]; }
- (void)setToolTip:(NSString *)toolTip { [_button setToolTip:toolTip]; }

- (NSView *)view {
    return _view;
}

/* A view of its own instead of the button. */
- (void)setView:(NSView *)view {
    [view retain];
    [_view release];
    _view = view;
    [_window setContentView:view != nil ? view : _button];
    [self _updateLength];
}

@end
