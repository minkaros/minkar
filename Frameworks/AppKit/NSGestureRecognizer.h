/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Gesture recognizers: attached to a view, they see the mouse, key and
 * trackpad events of a gesture that starts in it (or in a view inside it)
 * before the view does, and send their action when they recognize it.
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSGeometry.h>
#import <AppKit/AppKitExport.h>

@class NSEvent, NSView, NSPressureConfiguration;
@protocol NSGestureRecognizerDelegate;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, NSGestureRecognizerState) {
    NSGestureRecognizerStatePossible,
    NSGestureRecognizerStateBegan,
    NSGestureRecognizerStateChanged,
    NSGestureRecognizerStateEnded,
    NSGestureRecognizerStateCancelled,
    NSGestureRecognizerStateFailed,
    NSGestureRecognizerStateRecognized = NSGestureRecognizerStateEnded
};

@interface NSGestureRecognizer : NSObject <NSCoding>

- (instancetype)initWithTarget:(nullable id)target action:(nullable SEL)action NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;

@property (nullable, weak) id target;
@property (nullable) SEL action;
@property (readonly) NSGestureRecognizerState state;
@property (nullable, weak) id<NSGestureRecognizerDelegate> delegate;
@property (getter=isEnabled) BOOL enabled;
@property (nullable, readonly, weak) NSView *view;
@property (strong) NSPressureConfiguration *pressureConfiguration;

@property BOOL delaysPrimaryMouseButtonEvents;
@property BOOL delaysSecondaryMouseButtonEvents;
@property BOOL delaysOtherMouseButtonEvents;
@property BOOL delaysKeyEvents;
@property BOOL delaysMagnificationEvents;
@property BOOL delaysRotationEvents;

- (NSPoint)locationInView:(nullable NSView *)view;

@end

@protocol NSGestureRecognizerDelegate <NSObject>
@optional
- (BOOL)gestureRecognizer:(NSGestureRecognizer *)gestureRecognizer shouldAttemptToRecognizeWithEvent:(NSEvent *)event;
- (BOOL)gestureRecognizerShouldBegin:(NSGestureRecognizer *)gestureRecognizer;
- (BOOL)gestureRecognizer:(NSGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(NSGestureRecognizer *)otherGestureRecognizer;
- (BOOL)gestureRecognizer:(NSGestureRecognizer *)gestureRecognizer shouldRequireFailureOfGestureRecognizer:(NSGestureRecognizer *)otherGestureRecognizer;
- (BOOL)gestureRecognizer:(NSGestureRecognizer *)gestureRecognizer shouldBeRequiredToFailByGestureRecognizer:(NSGestureRecognizer *)otherGestureRecognizer;
@end

/* For subclasses (NSGestureRecognizerSubclass): the state they set, and the
   events they are given. */
@interface NSGestureRecognizer (NSSubclassUse)
@property (readwrite) NSGestureRecognizerState state;
- (void)reset;
- (BOOL)canPreventGestureRecognizer:(NSGestureRecognizer *)preventedGestureRecognizer;
- (BOOL)canBePreventedByGestureRecognizer:(NSGestureRecognizer *)preventingGestureRecognizer;
- (BOOL)shouldRequireFailureOfGestureRecognizer:(NSGestureRecognizer *)otherGestureRecognizer;
- (BOOL)shouldBeRequiredToFailByGestureRecognizer:(NSGestureRecognizer *)otherGestureRecognizer;
- (void)mouseDown:(NSEvent *)event;
- (void)rightMouseDown:(NSEvent *)event;
- (void)otherMouseDown:(NSEvent *)event;
- (void)mouseUp:(NSEvent *)event;
- (void)rightMouseUp:(NSEvent *)event;
- (void)otherMouseUp:(NSEvent *)event;
- (void)mouseDragged:(NSEvent *)event;
- (void)rightMouseDragged:(NSEvent *)event;
- (void)otherMouseDragged:(NSEvent *)event;
- (void)keyDown:(NSEvent *)event;
- (void)keyUp:(NSEvent *)event;
- (void)flagsChanged:(NSEvent *)event;
- (void)tabletPoint:(NSEvent *)event;
- (void)magnifyWithEvent:(NSEvent *)event;
- (void)rotateWithEvent:(NSEvent *)event;
- (void)pressureChangeWithEvent:(NSEvent *)event;
@end

NS_ASSUME_NONNULL_END

#import <AppKit/NSClickGestureRecognizer.h>
#import <AppKit/NSPressGestureRecognizer.h>
#import <AppKit/NSPanGestureRecognizer.h>
#import <AppKit/NSMagnificationGestureRecognizer.h>
#import <AppKit/NSRotationGestureRecognizer.h>
