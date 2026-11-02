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

/*
 * Event taps (WindowServer plan, WS3): the taps' list, in order, and each
 * event passed through them before it is routed. A listen-only tap is sent
 * a copy and never waited for; a filtering tap is asked and waited for up
 * to a second, then disabled. Events pass them on the input queue; the
 * list changes on the IPC queue, under tapsLock.
 */

#include <mach/mach.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/stat.h>
#import "WSEvents.h"
#import "WSConnection.h"
#include "WindowServerServer.h"	/* MIG server side */

enum { kHIDTap = 0, kSessionTap = 1, kAnnotatedSessionTap = 2 };
enum { kTapOptionListenOnly = 1 };
static const mach_msg_timeout_t kTapTimeout = 1000;	/* ms */

@interface WSTap : NSObject
@property uint32_t tapID;
@property (weak) WSConnection *owner;
@property pid_t ownerPID;
@property uint32_t location, options;
@property uint64_t mask;
@property mach_port_t port;		/* send right */
@property BOOL enabled;
@property double minUsec, maxUsec, totalUsec;
@property uint64_t answers;
@end

@implementation WSTap
- (void)dealloc
{
	if (_port != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), _port);
}
@end

static NSMutableArray<WSTap *> *taps;	/* tapsLock */
static pthread_mutex_t tapsLock = PTHREAD_MUTEX_INITIALIZER;

static NSArray<WSTap *> *
tapList(void)
{
	pthread_mutex_lock(&tapsLock);
	NSArray *list = taps ? [taps copy] : @[];
	pthread_mutex_unlock(&tapsLock);
	return list;
}
static uint32_t nextTapID = 1;
static mach_port_t replyPort = MACH_PORT_NULL;

/*
 * SHORTCUT(M5): the built-in policy of section 9 of the plan, until the
 * privacy service (its own ADR, before gate M5) decides: root and the
 * console's user may post events and filter them, and listen to keys;
 * taken in WS3.
 */
static BOOL mayControlInput(WSConnection *c);

BOOL
WSMayControlInput(WSConnection *connection)
{
	return mayControlInput(connection);
}

static BOOL
mayControlInput(WSConnection *c)
{
	struct stat st;

	if (c.uid == 0)
		return YES;
	return stat("/dev/console", &st) == 0 && st.st_uid == c.uid;
}

static BOOL
seesKeys(uint64_t mask)
{
	return (mask & ((1ULL << kSLSEventKeyDown) | (1ULL << kSLSEventKeyUp) |
	    (1ULL << kSLSEventFlagsChanged))) != 0;
}

static void
sendTo(WSTap *tap, const SLSEvent *e, mach_port_t reply)
{
	SLSTapEventMessage msg;

	memset(&msg, 0, sizeof(msg));
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,
	    reply ? MACH_MSG_TYPE_MAKE_SEND_ONCE : 0);
	msg.header.msgh_size = sizeof(msg);
	msg.header.msgh_remote_port = tap.port;
	msg.header.msgh_local_port = reply;
	msg.header.msgh_id = kSLSTapEventMessageID;
	msg.tap = tap.tapID;
	msg.event = *e;
	mach_msg(&msg.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(msg), 0, MACH_PORT_NULL,
	    0, MACH_PORT_NULL);
}

/* Asks a filtering tap; NO when it drops the event. */
static BOOL
ask(WSTap *tap, SLSEvent *e)
{
	struct {
		SLSTapReplyMessage reply;
		mach_msg_trailer_t trailer;
	} msg;
	uint64_t start = WSNanoseconds();

	if (replyPort == MACH_PORT_NULL)
		mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &replyPort);
	sendTo(tap, e, replyPort);
	for (;;) {
		memset(&msg, 0, sizeof(msg));
		mach_msg_return_t r = mach_msg(&msg.reply.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT,
		    0, sizeof(msg), replyPort, kTapTimeout, MACH_PORT_NULL);
		if (r == MACH_RCV_TIMED_OUT) {
			/* Disabled, and told so; the event goes on. */
			SLSEvent disabled;
			memset(&disabled, 0, sizeof(disabled));
			disabled.type = kSLSEventTapDisabledByTimeout;
			tap.enabled = NO;
			sendTo(tap, &disabled, MACH_PORT_NULL);
			fprintf(stderr, "WindowServer: tap %u (pid %d) did not answer: disabled\n",
			    tap.tapID, tap.ownerPID);
			return YES;
		}
		if (r != MACH_MSG_SUCCESS)
			return YES;
		/* A late answer to an earlier event: not this one's. */
		if (msg.reply.header.msgh_id != kSLSTapReplyMessageID ||
		    msg.reply.event.timestamp != e->timestamp)
			continue;
		break;
	}
	double us = (WSNanoseconds() - start) / 1000.0;
	if (tap.answers == 0 || us < tap.minUsec)
		tap.minUsec = us;
	if (us > tap.maxUsec)
		tap.maxUsec = us;
	tap.totalUsec += us;
	tap.answers++;
	if (msg.reply.drop)
		return NO;
	/* A changed event keeps its place in the stream. */
	uint64_t timestamp = e->timestamp;
	*e = msg.reply.event;
	e->timestamp = timestamp;
	return YES;
}

BOOL
WSTapsFilter(SLSEvent *e, uint32_t location)
{
	for (WSTap *tap in tapList()) {
		if (!tap.enabled || tap.location < location || e->type >= 64 ||
		    !(tap.mask & (1ULL << e->type)))
			continue;
		if (tap.options & kTapOptionListenOnly)
			sendTo(tap, e, MACH_PORT_NULL);
		else if (!ask(tap, e))
			return NO;
	}
	return YES;
}

