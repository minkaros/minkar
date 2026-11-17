/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Gesture recognizers and their window's part. A gesture starts with a
 * button going down (or a magnify or rotate event) over a view: the
 * recognizers of that view and of the views around it, enabled and let by
 * their delegates, are given each event of the gesture before the views
 * are. An event of a kind some of them delay (the primary button's, for a
 * click, press or pan recognizer) waits until those recognizers fail, and
 * then reaches the views; when one recognizes the gesture, what waited is
 * dropped and the rest of the gesture's events of those kinds go to no
 * view. The gesture ends when no button is down and no recognizer is
 * waiting; the recognizers are then reset. Defaults are the reference's
 * (checked on 16 Nov 2026).
 */

#import <AppKit/NSGestureRecognizer.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>
#import <Foundation/Foundation.h>
#include <math.h>
#include <objc/runtime.h>

@class _NSGestureSession;

/* ---- Event kinds a recognizer may delay ---- */

enum {
	kPrimary = 1 << 0,
	kSecondary = 1 << 1,
	kOther = 1 << 2,
	kKey = 1 << 3,
	kMagnify = 1 << 4,
	kRotate = 1 << 5,
};

static unsigned
kindOf(NSEvent *event)
{
	switch ([event type]) {
	case NSEventTypeLeftMouseDown: case NSEventTypeLeftMouseUp: case NSEventTypeLeftMouseDragged:
		return kPrimary;
	case NSEventTypeRightMouseDown: case NSEventTypeRightMouseUp: case NSEventTypeRightMouseDragged:
		return kSecondary;
	case NSEventTypeOtherMouseDown: case NSEventTypeOtherMouseUp: case NSEventTypeOtherMouseDragged:
		return kOther;
	case NSEventTypeKeyDown: case NSEventTypeKeyUp: case NSEventTypeFlagsChanged:
		return kKey;
	case NSEventTypeMagnify:
		return kMagnify;
	case NSEventTypeRotate:
		return kRotate;
	default:
		return 0;
	}
}

/* The buttonMask bit of a mouse event's button. */
static NSUInteger
buttonBit(NSEvent *event)
{
	switch (kindOf(event)) {
	case kPrimary: return 1;
	case kSecondary: return 2;
	case kOther: return (NSUInteger)1 << MIN([event buttonNumber], 31);
	default: return 0;
	}
}

static BOOL
isDown(NSEvent *event)
{
	NSEventType t = [event type];
	return t == NSEventTypeLeftMouseDown || t == NSEventTypeRightMouseDown || t == NSEventTypeOtherMouseDown;
}

static BOOL
isUp(NSEvent *event)
{
	NSEventType t = [event type];
	return t == NSEventTypeLeftMouseUp || t == NSEventTypeRightMouseUp || t == NSEventTypeOtherMouseUp;
}

/* ---- NSGestureRecognizer ---- */

@interface NSGestureRecognizer () {
@public
	NSGestureRecognizerState _state;
	__weak NSView *_view;
	__weak _NSGestureSession *_session;
	NSPoint _location;		/* window coordinates, of the last event */
	BOOL _enabled;
	unsigned _delays;
	id _pressureConfiguration;
}
- (void)_setView:(NSView *)view;
- (unsigned)_delayedKinds;
@end

@interface _NSGestureSession : NSObject {
@public
	__weak NSWindow *_window;
	NSMutableArray<NSGestureRecognizer *> *_recognizers;
	NSMutableArray<NSEvent *> *_held;
	unsigned _swallowed;		/* kinds no view gets until the gesture ends */
	NSUInteger _buttonsDown;
}
- (void)recognizerChanged:(NSGestureRecognizer *)recognizer from:(NSGestureRecognizerState)old;
@end

@implementation NSGestureRecognizer

@synthesize target = _target, action = _action, delegate = _delegate;

- (instancetype)init
{
	return [self initWithTarget:nil action:NULL];
}

