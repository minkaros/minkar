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
#include <pthread.h>
#include <stdio.h>
#include <LaunchServices/LSSession.h>
#import "WSEvents.h"
#import "WSConnection.h"
#import "WSWindowList.h"
#include "WindowServerServer.h"	/* MIG server side */
#include "WindowServerTypes.h"

/*
 * Two queues (WS4a). The IPC queue owns the model: the windows, the key
 * window, the front app, the pointer's capture. After a request that
 * changed any of it, it publishes a snapshot of what routing needs
 * (WSInputPublish). Input is read and routed on its own high-priority
 * queue, WSInputQueue, over the latest snapshot, and never waits for the
 * IPC queue: what it changes in the model (a click making a window key, a
 * server-side move) it hands to the IPC queue, in order. The connections'
 * event queues, the taps, the hotkey table and the input state are shared
 * under their own locks.
 */

dispatch_queue_t WSInputQueue;

/* A connection's queue holds at most this many events; more are dropped. */
enum { kQueueLimit = 256 };
/* Key repeat: by default the first after 500 ms, then every 33 ms. */
static uint64_t kRepeatDelay = 500000000, kRepeatInterval = 33000000;

/* The repeat's timing (SLSSetKeyboardSettings, through WSInput). */
void
WSEventsSetKeyRepeat(uint64_t delayNanos, uint64_t intervalNanos)
{
	kRepeatDelay = delayNanos;
	kRepeatInterval = intervalNanos;
}

/* A second click within 500 ms and 4 pixels of the first counts double. */
static const uint64_t kClickInterval = 500000000;
static const double kClickSlop = 4;

/* ---- The snapshot input routes over ---- */

/* A window, as routing sees it: what it was when the snapshot was made. */
@interface WSInputWindow : NSObject
@property SLSWindowID windowID;
@property SLSRect frame;
@property int32_t level;
@property (strong) WSConnection *owner;
@property (copy) NSData *dragRects;
@property (copy) NSData *dragExcludes;
@end

static BOOL
rectsContain(NSData *rects, double x, double y)
{
	const SLSIntRect *r = rects.bytes;
	NSUInteger n = rects.length / sizeof(SLSIntRect);

	for (NSUInteger i = 0; i < n; i++)
		if (x >= r[i].x && x < r[i].x + r[i].width && y >= r[i].y &&
		    y < r[i].y + r[i].height)
			return YES;
	return NO;
}

@implementation WSInputWindow
- (BOOL)dragRegionContainsX:(double)x y:(double)y
{
	return rectsContain(self.dragRects, x, y) && !rectsContain(self.dragExcludes, x, y);
}
@end

typedef struct {
	double x, y, width, height, menuBarHeight;
} WSInputDisplay;

@interface WSInputModel : NSObject
@property (strong) NSArray<WSInputWindow *> *windows;	/* on screen, front to back */
@property (strong) WSInputWindow *keyWindow;		/* or nil */
@property (strong) WSInputWindow *shield;		/* a capture's, or nil */
@property (strong) WSInputWindow *grab;			/* holds the pointer, or nil */
@property (strong) WSConnection *front;
@property (strong) NSData *displays;			/* WSInputDisplay */
@end

@implementation WSInputModel
@end

static pthread_mutex_t modelLock = PTHREAD_MUTEX_INITIALIZER;
static WSInputModel *model;	/* modelLock */
static BOOL modelChanged = YES;	/* IPC queue */

static WSInputModel *
currentModel(void)
{
	pthread_mutex_lock(&modelLock);
	WSInputModel *m = model;
	pthread_mutex_unlock(&modelLock);
	return m;
}

static WSInputWindow *
inputWindow(WSWindow *w)
{
	if (w == nil)
		return nil;
	WSInputWindow *i = [[WSInputWindow alloc] init];
	i.windowID = w.windowID;
	i.frame = w.frame;
	i.level = w.level;
	i.owner = w.owner;
	i.dragRects = w.dragRects;
	i.dragExcludes = w.dragExcludes;
	return i;
}

/* ---- The model: IPC queue ---- */

static __weak WSWindow *keyWindow;
/* Holds the pointer for an open menu (SLSCapturePointer). */
static __weak WSWindow *grabWindow;

void
WSInputModelChanged(void)
{
	modelChanged = YES;
}

