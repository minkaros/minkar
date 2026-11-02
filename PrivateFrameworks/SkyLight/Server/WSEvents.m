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

#include <mach/mach.h>
#include <stdio.h>
#include <LaunchServices/LSSession.h>
#import "WSEvents.h"
#import "WSConnection.h"
#import "WSWindowList.h"
#include "WindowServerServer.h"	/* MIG server side */
#include "WindowServerTypes.h"

/* A connection's queue holds at most this many events; more are dropped. */
enum { kQueueLimit = 256 };
/* Key repeat: the first after 500 ms, then every 33 ms. */
static const uint64_t kRepeatDelay = 500000000, kRepeatInterval = 33000000;
/* A second click within 500 ms and 4 pixels of the first counts double. */
static const uint64_t kClickInterval = 500000000;
static const double kClickSlop = 4;

static __weak WSWindow *captureWindow;	/* got the mouse down */
static uint32_t buttonsDown;		/* bit per button number */
static __weak WSWindow *keyWindow;
static uint64_t lastClickTime;
static double lastClickX, lastClickY;
static uint32_t lastClickButton;
static int32_t clickCount;
static dispatch_source_t repeatTimer;
/* A server-side move: the window, and the pointer's offset in it. */
static __weak WSWindow *moveWindow;
/* Holds the pointer for an open menu (SLSCapturePointer). */
static __weak WSWindow *grabWindow;
/* The window the pointer was last over, without a button down. */
static __weak WSWindow *hoverWindow;
static double moveOffsetX, moveOffsetY;
static SLSEvent repeatEvent;

/* ---- The front app ---- */

/*
 * The front app (WS4a): made so by a window becoming key, or by asking
 * (SLSSetFrontProcess, an app with no window). It stays in front when its
 * windows close; when it goes away, the app in front before it takes its
 * place. The key window, when there is one, is always the front app's.
 * LaunchServices' session is told from a queue of its own, in order: the
 * IPC queue never waits for launchservicesd.
 */
static WSConnection *frontConnection;
static NSMutableArray<WSConnection *> *activationOrder;	/* front first */
static pid_t frontPID;

static void
tellFrontApp(pid_t pid)
{
	static dispatch_queue_t queue;

	if (pid == frontPID)
		return;
	frontPID = pid;
	if (queue == nil)
		queue = dispatch_queue_create("org.minkar.WindowServer.launchservices",
		    DISPATCH_QUEUE_SERIAL);
	dispatch_async(queue, ^{
		int result = LSSessionSetFront(pid);
		if (result != kLSSessionSuccess)
			fprintf(stderr, "WindowServer: cannot tell launchservicesd the front app "
			    "(pid %d): %d\n", pid, result);
	});
}

static void
makeFront(WSConnection *c)
{
	/* A capture belongs to the front app. */
	if (grabWindow != nil && grabWindow.owner != c)
		grabWindow = nil;
	if (activationOrder == nil)
		activationOrder = [NSMutableArray array];
	if (c != nil) {
		[activationOrder removeObject:c];
		[activationOrder insertObject:c atIndex:0];
	}
	frontConnection = c;
	if (keyWindow != nil && keyWindow.owner != c)
		keyWindow = nil;
	WSCursorShapeChanged();
	tellFrontApp(c != nil ? c.pid : 0);
}

static void
setKeyWindow(WSWindow *w)
{
	keyWindow = w;
	if (w != nil && w.owner != frontConnection)
		makeFront(w.owner);
}

void
WSEventsWindowGone(WSWindow *w)
{
	if (grabWindow == w)
		grabWindow = nil;
	if (keyWindow == w && w.owner != nil)
		keyWindow = WSWindowFrontmostOf(w.owner);
	if (moveWindow == w)
		moveWindow = nil;
}

WSConnection *
WSFrontConnection(void)
{
	return frontConnection;
}

void
WSEventsConnectionDied(WSConnection *c)
{
	[activationOrder removeObject:c];
	if (c != frontConnection)
		return;
	WSConnection *next = activationOrder.firstObject;
	makeFront(next);
	if (next != nil)
		keyWindow = WSWindowFrontmostOf(next);
}

/* ---- The menu bar ---- */

/* A window moved by the server keeps its top below the menu bar of the
 * display under the pointer. */
