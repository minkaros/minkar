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
 * The window list of the new server. The on-screen windows are kept front
 * to back, sorted by level; the windows ordered out follow in the order
 * they were made.
 */

#include <libproc.h>
#include <math.h>
#include <stdio.h>
#import "WSWindowList.h"
#import "WSConnection.h"
#import "WSCompositor.h"
#import "WSSurface.h"
#include "WindowServerServer.h"	/* MIG server side */

@interface WSWindow ()
@property (readwrite) int32_t level;
@property (readwrite) SLSRect frame;
@property (readwrite) BOOL onScreen;
@property (readwrite) double alpha;
@property (readwrite, strong) id surface;
/* A surface of the new size, given to the client but not committed yet. */
@property (strong) WSSurface *pendingSurface;
@end

@implementation WSWindow
- (instancetype)initWithID:(SLSWindowID)windowID owner:(WSConnection *)owner
{
	if ((self = [super init]) != nil) {
		_windowID = windowID;
		_owner = owner;
		_alpha = 1;
	}
	return self;
}
@end

static NSMutableArray<WSWindow *> *stack;	/* on screen, front to back */
static NSMutableArray<WSWindow *> *allWindows;	/* in the order they were made */
static SLSWindowID nextWindowID = 1;

static WSWindow *
windowWithID(SLSWindowID windowID)
{
	for (WSWindow *w in allWindows)
		if (w.windowID == windowID)
			return w;
	return nil;
}

/* What the window covers on screen, if it is on screen: its frame, or its
 * surface where a surface of the old size is still shown. */
static void
damageWindow(WSWindow *w)
{
	SLSRect r = w.frame;
	WSSurface *surface = w.surface;

	if (!w.onScreen)
		return;
	if (surface != nil && surface.width > r.width)
		r.width = surface.width;
	if (surface != nil && surface.height > r.height)
		r.height = surface.height;
	WSDamageGlobalRect(r);
}

static BOOL
frameIsValid(SLSRect f)
{
	return isfinite(f.x) && isfinite(f.y) && isfinite(f.width) && isfinite(f.height) &&
	    f.width >= 0 && f.height >= 0;
}

WSWindow *
WSWindowCreate(WSConnection *owner, int32_t level, SLSRect frame)
{
	if (allWindows == nil) {
		allWindows = [NSMutableArray array];
		stack = [NSMutableArray array];
	}
	WSWindow *w = [[WSWindow alloc] initWithID:nextWindowID++ owner:owner];
	w.level = level;
	w.frame = frame;
	[allWindows addObject:w];
	return w;
}

void
WSWindowRelease(WSWindow *w)
{
	damageWindow(w);
	[stack removeObject:w];
	[allWindows removeObject:w];
	w.onScreen = NO;
}

void
WSWindowSetFrame(WSWindow *w, SLSRect frame)
{
	damageWindow(w);
	w.frame = frame;
	damageWindow(w);
}

/* Where a window of this level goes: the front of its level, or the back. */
static NSUInteger
levelEdge(int32_t level, BOOL front)
{
	NSUInteger i = 0;

	while (i < stack.count &&
	    (front ? stack[i].level > level : stack[i].level >= level))
		i++;
	return i;
}

int
WSWindowOrder(WSWindow *w, int32_t place, SLSWindowID relativeTo)
{
	WSWindow *relative = nil;

	if (place != kSLSOrderOut && place != kSLSOrderAbove && place != kSLSOrderBelow)
		return kSLSErrorIllegalArgument;
	if (relativeTo != 0) {
		relative = windowWithID(relativeTo);
		if (relative == nil || relative == w)
			return kSLSErrorIllegalArgument;
	}
	damageWindow(w);
	[stack removeObject:w];
	w.onScreen = NO;
	if (place == kSLSOrderOut)
		return kSLSErrorSuccess;

	NSUInteger index;
	/* Next to the relative window when it is on screen at the same level;
	 * otherwise at the front (above) or back (below) of the level. */
	if (relative != nil && relative.onScreen && relative.level == w.level) {
		index = [stack indexOfObject:relative];
		if (place == kSLSOrderBelow)
			index++;
	} else
		index = levelEdge(w.level, place == kSLSOrderAbove);
	[stack insertObject:w atIndex:index];
	w.onScreen = YES;
	damageWindow(w);
	return kSLSErrorSuccess;
}

unsigned
WSWindowsConnectionDied(WSConnection *c)
{
	unsigned n = 0;

	for (WSWindow *w in [allWindows copy])
		if (w.owner == c || w.owner == nil) {
			WSWindowRelease(w);
			n++;
		}
	return n;
}

/* ---- Routines ---- */