void
WSInputPublish(void)
{
	if (!modelChanged)
		return;
	modelChanged = NO;
	WSInputModel *m = [[WSInputModel alloc] init];
	NSMutableArray *windows = [NSMutableArray array];
	NSMutableData *displays = [NSMutableData data];
	double minX = 0, minY = 0, maxX = 0, maxY = 0;
	BOOL first = YES;

	for (WSWindow *w in WSWindowStack())
		[windows addObject:inputWindow(w)];
	m.windows = windows;
	m.keyWindow = inputWindow(keyWindow);
	m.grab = inputWindow(grabWindow);
	m.front = WSFrontConnection();
	for (WSDisplay *d in WSDisplays) {
		WSInputDisplay info = { d.x, d.y, d.width, d.height, d.menuBarHeight };
		[displays appendBytes:&info length:sizeof(info)];
		if (m.shield == nil && d.shield != nil)
			m.shield = inputWindow(d.shield);
		if (first || d.x < minX) minX = d.x;
		if (first || d.y < minY) minY = d.y;
		if (first || d.x + d.width > maxX) maxX = d.x + d.width;
		if (first || d.y + d.height > maxY) maxY = d.y + d.height;
		first = NO;
	}
	m.displays = displays;
	WSCursorSetBounds(minX, minY, maxX, maxY);
	pthread_mutex_lock(&modelLock);
	model = m;
	pthread_mutex_unlock(&modelLock);
}

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
	WSInputModelChanged();
	WSCursorShapeChanged();
	tellFrontApp(c != nil ? c.pid : 0);
}

static void
setKeyWindow(WSWindow *w)
{
	keyWindow = w;
	WSInputModelChanged();
	if (w != nil && w.owner != frontConnection)
		makeFront(w.owner);
}

static void inputForget(SLSWindowID window);

void
WSEventsWindowGone(WSWindow *w)
{
	if (grabWindow == w)
		grabWindow = nil;
	if (keyWindow == w && w.owner != nil)
		keyWindow = WSWindowFrontmostOf(w.owner);
	WSInputModelChanged();
	inputForget(w.windowID);
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
	WSInputModelChanged();
}

/* ---- Queues: shared, under queuesLock ---- */

static pthread_mutex_t queuesLock = PTHREAD_MUTEX_INITIALIZER;

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

/* Any queue. */
void
WSEventsEnqueue(WSConnection *c, const SLSEvent *e)
{
	BOOL tell = NO;

	if (c == nil)
		return;
	pthread_mutex_lock(&queuesLock);
	NSMutableData *queue = c.eventQueue;
	NSUInteger n = queue.length / sizeof(SLSEvent);
	SLSEvent *last = n > 0 ? (SLSEvent *)queue.mutableBytes + n - 1 : NULL;

	/* Coalesced: the last move to the same window takes this one in. */
	if (last != NULL && isMove(e->type) && last->type == e->type &&
	    last->window == e->window) {
		last->x = e->x;
		last->y = e->y;
		last->dx += e->dx;
		last->dy += e->dy;
		last->flags = e->flags;
		last->timestamp = e->timestamp;
	} else if (n >= kQueueLimit)
		c.droppedEvents++;
	else {
		[queue appendBytes:e length:sizeof(*e)];
		tell = n == 0;
	}
	pthread_mutex_unlock(&queuesLock);
	if (tell)
		ping(c);
}

/* ---- Routing: input queue ---- */

static uint32_t buttonsDown;		/* bit per button number */
static uint64_t lastClickTime;
static double lastClickX, lastClickY;
static uint32_t lastClickButton;
static int32_t clickCount;
static dispatch_source_t repeatTimer;
static SLSEvent repeatEvent;
/* Windows by ID: the one that got the mouse down, the one being moved (and
 * the pointer's offset in it), the one the pointer was last over, and the
 * one a click made key before the IPC queue says so. */
static SLSWindowID captureID, moveID, hoverID, pendingKeyID;
static double moveOffsetX, moveOffsetY;
static SLSRect moveFrame;

/* A window went away (IPC queue): input forgets it. */
static void
inputForget(SLSWindowID window)
{
	dispatch_async(WSInputQueue, ^{
		if (captureID == window)
			captureID = 0;
		if (moveID == window)
			moveID = 0;
		if (hoverID == window)
			hoverID = 0;
		if (pendingKeyID == window)
			pendingKeyID = 0;
	});
}

static WSInputWindow *
windowWithID(WSInputModel *m, SLSWindowID window)
{
	if (window == 0)
		return nil;
	for (WSInputWindow *w in m.windows)
		if (w.windowID == window)
			return w;
	return nil;
}

static WSInputWindow *
windowAtPoint(WSInputModel *m, double x, double y)
{
	for (WSInputWindow *w in m.windows) {
		SLSRect f = w.frame;
		if (x >= f.x && x < f.x + f.width && y >= f.y && y < f.y + f.height)
			return w;
	}
	return nil;
}

static void
send(WSInputWindow *w, SLSEvent *e)
{
	if (w == nil || w.owner == nil)
		return;
	e->window = w.windowID;
	WSEventsEnqueue(w.owner, e);
}