static void
keepBelowMenuBar(SLSRect *f, double px, double py)
{
	for (WSDisplay *d in WSDisplays) {
		if (px < d.x || py < d.y || px >= d.x + d.width || py >= d.y + d.height)
			continue;
		if (f->y < d.y + d.menuBarHeight)
			f->y = d.y + d.menuBarHeight;
		return;
	}
}

/* ---- Queues ---- */

/* Tells the client its queue has events: a header only, never waited for. */
static void
ping(WSConnection *c)
{
	mach_msg_header_t msg = {
		.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0),
		.msgh_size = sizeof(msg),
		.msgh_remote_port = c.port,
		.msgh_id = WS_EVENTS_PENDING_ID,
	};

	mach_msg(&msg, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(msg), 0, MACH_PORT_NULL, 0,
	    MACH_PORT_NULL);
}

static BOOL
isMove(uint32_t type)
{
	return type == kSLSEventMouseMoved || type == kSLSEventLeftMouseDragged ||
	    type == kSLSEventRightMouseDragged || type == kSLSEventOtherMouseDragged ||
	    type == kSLSEventWindowMoved;
}

static void enqueue(WSConnection *c, const SLSEvent *e);

void
WSEventsEnqueue(WSConnection *connection, const SLSEvent *event)
{
	enqueue(connection, event);
}

static void
enqueue(WSConnection *c, const SLSEvent *e)
{
	NSMutableData *queue = c.eventQueue;
	NSUInteger n = queue.length / sizeof(SLSEvent);

	if (n > 0 && isMove(e->type)) {
		SLSEvent *last = (SLSEvent *)queue.mutableBytes + n - 1;
		/* Coalesced: the last move to the same window takes this one in. */
		if (last->type == e->type && last->window == e->window) {
			last->x = e->x;
			last->y = e->y;
			last->dx += e->dx;
			last->dy += e->dy;
			last->flags = e->flags;
			last->timestamp = e->timestamp;
			return;
		}
	}
	if (n >= kQueueLimit) {
		c.droppedEvents++;
		return;
	}
	[queue appendBytes:e length:sizeof(*e)];
	if (n == 0)
		ping(c);
}

/* ---- Routing ---- */

static void
send(WSWindow *w, SLSEvent *e)
{
	WSConnection *owner = w.owner;

	if (owner == nil)
		return;
	e->window = w.windowID;
	enqueue(owner, e);
}

/* The window keys go to: a capture's shielding window, else the key one. */
static WSWindow *
keyTarget(void)
{
	for (WSDisplay *d in WSDisplays)
		if (d.shield != nil)
			return d.shield;
	return keyWindow;
}

static void
stopRepeat(void)
{
	if (repeatTimer != nil) {
		dispatch_source_cancel(repeatTimer);
		repeatTimer = nil;
	}
}

static void
startRepeat(const SLSEvent *e)
{
	stopRepeat();
	repeatEvent = *e;
	repeatEvent.isRepeat = 1;
	repeatTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, WSIPCQueue);
	dispatch_source_set_timer(repeatTimer, dispatch_time(DISPATCH_TIME_NOW, kRepeatDelay),
	    kRepeatInterval, kRepeatInterval / 10);
	dispatch_source_set_event_handler(repeatTimer, ^{
		WSWindow *target = keyTarget();
		if (target == nil) {
			stopRepeat();
			return;
		}
		repeatEvent.timestamp = WSNanoseconds();
		send(target, &repeatEvent);
	});
	dispatch_resume(repeatTimer);
}

static void
routeKey(SLSEvent *e)
{
	WSWindow *target = keyTarget();

	/* A system-wide shortcut goes to its owner, not to the key window. */
	if (WSHotKeysHandle(e, target))
		return;
	if (e->type == kSLSEventKeyDown && !e->isRepeat)
		startRepeat(e);
	else if (e->type == kSLSEventKeyUp && e->keyCode == repeatEvent.keyCode)
		stopRepeat();
	if (target != nil)
		send(target, e);
}

static uint32_t
dragType(void)
{
	if (buttonsDown & 1)
		return kSLSEventLeftMouseDragged;
	if (buttonsDown & 2)
		return kSLSEventRightMouseDragged;
	return kSLSEventOtherMouseDragged;
}