/* The caller's own window, or why not. */
static WSWindow *
ownWindow(audit_token_t token, SLSWindowID windowID, int32_t *result)
{
	WSConnection *c = [WSConnection connectionForToken:token];
	WSWindow *w = windowWithID(windowID);

	if (c == nil)
		*result = kSLSErrorInvalidConnection;
	else if (w == nil)
		*result = kSLSErrorIllegalArgument;
	else if (w.owner != c)
		*result = kSLSErrorCannotComplete;
	else {
		*result = kSLSErrorSuccess;
		return w;
	}
	return nil;
}

kern_return_t
wss_window_create(mach_port_t server, audit_token_t token, int32_t level, SLSRect frame,
    int32_t *result, uint32_t *window)
{
	WSConnection *c = [WSConnection connectionForToken:token];

	*window = 0;
	if (c == nil)
		*result = kSLSErrorInvalidConnection;
	else if (!frameIsValid(frame))
		*result = kSLSErrorIllegalArgument;
	else {
		*window = WSWindowCreate(c, level, frame).windowID;
		*result = kSLSErrorSuccess;
	}
	return KERN_SUCCESS;
}

kern_return_t
wss_window_release(mach_port_t server, audit_token_t token, uint32_t window,
    int32_t *result)
{
	WSWindow *w = ownWindow(token, window, result);

	if (w != nil) {
		/* A shielding window goes with its capture. */
		for (WSDisplay *d in WSDisplays)
			if (d.shield == w) {
				*result = kSLSErrorCannotComplete;
				return KERN_SUCCESS;
			}
		WSWindowRelease(w);
	}
	return KERN_SUCCESS;
}

kern_return_t
wss_window_set_frame(mach_port_t server, audit_token_t token, uint32_t window,
    SLSRect frame, int32_t *result)
{
	WSWindow *w = ownWindow(token, window, result);

	if (w != nil) {
		if (frameIsValid(frame))
			WSWindowSetFrame(w, frame);
		else
			*result = kSLSErrorIllegalArgument;
	}
	return KERN_SUCCESS;
}

kern_return_t
wss_window_set_level(mach_port_t server, audit_token_t token, uint32_t window,
    int32_t level, int32_t *result)
{
	WSWindow *w = ownWindow(token, window, result);

	if (w != nil && w.level != level) {
		w.level = level;
		/* On screen, it moves to the front of its new level. */
		if (w.onScreen)
			WSWindowOrder(w, kSLSOrderAbove, 0);
	}
	return KERN_SUCCESS;
}

kern_return_t
wss_window_order(mach_port_t server, audit_token_t token, uint32_t window, int32_t place,
    uint32_t relative, int32_t *result)
{
	WSWindow *w = ownWindow(token, window, result);

	if (w != nil)
		*result = WSWindowOrder(w, place, relative);
	return KERN_SUCCESS;
}

/* The list in CGWindowListCopyWindowInfo's order: on screen front to back,
 * then the others. */
static NSArray<WSWindow *> *
orderedWindows(void)
{
	NSMutableArray *list = [NSMutableArray arrayWithArray:stack];

	for (WSWindow *w in allWindows)
		if (!w.onScreen)
			[list addObject:w];
	return list;
}

kern_return_t
wss_window_list(mach_port_t server, uint32_t option, uint32_t relativeTo,
    ws_window_list_t windows, mach_msg_type_number_t *count)
{
	const uint32_t relativeOptions = kSLSWindowListOnScreenAboveWindow |
	    kSLSWindowListOnScreenBelowWindow;
	NSArray<WSWindow *> *list = orderedWindows();
	NSUInteger pivot = NSNotFound;

	*count = 0;
	if ((option & (relativeOptions | kSLSWindowListIncludingWindow)) && relativeTo != 0) {
		for (NSUInteger i = 0; i < list.count; i++)
			if (list[i].windowID == relativeTo)
				pivot = i;
		if (pivot == NSNotFound)
			return KERN_SUCCESS;	/* an unknown window: an empty list */
	}
	for (NSUInteger i = 0; i < list.count && *count < kSLSMaxWindowList; i++) {
		WSWindow *w = list[i];
		BOOL keep;

		if ((option & kSLSWindowListExcludeDesktopElements) &&
		    w.level <= kSLSDesktopIconWindowLevel)
			continue;
		if (option & relativeOptions) {
			if (pivot == NSNotFound)
				keep = w.onScreen;
			else if (i == pivot)
				keep = (option & kSLSWindowListIncludingWindow) != 0;
			else if (i < pivot)
				keep = (option & kSLSWindowListOnScreenAboveWindow) && w.onScreen;
			else
				keep = (option & kSLSWindowListOnScreenBelowWindow) && w.onScreen;
		} else if (option & kSLSWindowListIncludingWindow)
			keep = (pivot != NSNotFound && i == pivot);
		else if (option & kSLSWindowListOnScreenOnly)
			keep = w.onScreen;
		else
			keep = YES;
		if (keep)
			windows[(*count)++] = w.windowID;
	}
	return KERN_SUCCESS;
}