/* The window keys go to: a capture's shielding window, else the key one
 * (the one a click just made key first). */
static WSInputWindow *
keyTarget(WSInputModel *m)
{
	if (m.shield != nil)
		return m.shield;
	WSInputWindow *pending = windowWithID(m, pendingKeyID);
	return pending != nil ? pending : m.keyWindow;
}

/* A click makes its window key: the IPC queue does, and says when. */
static void
makeKey(WSInputModel *m, WSInputWindow *w)
{
	SLSWindowID window = w.windowID;

	if (m.keyWindow.windowID == window && pendingKeyID == 0)
		return;
	pendingKeyID = window;
	dispatch_async(WSIPCQueue, ^{
		WSWindow *target = WSWindowWithID(window);
		if (target != nil)
			setKeyWindow(target);
		WSInputPublish();
		dispatch_async(WSInputQueue, ^{
			if (pendingKeyID == window)
				pendingKeyID = 0;
		});
	});
}

/* A server-side move: the IPC queue moves the window. */
static void
moveTo(SLSWindowID window, SLSRect f)
{
	dispatch_async(WSIPCQueue, ^{
		WSWindow *w = WSWindowWithID(window);
		if (w != nil)
			WSWindowSetFrame(w, f);
		WSInputPublish();
	});
}

/* A window moved by the server keeps its top below the menu bar of the
 * display under the pointer. */