static void
routeMouse(SLSEvent *e)
{
	BOOL down = e->type == kSLSEventLeftMouseDown || e->type == kSLSEventRightMouseDown ||
	    e->type == kSLSEventOtherMouseDown;
	BOOL up = e->type == kSLSEventLeftMouseUp || e->type == kSLSEventRightMouseUp ||
	    e->type == kSLSEventOtherMouseUp;
	WSWindow *target;

	SLSPoint previous = WSCursorPosition();
	WSCursorMoveTo(e->x, e->y);
	SLSPoint now = WSCursorPosition();
	e->x = now.x;
	e->y = now.y;
	if (isMove(e->type)) {
		e->dx = now.x - previous.x;
		e->dy = now.y - previous.y;
	}

	/* A captured pointer: every mouse event to the capturing window, with
	 * drags while a button is down; no key window change, no move. */
	WSWindow *grab = grabWindow;
	if (grab != nil) {
		if (down) {
			buttonsDown |= 1u << e->buttonNumber;
			e->clickCount = 1;
		} else if (up) {
			buttonsDown &= ~(1u << e->buttonNumber);
			e->clickCount = 1;
		} else if (isMove(e->type) && buttonsDown != 0)
			e->type = dragType();
		send(grab, e);
		return;
	}

	/* A server-side move takes the left button's drags; its up ends it. */
	WSWindow *moving = moveWindow;
	if (moving != nil && (isMove(e->type) || e->type == kSLSEventLeftMouseUp)) {
		SLSRect f = moving.frame;
		BOOL ends = e->type == kSLSEventLeftMouseUp;
		f.x = e->x - moveOffsetX;
		f.y = e->y - moveOffsetY;
		keepBelowMenuBar(&f, e->x, e->y);
		WSWindowSetFrame(moving, f);
		SLSEvent moved = *e;
		moved.type = kSLSEventWindowMoved;
		moved.x = f.x;
		moved.y = f.y;
		moved.clickCount = ends;
		send(moving, &moved);
		if (ends) {
			/* The owner gets the mouse up that ends it, as for any click. */
			e->clickCount = clickCount;
			send(moving, e);
			moveWindow = nil;
			buttonsDown &= ~1u;
			if (buttonsDown == 0)
				captureWindow = nil;
		}
		return;
	}

	if (down) {
		if (buttonsDown == 0)
			captureWindow = WSWindowAtPoint(e->x, e->y);
		buttonsDown |= 1u << e->buttonNumber;
		if (e->buttonNumber == lastClickButton &&
		    e->timestamp - lastClickTime < kClickInterval &&
		    fabs(e->x - lastClickX) <= kClickSlop && fabs(e->y - lastClickY) <= kClickSlop)
			clickCount++;
		else
			clickCount = 1;
		lastClickTime = e->timestamp;
		lastClickX = e->x;
		lastClickY = e->y;
		lastClickButton = e->buttonNumber;
		e->clickCount = clickCount;
		target = captureWindow;
		/* A click makes its window key. */
		if (target != nil && e->buttonNumber == 0)
			setKeyWindow(target);
		/* A left click in a drag region moves the window from now on,
		 * whether or not its app answers (it still gets this click). */
		if (target != nil && e->type == kSLSEventLeftMouseDown && buttonsDown == 1u &&
		    [target dragRegionContainsX:e->x - target.frame.x y:e->y - target.frame.y]) {
			moveWindow = target;
			moveOffsetX = e->x - target.frame.x;
			moveOffsetY = e->y - target.frame.y;
		}
	} else if (up) {
		target = captureWindow ? captureWindow : WSWindowAtPoint(e->x, e->y);
		buttonsDown &= ~(1u << e->buttonNumber);
		e->clickCount = clickCount;
		if (buttonsDown == 0)
			captureWindow = nil;
	} else if (isMove(e->type) && buttonsDown != 0) {
		e->type = dragType();
		target = captureWindow;
	} else {
		target = WSWindowAtPoint(e->x, e->y);
		/* The window the pointer left gets this move too, outside it: its
		 * app ends its tracking areas there (cursor rects back to the
		 * arrow, mouse exited). */
		WSWindow *left = hoverWindow;
		if (left != nil && left != target && e->type == kSLSEventMouseMoved) {
			SLSEvent last = *e;
			send(left, &last);
		}
		hoverWindow = target;
	}
	if (target != nil)
		send(target, e);
}