kern_return_t
wss_window_info(mach_port_t server, uint32_t window, int32_t *result, SLSWindowInfo *info)
{
	WSWindow *w = windowWithID(window);

	memset(info, 0, sizeof(*info));
	if (w == nil) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	info->window = w.windowID;
	info->level = w.level;
	info->ownerPID = w.owner.pid;
	info->connection = w.owner.connectionID;
	info->x = w.frame.x;
	info->y = w.frame.y;
	info->width = w.frame.width;
	info->height = w.frame.height;
	info->alpha = w.alpha;
	info->flags = w.onScreen ? kSLSWindowOnScreen : 0;
	info->sharingState = 1;		/* kCGWindowSharingReadOnly */
	if (proc_name(w.owner.pid, info->ownerName, sizeof(info->ownerName)) <= 0)
		info->ownerName[0] = '\0';
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

kern_return_t
wss_window_set_alpha(mach_port_t server, audit_token_t token, uint32_t window, double alpha,
    int32_t *result)
{
	WSWindow *w = ownWindow(token, window, result);

	if (w == nil)
		return KERN_SUCCESS;
	if (!isfinite(alpha) || alpha < 0 || alpha > 1) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	w.alpha = alpha;
	damageWindow(w);
	return KERN_SUCCESS;
}

/* ---- Surfaces ---- */

kern_return_t
wss_window_surface(mach_port_t server, audit_token_t token, uint32_t window, int32_t *result,
    uint32_t *surfaceID, uint32_t *width, uint32_t *height, uint32_t *rowBytes,
    uint32_t *back, mach_port_t *memory)
{
	WSWindow *w = ownWindow(token, window, result);

	*surfaceID = *width = *height = *rowBytes = *back = 0;
	*memory = MACH_PORT_NULL;
	if (w == nil)
		return KERN_SUCCESS;
	double fw = ceil(w.frame.width), fh = ceil(w.frame.height);
	if (fw < 1 || fh < 1 || fw > kSLSMaxSurfaceSide || fh > kSLSMaxSurfaceSide) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	WSSurface *current = w.pendingSurface ? w.pendingSurface : w.surface;
	if (current == nil || current.width != fw || current.height != fh) {
		current = [[WSSurface alloc] initWithWidth:(uint32_t)fw height:(uint32_t)fh];
		if (current == nil) {
			*result = kSLSErrorFailure;
			return KERN_SUCCESS;
		}
		/* A shown surface stays until the new one is committed. */
		if (w.surface == nil || ![w.surface committed]) {
			w.surface = current;
			w.pendingSurface = nil;
		} else
			w.pendingSurface = current;
	}
	WSSurface *given = current;
	*surfaceID = given.surfaceID;
	*width = given.width;
	*height = given.height;
	*rowBytes = given.rowBytes;
	*back = given.backBuffer;
	*memory = given.memoryEntry;
	return KERN_SUCCESS;
}

kern_return_t
wss_window_commit(mach_port_t server, audit_token_t token, uint32_t window, uint32_t surfaceID,
    uint32_t buffer, ws_damage_t damage, mach_msg_type_number_t count, int32_t *result,
    uint32_t *back)
{
	WSWindow *w = ownWindow(token, window, result);
	WSSurface *surface;

	*back = 0;
	if (w == nil)
		return KERN_SUCCESS;
	surface = w.pendingSurface ? w.pendingSurface : w.surface;
	if (surface == nil || surface.surfaceID != surfaceID) {
		*result = kSLSErrorRangeCheck;	/* a surface the window no longer has */
		return KERN_SUCCESS;
	}
	BOOL first = !surface.committed;
	if (![surface commitBuffer:buffer]) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	if (w.pendingSurface != nil) {
		/* The new size shows from now: the old area and the new one change. */
		damageWindow(w);
		w.surface = w.pendingSurface;
		w.pendingSurface = nil;
		first = YES;
	}
	if (first)
		damageWindow(w);
	else if (w.onScreen)
		for (mach_msg_type_number_t i = 0; i < count; i++)
			WSDamageGlobalRect((SLSRect){ w.frame.x + damage[i].x, w.frame.y + damage[i].y,
			    damage[i].width, damage[i].height });
	WSStatsCountCommit();
	*back = surface.backBuffer;
	return KERN_SUCCESS;
}

/* ---- Frames ---- */

NSArray<WSFrameItem *> *
WSWindowSnapshot(void)
{
	NSMutableArray *items = [NSMutableArray arrayWithCapacity:stack.count];

	for (WSWindow *w in [stack reverseObjectEnumerator]) {
		WSFrameItem *item = [[WSFrameItem alloc] init];
		item.frame = w.frame;
		item.alpha = w.alpha;
		item.surface = w.surface;
		item.solid = w.shield;
		[items addObject:item];
	}
	return items;
}