- (instancetype)initWithTarget:(id)target action:(SEL)action
{
	if ((self = [super init]) != nil) {
		_target = target;
		_action = action;
		_enabled = YES;
		_state = NSGestureRecognizerStatePossible;
	}
	return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
	if ((self = [super init]) == nil)
		return nil;
	_enabled = YES;
	_state = NSGestureRecognizerStatePossible;
	if ([coder allowsKeyedCoding]) {
		_target = [coder decodeObjectForKey:@"NSGestureRecognizer.target"];
		NSString *action = [coder decodeObjectForKey:@"NSGestureRecognizer.action"];
		if (action != nil)
			_action = NSSelectorFromString(action);
		_enabled = ![coder decodeBoolForKey:@"NSGestureRecognizer.disabled"];
		_delegate = [coder decodeObjectForKey:@"NSGestureRecognizer.delegate"];
		struct { NSString *key; unsigned kind; } flags[] = {
			{ @"NSGestureRecognizer.delaysPrimaryMouseButtonEvents", kPrimary },
			{ @"NSGestureRecognizer.delaysSecondaryMouseButtonEvents", kSecondary },
			{ @"NSGestureRecognizer.delaysOtherMouseButtonEvents", kOther },
			{ @"NSGestureRecognizer.delaysKeyEvents", kKey },
			{ @"NSGestureRecognizer.delaysMagnificationEvents", kMagnify },
			{ @"NSGestureRecognizer.delaysRotationEvents", kRotate },
		};
		for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
			if ([coder containsValueForKey:flags[i].key]) {
				if ([coder decodeBoolForKey:flags[i].key])
					_delays |= flags[i].kind;
				else
					_delays &= ~flags[i].kind;
			}
	}
	return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
	if (![coder allowsKeyedCoding])
		return;
	if (_target != nil)
		[coder encodeConditionalObject:_target forKey:@"NSGestureRecognizer.target"];
	if (_action != NULL)
		[coder encodeObject:NSStringFromSelector(_action) forKey:@"NSGestureRecognizer.action"];
	if (!_enabled)
		[coder encodeBool:YES forKey:@"NSGestureRecognizer.disabled"];
	if (_delegate != nil)
		[coder encodeConditionalObject:_delegate forKey:@"NSGestureRecognizer.delegate"];
	[coder encodeBool:(_delays & kPrimary) != 0 forKey:@"NSGestureRecognizer.delaysPrimaryMouseButtonEvents"];
	[coder encodeBool:(_delays & kSecondary) != 0 forKey:@"NSGestureRecognizer.delaysSecondaryMouseButtonEvents"];
	[coder encodeBool:(_delays & kOther) != 0 forKey:@"NSGestureRecognizer.delaysOtherMouseButtonEvents"];
	[coder encodeBool:(_delays & kKey) != 0 forKey:@"NSGestureRecognizer.delaysKeyEvents"];
	[coder encodeBool:(_delays & kMagnify) != 0 forKey:@"NSGestureRecognizer.delaysMagnificationEvents"];
	[coder encodeBool:(_delays & kRotate) != 0 forKey:@"NSGestureRecognizer.delaysRotationEvents"];
}

- (NSGestureRecognizerState)state
{
	return _state;
}

/* Subclasses set it; the session sends the action and lets the views have
   or not have the gesture's events. */
- (void)setState:(NSGestureRecognizerState)state
{
	NSGestureRecognizerState old = _state;

	if (state == old && state != NSGestureRecognizerStateChanged)
		return;
	/* The delegate may keep a gesture from beginning. */
	if (old == NSGestureRecognizerStatePossible &&
	    (state == NSGestureRecognizerStateBegan || state == NSGestureRecognizerStateEnded)) {
		id<NSGestureRecognizerDelegate> d = _delegate;
		if ([d respondsToSelector:@selector(gestureRecognizerShouldBegin:)] && ![d gestureRecognizerShouldBegin:self])
			state = NSGestureRecognizerStateFailed;
	}
	_state = state;
	if (state == NSGestureRecognizerStateBegan || state == NSGestureRecognizerStateChanged ||
	    state == NSGestureRecognizerStateEnded ||
	    (state == NSGestureRecognizerStateCancelled && old != NSGestureRecognizerStatePossible)) {
		if (_action != NULL)
			[NSApp sendAction:_action to:_target from:self];
	}
	[_session recognizerChanged:self from:old];
}

- (BOOL)isEnabled { return _enabled; }

- (void)setEnabled:(BOOL)enabled
{
	_enabled = enabled;
	if (!enabled && (_state == NSGestureRecognizerStateBegan || _state == NSGestureRecognizerStateChanged))
		self.state = NSGestureRecognizerStateCancelled;
}

- (NSView *)view { return _view; }
- (void)_setView:(NSView *)view { _view = view; }

- (id)pressureConfiguration { return _pressureConfiguration; }
- (void)setPressureConfiguration:(id)configuration { _pressureConfiguration = configuration; }