/* ---- The input state (SLSGetHIDState) ---- */

static SLSHIDState hidStates[2];	/* hardware, combined */

static void
noteState(SLSHIDState *s, const SLSEvent *e)
{
	uint32_t type = e->type;

	if (type < kSLSHIDEventTypes) {
		s->counters[type]++;
		s->lastEvent[type] = e->timestamp ? e->timestamp : WSNanoseconds();
	}
	switch (type) {
	case kSLSEventLeftMouseDown: case kSLSEventRightMouseDown: case kSLSEventOtherMouseDown:
		if (e->buttonNumber < 32)
			s->buttons |= 1u << e->buttonNumber;
		break;
	case kSLSEventLeftMouseUp: case kSLSEventRightMouseUp: case kSLSEventOtherMouseUp:
		if (e->buttonNumber < 32)
			s->buttons &= ~(1u << e->buttonNumber);
		break;
	case kSLSEventKeyDown:
		if (e->keyCode < 128)
			s->keys[e->keyCode / 32] |= 1u << (e->keyCode % 32);
		break;
	case kSLSEventKeyUp:
		if (e->keyCode < 128)
			s->keys[e->keyCode / 32] &= ~(1u << (e->keyCode % 32));
		break;
	case kSLSEventFlagsChanged:
		/* A modifier key: one event when pressed, one when released. */
		if (e->keyCode < 128)
			s->keys[e->keyCode / 32] ^= 1u << (e->keyCode % 32);
		break;
	}
	if (type == kSLSEventKeyDown || type == kSLSEventKeyUp || type == kSLSEventFlagsChanged ||
	    (type >= kSLSEventLeftMouseDown && type <= 27))
		s->flags = e->flags;
}