void
WSTapsConnectionDied(WSConnection *c)
{
	pthread_mutex_lock(&tapsLock);
	for (WSTap *tap in [taps copy])
		if (tap.owner == c || tap.owner == nil)
			[taps removeObject:tap];
	pthread_mutex_unlock(&tapsLock);
}

/* ---- Routines ---- */

kern_return_t
wss_tap_create(mach_port_t server, audit_token_t token, uint32_t location, uint32_t placement,
    uint32_t options, uint64_t mask, mach_port_t port, int32_t *result, uint32_t *tapID)
{
	WSConnection *c = [WSConnection connectionForToken:token];

	*tapID = 0;
	if (c == nil || location > kAnnotatedSessionTap || placement > 1 || options > 1) {
		*result = c ? kSLSErrorIllegalArgument : kSLSErrorInvalidConnection;
		mach_port_deallocate(mach_task_self(), port);
		return KERN_SUCCESS;
	}
	if ((!(options & kTapOptionListenOnly) || seesKeys(mask)) && !mayControlInput(c)) {
		*result = kSLSErrorCannotComplete;
		mach_port_deallocate(mach_task_self(), port);
		return KERN_SUCCESS;
	}
	WSTap *tap = [[WSTap alloc] init];
	tap.tapID = nextTapID++;
	tap.owner = c;
	tap.ownerPID = c.pid;
	tap.location = location;
	tap.options = options;
	tap.mask = mask;
	tap.port = port;
	tap.enabled = YES;
	/* Head-insert taps see events first. */
	pthread_mutex_lock(&tapsLock);
	if (taps == nil)
		taps = [NSMutableArray array];
	if (placement == 0)
		[taps insertObject:tap atIndex:0];
	else
		[taps addObject:tap];
	pthread_mutex_unlock(&tapsLock);
	*tapID = tap.tapID;
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

static WSTap *
ownTap(audit_token_t token, uint32_t tapID, int32_t *result)
{
	WSConnection *c = [WSConnection connectionForToken:token];

	for (WSTap *tap in tapList())
		if (tap.tapID == tapID) {
			if (tap.owner != c) {
				*result = kSLSErrorCannotComplete;
				return nil;
			}
			*result = kSLSErrorSuccess;
			return tap;
		}
	*result = kSLSErrorIllegalArgument;
	return nil;
}

kern_return_t
wss_tap_enable(mach_port_t server, audit_token_t token, uint32_t tapID, int32_t enable,
    int32_t *result)
{
	WSTap *tap = ownTap(token, tapID, result);

	if (tap != nil)
		tap.enabled = enable != 0;
	return KERN_SUCCESS;
}

kern_return_t
wss_tap_release(mach_port_t server, audit_token_t token, uint32_t tapID, int32_t *result)
{
	WSTap *tap = ownTap(token, tapID, result);

	if (tap != nil) {
		pthread_mutex_lock(&tapsLock);
		[taps removeObject:tap];
		pthread_mutex_unlock(&tapsLock);
	}
	return KERN_SUCCESS;
}

kern_return_t
wss_tap_list(mach_port_t server, ws_tap_list_t list, mach_msg_type_number_t *count)
{
	mach_msg_type_number_t n = 0;

	for (WSTap *tap in tapList()) {
		if (n == kSLSMaxEventTaps)
			break;
		SLSEventTapInfo *info = &list[n++];
		memset(info, 0, sizeof(*info));
		info->mask = tap.mask;
		info->tap = tap.tapID;
		info->location = tap.location;
		info->options = tap.options;
		info->tappingPID = tap.ownerPID;
		info->enabled = tap.enabled;
		info->minUsecLatency = tap.minUsec;
		info->maxUsecLatency = tap.maxUsec;
		info->avgUsecLatency = tap.answers ? tap.totalUsec / tap.answers : 0;
	}
	*count = n;
	return KERN_SUCCESS;
}

kern_return_t
wss_post_event(mach_port_t server, audit_token_t token, uint32_t location, SLSEvent event,
    int32_t *result)
{
	WSConnection *c = [WSConnection connectionForToken:token];

	if (c == nil)
		*result = kSLSErrorInvalidConnection;
	else if (!mayControlInput(c))
		*result = kSLSErrorCannotComplete;
	else if (location > kAnnotatedSessionTap || event.type == 0 || event.type >= 64)
		*result = kSLSErrorIllegalArgument;
	else {
		event.window = 0;
		event.timestamp = WSNanoseconds();
		/* Routed as input, on its queue, after what came before it there
		 * (the keymap is input's too). */
		dispatch_async(WSInputQueue, ^{
			SLSEvent e = event;
			if ((e.type == kSLSEventKeyDown || e.type == kSLSEventKeyUp) &&
			    e.characters[0] == '\0') {
				uint32_t flags = e.flags;
				WSInputTranslatePosted(&e);
				e.flags |= flags;
			}
			BOOL mouse = e.type != kSLSEventKeyDown && e.type != kSLSEventKeyUp &&
			    e.type != kSLSEventFlagsChanged;
			WSRouteEventAt(&e, location);
			/* The HID system's cursor follows a posted mouse event. */
			if (mouse) {
				SLSPoint p = WSCursorPosition();
				WSInputSetCursor(p.x, p.y);
			}
		});
		*result = kSLSErrorSuccess;
	}
	return KERN_SUCCESS;
}