- (unsigned)_delayedKinds { return _delays; }

#define DELAY_PROPERTY(getter, setter, kind) \
- (BOOL)getter { return (_delays & (kind)) != 0; } \
- (void)setter:(BOOL)flag { if (flag) _delays |= (kind); else _delays &= ~(unsigned)(kind); }
DELAY_PROPERTY(delaysPrimaryMouseButtonEvents, setDelaysPrimaryMouseButtonEvents, kPrimary)
DELAY_PROPERTY(delaysSecondaryMouseButtonEvents, setDelaysSecondaryMouseButtonEvents, kSecondary)
DELAY_PROPERTY(delaysOtherMouseButtonEvents, setDelaysOtherMouseButtonEvents, kOther)
DELAY_PROPERTY(delaysKeyEvents, setDelaysKeyEvents, kKey)
DELAY_PROPERTY(delaysMagnificationEvents, setDelaysMagnificationEvents, kMagnify)
DELAY_PROPERTY(delaysRotationEvents, setDelaysRotationEvents, kRotate)

- (NSPoint)locationInView:(NSView *)view
{
	return view != nil ? [view convertPoint:_location fromView:nil] : _location;
}

- (void)reset
{
}

- (BOOL)canPreventGestureRecognizer:(NSGestureRecognizer *)other { return YES; }
- (BOOL)canBePreventedByGestureRecognizer:(NSGestureRecognizer *)other { return YES; }
- (BOOL)shouldRequireFailureOfGestureRecognizer:(NSGestureRecognizer *)other { return NO; }
- (BOOL)shouldBeRequiredToFailByGestureRecognizer:(NSGestureRecognizer *)other { return NO; }

- (void)mouseDown:(NSEvent *)event {}
- (void)rightMouseDown:(NSEvent *)event {}
- (void)otherMouseDown:(NSEvent *)event {}
- (void)mouseUp:(NSEvent *)event {}
- (void)rightMouseUp:(NSEvent *)event {}
- (void)otherMouseUp:(NSEvent *)event {}
- (void)mouseDragged:(NSEvent *)event {}
- (void)rightMouseDragged:(NSEvent *)event {}
- (void)otherMouseDragged:(NSEvent *)event {}
- (void)keyDown:(NSEvent *)event {}
- (void)keyUp:(NSEvent *)event {}
- (void)flagsChanged:(NSEvent *)event {}
- (void)tabletPoint:(NSEvent *)event {}
- (void)magnifyWithEvent:(NSEvent *)event {}
- (void)rotateWithEvent:(NSEvent *)event {}
- (void)pressureChangeWithEvent:(NSEvent *)event {}

/* The event, to the method for it. */
- (void)_handleEvent:(NSEvent *)event
{
	if (kindOf(event) != kKey)
		_location = [event locationInWindow];
	switch ([event type]) {
	case NSEventTypeLeftMouseDown: [self mouseDown:event]; break;
	case NSEventTypeRightMouseDown: [self rightMouseDown:event]; break;
	case NSEventTypeOtherMouseDown: [self otherMouseDown:event]; break;
	case NSEventTypeLeftMouseUp: [self mouseUp:event]; break;
	case NSEventTypeRightMouseUp: [self rightMouseUp:event]; break;
	case NSEventTypeOtherMouseUp: [self otherMouseUp:event]; break;
	case NSEventTypeLeftMouseDragged: [self mouseDragged:event]; break;
	case NSEventTypeRightMouseDragged: [self rightMouseDragged:event]; break;
	case NSEventTypeOtherMouseDragged: [self otherMouseDragged:event]; break;
	case NSEventTypeKeyDown: [self keyDown:event]; break;
	case NSEventTypeKeyUp: [self keyUp:event]; break;
	case NSEventTypeFlagsChanged: [self flagsChanged:event]; break;
	case NSEventTypeMagnify: [self magnifyWithEvent:event]; break;
	case NSEventTypeRotate: [self rotateWithEvent:event]; break;
	case NSEventTypePressure: [self pressureChangeWithEvent:event]; break;
	case NSEventTypeTabletPoint: [self tabletPoint:event]; break;
	default: break;
	}
}

@end

/* ---- The window's part: one gesture at a time ---- */

static const void *kSessionKey = &kSessionKey;
static BOOL gReplaying;