kern_return_t
wss_hid_state(mach_port_t server, uint32_t which, int32_t *result, SLSHIDState *state)
{
	memset(state, 0, sizeof(*state));
	if (which > kSLSHIDStateCombined) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	*state = hidStates[which];
	state->now = WSNanoseconds();
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

/* From the input devices. */
void
WSRouteEvent(SLSEvent *e)
{
	WSStatsCountEvent();
	noteState(&hidStates[kSLSHIDStateHardware], e);
	WSRouteEventAt(e, 0);
}

/* From the devices, or posted at a tap location. */
void
WSRouteEventAt(SLSEvent *e, uint32_t location)
{
	noteState(&hidStates[kSLSHIDStateCombined], e);
	if (!WSTapsFilter(e, location))
		return;
	if (e->type == kSLSEventKeyDown || e->type == kSLSEventKeyUp ||
	    e->type == kSLSEventFlagsChanged)
		routeKey(e);
	else
		routeMouse(e);
}

/* ---- Routines ---- */

kern_return_t
wss_next_events(mach_port_t server, audit_token_t token, ws_event_list_t events,
    mach_msg_type_number_t *count)
{
	WSConnection *c = [WSConnection connectionForToken:token];
	NSMutableData *queue = c.eventQueue;
	NSUInteger n = queue.length / sizeof(SLSEvent);

	*count = 0;
	if (c == nil)
		return KERN_SUCCESS;
	if (n > kSLSMaxEvents)
		n = kSLSMaxEvents;
	memcpy(events, queue.bytes, n * sizeof(SLSEvent));
	[queue replaceBytesInRange:NSMakeRange(0, n * sizeof(SLSEvent)) withBytes:NULL length:0];
	*count = (mach_msg_type_number_t)n;
	/* More left: the client is told again. */
	if (queue.length > 0)
		ping(c);
	return KERN_SUCCESS;
}

kern_return_t
wss_set_key_window(mach_port_t server, audit_token_t token, uint32_t window, int32_t *result)
{
	WSConnection *c = [WSConnection connectionForToken:token];
	WSWindow *w = WSWindowWithID(window);

	if (c == nil)
		*result = kSLSErrorInvalidConnection;
	else if (w == nil)
		*result = kSLSErrorIllegalArgument;
	else if (w.owner != c)
		*result = kSLSErrorCannotComplete;
	else {
		setKeyWindow(w);
		*result = kSLSErrorSuccess;
	}
	return KERN_SUCCESS;
}

kern_return_t
wss_window_start_move(mach_port_t server, audit_token_t token, uint32_t window,
    int32_t *result)
{
	WSConnection *c = [WSConnection connectionForToken:token];
	WSWindow *w = WSWindowWithID(window);

	if (c == nil)
		*result = kSLSErrorInvalidConnection;
	else if (w == nil)
		*result = kSLSErrorIllegalArgument;
	else if (w.owner != c || !(buttonsDown & 1))
		*result = kSLSErrorCannotComplete;	/* not its window, or no button down */
	else {
		SLSPoint p = WSCursorPosition();
		moveWindow = w;
		moveOffsetX = p.x - w.frame.x;
		moveOffsetY = p.y - w.frame.y;
		*result = kSLSErrorSuccess;
	}
	return KERN_SUCCESS;
}

kern_return_t
wss_window_set_drag_region(mach_port_t server, audit_token_t token, uint32_t window,
    ws_region_t drag, mach_msg_type_number_t dragCount, ws_region_t exclude,
    mach_msg_type_number_t excludeCount, int32_t *result)
{
	WSConnection *c = [WSConnection connectionForToken:token];
	WSWindow *w = WSWindowWithID(window);

	if (c == nil)
		*result = kSLSErrorInvalidConnection;
	else if (w == nil || dragCount > kSLSMaxRegionRects || excludeCount > kSLSMaxRegionRects)
		*result = kSLSErrorIllegalArgument;
	else if (w.owner != c)
		*result = kSLSErrorCannotComplete;
	else {
		w.dragRects = [NSData dataWithBytes:drag length:dragCount * sizeof(SLSIntRect)];
		w.dragExcludes = [NSData dataWithBytes:exclude
		    length:excludeCount * sizeof(SLSIntRect)];
		fprintf(stderr, "WindowServer: window %u drag region: %u rect(s), %u excluded",
		    window, dragCount, excludeCount);
		if (dragCount > 0)
			fprintf(stderr, " (first %d,%d %dx%d)", drag[0].x, drag[0].y, drag[0].width,
			    drag[0].height);
		fprintf(stderr, "\n");
		*result = kSLSErrorSuccess;
	}
	return KERN_SUCCESS;
}

kern_return_t
wss_set_front_process(mach_port_t server, audit_token_t token, int32_t *result)
{
	WSConnection *c = [WSConnection connectionForToken:token];

	if (c == nil) {
		*result = kSLSErrorInvalidConnection;
		return KERN_SUCCESS;
	}
	if (c != frontConnection) {
		makeFront(c);
		keyWindow = WSWindowFrontmostOf(c);
	}
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

kern_return_t
wss_capture_pointer(mach_port_t server, audit_token_t token, uint32_t window,
    int32_t *result)
{
	WSConnection *c = [WSConnection connectionForToken:token];
	WSWindow *w = window ? WSWindowWithID(window) : nil;

	if (c == nil)
		*result = kSLSErrorInvalidConnection;
	else if (window == 0) {
		if (grabWindow != nil && grabWindow.owner == c)
			grabWindow = nil;
		*result = kSLSErrorSuccess;
	} else if (w == nil)
		*result = kSLSErrorIllegalArgument;
	else if (w.owner != c || c != frontConnection)
		*result = kSLSErrorCannotComplete;	/* not its window, or not in front */
	else {
		/* What the pointer was doing stops: a move, a click's window. */
		moveWindow = nil;
		captureWindow = nil;
		grabWindow = w;
		*result = kSLSErrorSuccess;
	}
	return KERN_SUCCESS;
}

kern_return_t
wss_pointer_capture(mach_port_t server, uint32_t *window)
{
	WSWindow *grab = grabWindow;

	*window = grab != nil ? grab.windowID : 0;
	return KERN_SUCCESS;
}

kern_return_t
wss_front_process(mach_port_t server, int32_t *pid)
{
	*pid = frontConnection != nil ? frontConnection.pid : 0;
	return KERN_SUCCESS;
}

kern_return_t
wss_key_window(mach_port_t server, uint32_t *window)
{
	*window = keyWindow ? keyWindow.windowID : 0;
	return KERN_SUCCESS;
}
