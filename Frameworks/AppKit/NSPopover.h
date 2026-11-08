/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSPopover (API 19): a view controller's view shown in a bubble that
 * points at a rect of a view, closed by a click elsewhere (transient) or
 * by its owner.
 */

#import <AppKit/NSResponder.h>
#import <Foundation/NSGeometry.h>

@class NSViewController, NSView, NSWindow, NSNotification, NSString;
@protocol NSPopoverDelegate;

typedef NS_ENUM(NSInteger, NSPopoverAppearance) {
    NSPopoverAppearanceMinimal = 0,
    NSPopoverAppearanceHUD = 1,
};

typedef NS_ENUM(NSInteger, NSPopoverBehavior) {
    NSPopoverBehaviorApplicationDefined = 0,
    NSPopoverBehaviorTransient = 1,
    NSPopoverBehaviorSemitransient = 2,
};

APPKIT_EXPORT NSString *const NSPopoverCloseReasonKey;
APPKIT_EXPORT NSString *const NSPopoverCloseReasonStandard;
APPKIT_EXPORT NSString *const NSPopoverCloseReasonDetachToWindow;
APPKIT_EXPORT NSString *const NSPopoverWillShowNotification;
APPKIT_EXPORT NSString *const NSPopoverDidShowNotification;
APPKIT_EXPORT NSString *const NSPopoverWillCloseNotification;
APPKIT_EXPORT NSString *const NSPopoverDidCloseNotification;

@interface NSPopover : NSResponder {
    NSViewController *_contentViewController;
    NSSize _contentSize;
    NSPopoverBehavior _behavior;
    BOOL _animates;
    BOOL _shown;
    NSRect _positioningRect;
    NSView *_positioningView;
    NSRectEdge _preferredEdge;
    id _delegate;
    NSWindow *_window;
    NSInteger _appearance;
    id _reserved;
}

@property(retain) NSViewController *contentViewController;
@property NSSize contentSize;
@property NSPopoverBehavior behavior;
@property BOOL animates;
@property NSPopoverAppearance appearance;
@property(assign) id <NSPopoverDelegate> delegate;
@property(readonly, getter=isShown) BOOL shown;
@property(readonly, getter=isDetached) BOOL detached;
@property NSRect positioningRect;

- (void)showRelativeToRect:(NSRect)positioningRect ofView:(NSView *)positioningView preferredEdge:(NSRectEdge)preferredEdge;
- (void)performClose:(id)sender;
- (void)close;

@end

@protocol NSPopoverDelegate <NSObject>
@optional
- (BOOL)popoverShouldClose:(NSPopover *)popover;
- (BOOL)popoverShouldDetach:(NSPopover *)popover;
- (void)popoverDidDetach:(NSPopover *)popover;
- (NSWindow *)detachableWindowForPopover:(NSPopover *)popover;
- (void)popoverWillShow:(NSNotification *)notification;
- (void)popoverDidShow:(NSNotification *)notification;
- (void)popoverWillClose:(NSNotification *)notification;
- (void)popoverDidClose:(NSNotification *)notification;
@end