static BOOL
isFinal(NSGestureRecognizerState s)
{
	return s == NSGestureRecognizerStateEnded || s == NSGestureRecognizerStateCancelled ||
	    s == NSGestureRecognizerStateFailed;
}

static BOOL
isActive(NSGestureRecognizerState s)
{
	return s == NSGestureRecognizerStateBegan || s == NSGestureRecognizerStateChanged ||
	    s == NSGestureRecognizerStateEnded;
}

@implementation _NSGestureSession

/* Kinds still waited for: those delayed by a recognizer that has not
   failed. */
- (unsigned)_pendingDelays
{
	unsigned kinds = 0;

	for (NSGestureRecognizer *g in _recognizers)
		if (g->_state == NSGestureRecognizerStatePossible)
			kinds |= [g _delayedKinds];
	return kinds;
}

- (void)_replay:(NSArray<NSEvent *> *)events
{
	NSWindow *window = _window;

	gReplaying = YES;
	@try {
		for (NSEvent *e in events)
			[window sendEvent:e];
	} @finally {
		gReplaying = NO;
	}
}

/* Held events no recognizer waits for any more go to the views, in
   order, until one that is still waited for. */
- (void)_flush
{
	unsigned pending = [self _pendingDelays];
	NSMutableArray *ready = [NSMutableArray array];

	while ([_held count] > 0 && (kindOf(_held[0]) & pending) == 0) {
		NSEvent *e = _held[0];
		[_held removeObjectAtIndex:0];
		if ((kindOf(e) & _swallowed) == 0)
			[ready addObject:e];
	}
	[self _replay:ready];
}