static void
keepBelowMenuBar(WSInputModel *m, SLSRect *f, double px, double py)
{
	const WSInputDisplay *d = m.displays.bytes;
	NSUInteger n = m.displays.length / sizeof(*d);

	for (NSUInteger i = 0; i < n; i++) {
		if (px < d[i].x || py < d[i].y || px >= d[i].x + d[i].width ||
		    py >= d[i].y + d[i].height)
			continue;
		if (f->y < d[i].y + d[i].menuBarHeight)
			f->y = d[i].y + d[i].menuBarHeight;
		return;
	}
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
	repeatTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, WSInputQueue);
	dispatch_source_set_timer(repeatTimer, dispatch_time(DISPATCH_TIME_NOW, kRepeatDelay),
	    kRepeatInterval, kRepeatInterval / 10);
	dispatch_source_set_event_handler(repeatTimer, ^{
		WSInputWindow *target = keyTarget(currentModel());
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
routeKey(WSInputModel *m, SLSEvent *e)
{
	WSInputWindow *target = keyTarget(m);

	/* A system-wide shortcut goes to its owner, not to the key window. */
	if (WSHotKeysHandle(e, target.owner, target.windowID, m.front))
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
routeMouse(WSInputModel *m, SLSEvent *e)
{
	BOOL down = e->type == kSLSEventLeftMouseDown || e->type == kSLSEventRightMouseDown ||
	    e->type == kSLSEventOtherMouseDown;
	BOOL up = e->type == kSLSEventLeftMouseUp || e->type == kSLSEventRightMouseUp ||
	    e->type == kSLSEventOtherMouseUp;
	WSInputWindow *target;

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
	WSInputWindow *grab = m.grab;
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
	if (moveID != 0 && (isMove(e->type) || e->type == kSLSEventLeftMouseUp)) {
		WSInputWindow *moving = windowWithID(m, moveID);
		SLSRect f = moveFrame;
		BOOL ends = e->type == kSLSEventLeftMouseUp;
		f.x = e->x - moveOffsetX;
		f.y = e->y - moveOffsetY;
		keepBelowMenuBar(m, &f, e->x, e->y);
		moveFrame = f;
		moveTo(moveID, f);
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
			moveID = 0;
			buttonsDown &= ~1u;
			if (buttonsDown == 0)
				captureID = 0;
		}
		return;
	}

	if (down) {
		if (buttonsDown == 0) {
			WSInputWindow *hit = windowAtPoint(m, e->x, e->y);
			captureID = hit.windowID;
		}
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
		target = windowWithID(m, captureID);
		/* A click makes its window key; not a menu bar's, a menu's or a
		 * status item's (the menu levels and above), nor the Dock's or
		 * the desktop's (the Dock level, the desktop levels): the front
		 * app keeps its key window and menu bar, as on the reference
		 * platform. */
		if (target != nil && e->buttonNumber == 0 && target.level < kSLSMainMenuWindowLevel &&
		    target.level != kSLSDockWindowLevel && target.level > kSLSDesktopIconWindowLevel)
			makeKey(m, target);
		/* A left click in a drag region moves the window from now on,
		 * whether or not its app answers (it still gets this click). */
		if (target != nil && e->type == kSLSEventLeftMouseDown && buttonsDown == 1u &&
		    [target dragRegionContainsX:e->x - target.frame.x y:e->y - target.frame.y]) {
			moveID = target.windowID;
			moveFrame = target.frame;
			moveOffsetX = e->x - target.frame.x;
			moveOffsetY = e->y - target.frame.y;
		}
	} else if (up) {
		target = captureID ? windowWithID(m, captureID) : windowAtPoint(m, e->x, e->y);
		buttonsDown &= ~(1u << e->buttonNumber);
		e->clickCount = clickCount;
		if (buttonsDown == 0)
			captureID = 0;
	} else if (isMove(e->type) && buttonsDown != 0) {
		e->type = dragType();
		target = windowWithID(m, captureID);
	} else {
		target = windowAtPoint(m, e->x, e->y);
		/* The window the pointer left gets this move too, outside it: its
		 * app ends its tracking areas there (cursor rects back to the
		 * arrow, mouse exited). */
		WSInputWindow *left = windowWithID(m, hoverID);
		if (left != nil && left != target && e->type == kSLSEventMouseMoved) {
			SLSEvent last = *e;
			send(left, &last);
		}
		hoverID = target.windowID;
	}
	if (target != nil)
		send(target, e);
}

/* ---- The input state (SLSGetHIDState) ---- */

static SLSHIDState hidStates[2];	/* hardware, combined; hidLock */
static pthread_mutex_t hidLock = PTHREAD_MUTEX_INITIALIZER;

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
	pthread_mutex_lock(&hidLock);
	*state = hidStates[which];
	pthread_mutex_unlock(&hidLock);
	state->now = WSNanoseconds();
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

/* From the input devices (input queue). */
void
WSRouteEvent(SLSEvent *e)
{
	WSStatsCountEvent();
	pthread_mutex_lock(&hidLock);
	noteState(&hidStates[kSLSHIDStateHardware], e);
	pthread_mutex_unlock(&hidLock);
	WSRouteEventAt(e, 0);
}

/* From the devices, or posted at a tap location (input queue). */
void
WSRouteEventAt(SLSEvent *e, uint32_t location)
{
	pthread_mutex_lock(&hidLock);
	noteState(&hidStates[kSLSHIDStateCombined], e);
	pthread_mutex_unlock(&hidLock);
	if (!WSTapsFilter(e, location))
		return;
	WSInputModel *m = currentModel();
	if (e->type == kSLSEventKeyDown || e->type == kSLSEventKeyUp ||
	    e->type == kSLSEventFlagsChanged)
		routeKey(m, e);
	else
		routeMouse(m, e);
}

/* ---- Routines: IPC queue ---- */

kern_return_t
wss_next_events(mach_port_t server, audit_token_t token, ws_event_list_t events,
    mach_msg_type_number_t *count)
{
	WSConnection *c = [WSConnection connectionForToken:token];
	BOOL more;

	*count = 0;
	if (c == nil)
		return KERN_SUCCESS;
	pthread_mutex_lock(&queuesLock);
	NSMutableData *queue = c.eventQueue;
	NSUInteger n = queue.length / sizeof(SLSEvent);
	if (n > kSLSMaxEvents)
		n = kSLSMaxEvents;
	memcpy(events, queue.bytes, n * sizeof(SLSEvent));
	[queue replaceBytesInRange:NSMakeRange(0, n * sizeof(SLSEvent)) withBytes:NULL length:0];
	more = queue.length > 0;
	pthread_mutex_unlock(&queuesLock);
	*count = (mach_msg_type_number_t)n;
	/* More left: the client is told again. */
	if (more)
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
	else if (w.owner != c)
		*result = kSLSErrorCannotComplete;	/* not its window */
	else {
		/* The move is input's: it starts only with the left button down.
		 * The IPC queue may wait for the input queue, never the other way. */
		SLSRect frame = w.frame;
		__block int32_t r = kSLSErrorCannotComplete;
		dispatch_sync(WSInputQueue, ^{
			if (!(buttonsDown & 1))
				return;
			SLSPoint p = WSCursorPosition();
			moveID = window;
			moveFrame = frame;
			moveOffsetX = p.x - frame.x;
			moveOffsetY = p.y - frame.y;
			r = kSLSErrorSuccess;
		});
		*result = r;
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
		WSInputModelChanged();
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
		WSInputModelChanged();
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
		WSInputModelChanged();
		*result = kSLSErrorSuccess;
	} else if (w == nil)
		*result = kSLSErrorIllegalArgument;
	else if (w.owner != c || c != frontConnection)
		*result = kSLSErrorCannotComplete;	/* not its window, or not in front */
	else {
		grabWindow = w;
		WSInputModelChanged();
		/* What the pointer was doing stops: a move, a click's window. */
		dispatch_async(WSInputQueue, ^{
			moveID = 0;
			captureID = 0;
		});
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