/* The gesture is over: reset, and the window forgets it. */
- (void)_endIfDone
{
	if (_buttonsDown != 0)
		return;
	for (NSGestureRecognizer *g in _recognizers)
		if (g->_state == NSGestureRecognizerStatePossible || g->_state == NSGestureRecognizerStateBegan ||
		    g->_state == NSGestureRecognizerStateChanged)
			return;
	[self _flush];
	for (NSGestureRecognizer *g in _recognizers) {
		g->_session = nil;
		[g reset];
		g->_state = NSGestureRecognizerStatePossible;
	}
	NSWindow *window = _window;
	if (window != nil && objc_getAssociatedObject(window, kSessionKey) == self)
		objc_setAssociatedObject(window, kSessionKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (void)recognizerChanged:(NSGestureRecognizer *)recognizer from:(NSGestureRecognizerState)old
{
	NSGestureRecognizerState now = recognizer->_state;

	if (old == NSGestureRecognizerStatePossible && isActive(now)) {
		/* It has the gesture: what it delays goes to no view, and the
		   others it can prevent fail. */
		_swallowed |= [recognizer _delayedKinds];
		[_held filterUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(NSEvent *e, NSDictionary *b) {
			return (kindOf(e) & self->_swallowed) == 0;
		}]];
		for (NSGestureRecognizer *other in [_recognizers copy]) {
			if (other == recognizer || other->_state != NSGestureRecognizerStatePossible)
				continue;
			id<NSGestureRecognizerDelegate> d1 = recognizer.delegate, d2 = other.delegate;
			BOOL together =
			    ([d1 respondsToSelector:@selector(gestureRecognizer:shouldRecognizeSimultaneouslyWithGestureRecognizer:)] &&
			     [d1 gestureRecognizer:recognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:other]) ||
			    ([d2 respondsToSelector:@selector(gestureRecognizer:shouldRecognizeSimultaneouslyWithGestureRecognizer:)] &&
			     [d2 gestureRecognizer:other shouldRecognizeSimultaneouslyWithGestureRecognizer:recognizer]);
			if (!together && [recognizer canPreventGestureRecognizer:other] &&
			    [other canBePreventedByGestureRecognizer:recognizer])
				other->_state = NSGestureRecognizerStateFailed;
		}
	}
	[self _flush];
	[self _endIfDone];
}

/* The event, to the recognizers first: YES when the views are not to
   have it now (it is held, or swallowed). */
- (BOOL)handleEvent:(NSEvent *)event
{
	unsigned kind = kindOf(event);

	if (isDown(event))
		_buttonsDown |= buttonBit(event);
	else if (isUp(event))
		_buttonsDown &= ~buttonBit(event);

	for (NSGestureRecognizer *g in [_recognizers copy])
		if (g->_enabled && !isFinal(g->_state))
			[g _handleEvent:event];

	BOOL keep = NO;
	if (kind & _swallowed)
		keep = YES;
	else if ((kind & [self _pendingDelays]) != 0 || [_held count] > 0) {
		[_held addObject:event];
		keep = YES;
	}
	[self _flush];
	[self _endIfDone];
	return keep;
}

@end

/* Called by NSWindow's sendEvent: before it hands the event to a view: YES
   when the gesture's recognizers keep it from the views for now. */
BOOL
_NSGestureWindowHandleEvent(NSWindow *window, NSEvent *event, NSView *hitView)
{
	if (gReplaying)
		return NO;
	unsigned kind = kindOf(event);
	if (kind == 0)
		return NO;
	_NSGestureSession *session = objc_getAssociatedObject(window, kSessionKey);

	/* A gesture starts with a button going down, or a trackpad gesture. */
	if (session == nil) {
		if (!(isDown(event) || kind == kMagnify || kind == kRotate) || hitView == nil)
			return NO;
		NSMutableArray *found = [NSMutableArray array];
		for (NSView *v = hitView; v != nil; v = [v superview])
			for (NSGestureRecognizer *g in [v gestureRecognizers]) {
				id<NSGestureRecognizerDelegate> d = g.delegate;
				if (!g->_enabled)
					continue;
				if ([d respondsToSelector:@selector(gestureRecognizer:shouldAttemptToRecognizeWithEvent:)] &&
				    ![d gestureRecognizer:g shouldAttemptToRecognizeWithEvent:event])
					continue;
				[found addObject:g];
			}
		if ([found count] == 0)
			return NO;
		session = [[_NSGestureSession alloc] init];
		session->_window = window;
		session->_recognizers = found;
		session->_held = [NSMutableArray array];
		for (NSGestureRecognizer *g in found) {
			g->_session = session;
			g->_state = NSGestureRecognizerStatePossible;
		}
		objc_setAssociatedObject(window, kSessionKey, session, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
	}
	return [session handleEvent:event];
}

/* ---- NSClickGestureRecognizer ---- */

@implementation NSClickGestureRecognizer {
	NSInteger _clicks;
	NSPoint _start;
	NSTimer *_timer;
}

@synthesize buttonMask = _buttonMask, numberOfClicksRequired = _numberOfClicksRequired,
    numberOfTouchesRequired = _numberOfTouchesRequired;

- (instancetype)initWithTarget:(id)target action:(SEL)action
{
	if ((self = [super initWithTarget:target action:action]) != nil) {
		_buttonMask = 1;
		_numberOfClicksRequired = 1;
		_numberOfTouchesRequired = 1;
		_delays = kPrimary;
	}
	return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
	if ((self = [super initWithCoder:coder]) != nil && [coder allowsKeyedCoding]) {
		_buttonMask = 1;
		_numberOfClicksRequired = 1;
		_numberOfTouchesRequired = 1;
		if (![coder containsValueForKey:@"NSGestureRecognizer.delaysPrimaryMouseButtonEvents"])
			_delays |= kPrimary;
		if ([coder containsValueForKey:@"NSClickGestureRecognizer.buttonMask"])
			_buttonMask = (NSUInteger)[coder decodeIntegerForKey:@"NSClickGestureRecognizer.buttonMask"];
		if ([coder containsValueForKey:@"NSClickGestureRecognizer.numberOfClicksRequired"])
			_numberOfClicksRequired = [coder decodeIntegerForKey:@"NSClickGestureRecognizer.numberOfClicksRequired"];
		if ([coder containsValueForKey:@"NSClickGestureRecognizer.numberOfTouchesRequired"])
			_numberOfTouchesRequired = [coder decodeIntegerForKey:@"NSClickGestureRecognizer.numberOfTouchesRequired"];
	}
	return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
	[super encodeWithCoder:coder];
	if (![coder allowsKeyedCoding])
		return;
	[coder encodeInteger:(NSInteger)_buttonMask forKey:@"NSClickGestureRecognizer.buttonMask"];
	[coder encodeInteger:_numberOfClicksRequired forKey:@"NSClickGestureRecognizer.numberOfClicksRequired"];
	[coder encodeInteger:_numberOfTouchesRequired forKey:@"NSClickGestureRecognizer.numberOfTouchesRequired"];
}

- (void)_stopTimer
{
	[_timer invalidate];
	_timer = nil;
}

- (void)_tooLate:(NSTimer *)timer
{
	_timer = nil;
	if (self.state == NSGestureRecognizerStatePossible)
		self.state = NSGestureRecognizerStateFailed;
}

- (void)_down:(NSEvent *)event
{
	[self _stopTimer];
	if ((buttonBit(event) & _buttonMask) == 0) {
		self.state = NSGestureRecognizerStateFailed;
		return;
	}
	if (_clicks == 0)
		_start = [event locationInWindow];
}

- (void)_dragged:(NSEvent *)event
{
	NSPoint p = [event locationInWindow];
	/* Moving away is not a click (the reference allows a few points). */
	if (hypot(p.x - _start.x, p.y - _start.y) > 4.0)
		self.state = NSGestureRecognizerStateFailed;
}

- (void)_up:(NSEvent *)event
{
	if ((buttonBit(event) & _buttonMask) == 0)
		return;
	_clicks++;
	if (_clicks >= _numberOfClicksRequired) {
		self.state = NSGestureRecognizerStateEnded;
		return;
	}
	/* The next click must come soon. */
	_timer = [NSTimer scheduledTimerWithTimeInterval:[NSEvent doubleClickInterval] target:self
	    selector:@selector(_tooLate:) userInfo:nil repeats:NO];
	[[NSRunLoop currentRunLoop] addTimer:_timer forMode:NSEventTrackingRunLoopMode];
}

- (void)mouseDown:(NSEvent *)e { [self _down:e]; }
- (void)rightMouseDown:(NSEvent *)e { [self _down:e]; }
- (void)otherMouseDown:(NSEvent *)e { [self _down:e]; }
- (void)mouseDragged:(NSEvent *)e { [self _dragged:e]; }
- (void)rightMouseDragged:(NSEvent *)e { [self _dragged:e]; }
- (void)otherMouseDragged:(NSEvent *)e { [self _dragged:e]; }
- (void)mouseUp:(NSEvent *)e { [self _up:e]; }
- (void)rightMouseUp:(NSEvent *)e { [self _up:e]; }
- (void)otherMouseUp:(NSEvent *)e { [self _up:e]; }

- (void)reset
{
	[self _stopTimer];
	_clicks = 0;
}

@end

/* ---- NSPressGestureRecognizer ---- */

@implementation NSPressGestureRecognizer {
	NSPoint _start;
	NSTimer *_timer;
}

@synthesize buttonMask = _buttonMask, minimumPressDuration = _minimumPressDuration,
    allowableMovement = _allowableMovement, numberOfTouchesRequired = _numberOfTouchesRequired;

- (instancetype)initWithTarget:(id)target action:(SEL)action
{
	if ((self = [super initWithTarget:target action:action]) != nil) {
		_buttonMask = 1;
		_minimumPressDuration = 0.5;
		_allowableMovement = 5.0;
		_numberOfTouchesRequired = 1;
		_delays = kPrimary;
	}
	return self;
}

- (void)_held:(NSTimer *)timer
{
	_timer = nil;
	if (self.state == NSGestureRecognizerStatePossible)
		self.state = NSGestureRecognizerStateBegan;
}

- (void)_down:(NSEvent *)event
{
	if ((buttonBit(event) & _buttonMask) == 0) {
		self.state = NSGestureRecognizerStateFailed;
		return;
	}
	_start = [event locationInWindow];
	[_timer invalidate];
	_timer = [NSTimer scheduledTimerWithTimeInterval:_minimumPressDuration target:self
	    selector:@selector(_held:) userInfo:nil repeats:NO];
	[[NSRunLoop currentRunLoop] addTimer:_timer forMode:NSEventTrackingRunLoopMode];
}

- (void)_dragged:(NSEvent *)event
{
	NSPoint p = [event locationInWindow];

	if (self.state == NSGestureRecognizerStatePossible) {
		if (hypot(p.x - _start.x, p.y - _start.y) > _allowableMovement)
			self.state = NSGestureRecognizerStateFailed;
	} else if (self.state == NSGestureRecognizerStateBegan || self.state == NSGestureRecognizerStateChanged)
		self.state = NSGestureRecognizerStateChanged;
}

- (void)_up:(NSEvent *)event
{
	[_timer invalidate];
	_timer = nil;
	if (self.state == NSGestureRecognizerStatePossible)
		self.state = NSGestureRecognizerStateFailed;
	else if (self.state == NSGestureRecognizerStateBegan || self.state == NSGestureRecognizerStateChanged)
		self.state = NSGestureRecognizerStateEnded;
}

- (void)mouseDown:(NSEvent *)e { [self _down:e]; }
- (void)rightMouseDown:(NSEvent *)e { [self _down:e]; }
- (void)otherMouseDown:(NSEvent *)e { [self _down:e]; }
- (void)mouseDragged:(NSEvent *)e { [self _dragged:e]; }
- (void)rightMouseDragged:(NSEvent *)e { [self _dragged:e]; }
- (void)otherMouseDragged:(NSEvent *)e { [self _dragged:e]; }
- (void)mouseUp:(NSEvent *)e { [self _up:e]; }
- (void)rightMouseUp:(NSEvent *)e { [self _up:e]; }
- (void)otherMouseUp:(NSEvent *)e { [self _up:e]; }

- (void)reset
{
	[_timer invalidate];
	_timer = nil;
}

@end

/* ---- NSPanGestureRecognizer ---- */

@implementation NSPanGestureRecognizer {
	NSPoint _start, _last, _offset, _velocity;
	NSTimeInterval _lastTime;
}

@synthesize buttonMask = _buttonMask, numberOfTouchesRequired = _numberOfTouchesRequired;

- (instancetype)initWithTarget:(id)target action:(SEL)action
{
	if ((self = [super initWithTarget:target action:action]) != nil) {
		_buttonMask = 1;
		_numberOfTouchesRequired = 1;
		_delays = kPrimary;
	}
	return self;
}

- (void)_down:(NSEvent *)event
{
	if ((buttonBit(event) & _buttonMask) == 0) {
		self.state = NSGestureRecognizerStateFailed;
		return;
	}
	_start = _last = [event locationInWindow];
	_offset = _velocity = NSZeroPoint;
	_lastTime = [event timestamp];
}

- (void)_dragged:(NSEvent *)event
{
	NSPoint p = [event locationInWindow];
	NSTimeInterval dt = [event timestamp] - _lastTime;

	if (dt > 0)
		_velocity = NSMakePoint((p.x - _last.x) / dt, (p.y - _last.y) / dt);
	_last = p;
	_lastTime = [event timestamp];
	if (self.state == NSGestureRecognizerStatePossible) {
		if (hypot(p.x - _start.x, p.y - _start.y) > 3.0)
			self.state = NSGestureRecognizerStateBegan;
	} else if (self.state == NSGestureRecognizerStateBegan || self.state == NSGestureRecognizerStateChanged)
		self.state = NSGestureRecognizerStateChanged;
}

- (void)_up:(NSEvent *)event
{
	if (self.state == NSGestureRecognizerStatePossible)
		self.state = NSGestureRecognizerStateFailed;
	else if (self.state == NSGestureRecognizerStateBegan || self.state == NSGestureRecognizerStateChanged)
		self.state = NSGestureRecognizerStateEnded;
}

- (void)mouseDown:(NSEvent *)e { [self _down:e]; }
- (void)rightMouseDown:(NSEvent *)e { [self _down:e]; }
- (void)otherMouseDown:(NSEvent *)e { [self _down:e]; }
- (void)mouseDragged:(NSEvent *)e { [self _dragged:e]; }
- (void)rightMouseDragged:(NSEvent *)e { [self _dragged:e]; }
- (void)otherMouseDragged:(NSEvent *)e { [self _dragged:e]; }
- (void)mouseUp:(NSEvent *)e { [self _up:e]; }
- (void)rightMouseUp:(NSEvent *)e { [self _up:e]; }
- (void)otherMouseUp:(NSEvent *)e { [self _up:e]; }

/* In view's coordinates (window coordinates for nil). */
- (NSPoint)translationInView:(NSView *)view
{
	NSPoint a = _start, b = _last;
	if (view != nil) {
		a = [view convertPoint:a fromView:nil];
		b = [view convertPoint:b fromView:nil];
	}
	return NSMakePoint(b.x - a.x + _offset.x, b.y - a.y + _offset.y);
}

- (void)setTranslation:(NSPoint)translation inView:(NSView *)view
{
	NSPoint now = [self translationInView:view];
	_offset = NSMakePoint(_offset.x + translation.x - now.x, _offset.y + translation.y - now.y);
}

- (NSPoint)velocityInView:(NSView *)view
{
	if (view == nil)
		return _velocity;
	NSPoint o = [view convertPoint:NSZeroPoint fromView:nil];
	NSPoint v = [view convertPoint:_velocity fromView:nil];
	return NSMakePoint(v.x - o.x, v.y - o.y);
}

@end

/* ---- Trackpad gestures ---- */

@implementation NSMagnificationGestureRecognizer

@synthesize magnification = _magnification;

- (instancetype)initWithTarget:(id)target action:(SEL)action
{
	if ((self = [super initWithTarget:target action:action]) != nil)
		_delays = kMagnify;
	return self;
}

- (void)magnifyWithEvent:(NSEvent *)event
{
	NSEventPhase phase = [event phase];

	_magnification += [event magnification];
	if (phase & NSEventPhaseBegan)
		self.state = NSGestureRecognizerStateBegan;
	else if (phase & (NSEventPhaseEnded))
		self.state = NSGestureRecognizerStateEnded;
	else if (phase & NSEventPhaseCancelled)
		self.state = NSGestureRecognizerStateCancelled;
	else if (self.state == NSGestureRecognizerStatePossible)
		self.state = NSGestureRecognizerStateBegan;
	else
		self.state = NSGestureRecognizerStateChanged;
}

- (void)reset
{
	_magnification = 0;
}

@end

@implementation NSRotationGestureRecognizer

@synthesize rotation = _rotation;

- (instancetype)initWithTarget:(id)target action:(SEL)action
{
	if ((self = [super initWithTarget:target action:action]) != nil)
		_delays = kRotate;
	return self;
}

- (CGFloat)rotationInDegrees { return _rotation * 180.0 / M_PI; }
- (void)setRotationInDegrees:(CGFloat)degrees { _rotation = degrees * M_PI / 180.0; }

- (void)rotateWithEvent:(NSEvent *)event
{
	NSEventPhase phase = [event phase];

	_rotation += [event rotation] * M_PI / 180.0;
	if (phase & NSEventPhaseBegan)
		self.state = NSGestureRecognizerStateBegan;
	else if (phase & NSEventPhaseEnded)
		self.state = NSGestureRecognizerStateEnded;
	else if (phase & NSEventPhaseCancelled)
		self.state = NSGestureRecognizerStateCancelled;
	else if (self.state == NSGestureRecognizerStatePossible)
		self.state = NSGestureRecognizerStateBegan;
	else
		self.state = NSGestureRecognizerStateChanged;
}

- (void)reset
{
	_rotation = 0;
}

@end

/* ---- NSView's clipsToBounds ---- */

static const void *kClipsKey = &kClipsKey;

@implementation NSView (NSClipsToBounds)

- (BOOL)clipsToBounds
{
	NSNumber *value = objc_getAssociatedObject(self, kClipsKey);
	return value != nil ? [value boolValue] : YES;
}

- (void)setClipsToBounds:(BOOL)clips
{
	objc_setAssociatedObject(self, kClipsKey, clips ? nil : @NO, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

@end

/* ---- NSView's recognizers ---- */

static const void *kRecognizersKey = &kRecognizersKey;

@implementation NSView (NSGestureRecognizer)

- (NSArray<NSGestureRecognizer *> *)gestureRecognizers
{
	return [objc_getAssociatedObject(self, kRecognizersKey) copy] ?: @[];
}

- (void)setGestureRecognizers:(NSArray<NSGestureRecognizer *> *)recognizers
{
	for (NSGestureRecognizer *g in [self gestureRecognizers])
		[self removeGestureRecognizer:g];
	for (NSGestureRecognizer *g in recognizers)
		[self addGestureRecognizer:g];
}

- (void)addGestureRecognizer:(NSGestureRecognizer *)recognizer
{
	NSMutableArray *list = objc_getAssociatedObject(self, kRecognizersKey);

	if (recognizer == nil)
		return;
	NSView *old = recognizer.view;
	if (old == self)
		return;
	[old removeGestureRecognizer:recognizer];
	if (list == nil) {
		list = [NSMutableArray array];
		objc_setAssociatedObject(self, kRecognizersKey, list, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
	}
	[list addObject:recognizer];
	[recognizer _setView:self];
}

- (void)removeGestureRecognizer:(NSGestureRecognizer *)recognizer
{
	NSMutableArray *list = objc_getAssociatedObject(self, kRecognizersKey);

	if (recognizer == nil || ![list containsObject:recognizer])
		return;
	[recognizer _setView:nil];
	[list removeObjectIdenticalTo:recognizer];
}

@end
