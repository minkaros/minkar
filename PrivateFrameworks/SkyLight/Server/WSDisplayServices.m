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
 * The display services of the new server: displays, modes and
 * configurations, transfer tables, capture and the cursor (the
 * CGDirectDisplay API).
 */

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#import "WSServer.h"
#import "WSConnection.h"
#import "WSEvents.h"
#import "WSWindowList.h"
#import "WSCompositor.h"
#import "WSDisplayStream.h"
#include "WindowServerServer.h"	/* MIG server side */

/* A transfer table or the fade changed (below). */
static void colorsChanged(void);

NSArray<WSDisplay *> *WSDisplays;

/* ---- Displays ---- */

WSDisplay *
WSDisplayWithID(uint32_t ID)
{
	for (WSDisplay *d in WSDisplays)
		if (d.displayID == ID)
			return d;
	return nil;
}

kern_return_t
wss_main_display(mach_port_t server, uint32_t *display)
{
	*display = WSDisplays.firstObject.displayID;
	return KERN_SUCCESS;
}

kern_return_t
wss_display_list(mach_port_t server, uint32_t which, ws_display_list_t list,
    mach_msg_type_number_t *count)
{
	uint32_t want = (which == 0) ? kSLSDisplayOnline : kSLSDisplayActive;
	mach_msg_type_number_t n = 0;

	for (WSDisplay *d in WSDisplays)
		if ((d.flags & want) && n < 32)
			list[n++] = d.displayID;
	*count = n;
	return KERN_SUCCESS;
}

kern_return_t
wss_display_info(mach_port_t server, uint32_t display, int32_t *result,
    SLSDisplayInfo *info)
{
	WSDisplay *d = WSDisplayWithID(display);

	memset(info, 0, sizeof(*info));
	if (d == nil) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	[d getInfo:info];
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

kern_return_t
wss_display_menu_bar_height(mach_port_t server, uint32_t display, int32_t *result,
    uint32_t *height)
{
	WSDisplay *d = WSDisplayWithID(display);

	*height = 0;
	if (d == nil) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	*height = d.menuBarHeight;
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

/* ---- Display modes and configuration ---- */

kern_return_t
wss_display_modes(mach_port_t server, uint32_t display, int32_t *result,
    ws_display_mode_list_t modes, mach_msg_type_number_t *count)
{
	WSDisplay *d = WSDisplayWithID(display);

	*count = 0;
	if (d == nil) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	for (uint32_t i = 0; i < d.modeCount && i < kSLSMaxDisplayModes; i++)
		modes[(*count)++] = *[d modeAtIndex:i];
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

kern_return_t
wss_display_current_mode(mach_port_t server, uint32_t display, int32_t *result,
    SLSDisplayMode *mode)
{
	WSDisplay *d = WSDisplayWithID(display);
	const SLSDisplayMode *current = [d currentMode];

	memset(mode, 0, sizeof(*mode));
	if (current == NULL) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	*mode = *current;
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

/* The public CGDisplayChangeSummaryFlags the notices carry. */
enum {
	kChangeMoved		= 1 << 1,
	kChangeSetMode		= 1 << 3,
	kChangeMirror		= 1 << 10,
	kChangeUnMirror		= 1 << 11,
	kChangeDesktopShape	= 1 << 12,
};

/*
 * Tells every connection what changed on each display since the last
 * notice (kSLSEventDisplayReconfigured): apps update their screens and
 * keep their windows on them, as CGDisplayRegisterReconfigurationCallback
 * and NSApplicationDidChangeScreenParametersNotification promise.
 */
static void
notifyReconfigured(void)
{
	static NSMutableDictionary<NSNumber *, NSArray<NSNumber *> *> *notified;

	if (notified == nil)
		notified = [NSMutableDictionary dictionary];
	for (WSDisplay *d in WSDisplays) {
		NSNumber *key = @(d.displayID);
		NSArray<NSNumber *> *now = @[ @(d.currentMode->modeID), @(d.x), @(d.y),
		    @(d.width), @(d.height), @(d.mirrorOf) ];
		NSArray<NSNumber *> *was = notified[key];
		uint32_t flags = 0;

		notified[key] = now;
		if (was == nil)
			continue;	/* as it started: nothing to tell */
		if (![was[0] isEqual:now[0]])
			flags |= kChangeSetMode;
		if (![was[1] isEqual:now[1]] || ![was[2] isEqual:now[2]])
			flags |= kChangeMoved;
		if (![was[3] isEqual:now[3]] || ![was[4] isEqual:now[4]] ||
		    (flags & kChangeMoved))
			flags |= kChangeDesktopShape;
		if (![was[5] isEqual:now[5]])
			flags |= now[5].unsignedIntValue != 0 ? kChangeMirror : kChangeUnMirror;
		if (flags == 0)
			continue;

		SLSEvent e;
		memset(&e, 0, sizeof(e));
		e.type = kSLSEventDisplayReconfigured;
		e.buttonNumber = d.displayID;
		e.flags = flags;
		e.x = d.x;
		e.y = d.y;
		e.dx = d.width;
		e.dy = d.height;
		e.timestamp = WSNanoseconds();
		for (WSConnection *c in [WSConnection allConnections])
			WSEventsEnqueue(c, &e);
	}
}

/* Keeps the main display at the global origin, the others where they were
 * relative to it, and the shielding windows over their displays. */
static void
displaysMoved(void)
{
	WSInputModelChanged();
	WSDisplay *first = WSDisplays.firstObject;
	double dx = first.x, dy = first.y;

	for (WSDisplay *d in WSDisplays) {
		d.x -= dx;
		d.y -= dy;
		if (d.shield != nil)
			WSWindowSetFrame(d.shield, (SLSRect){ d.x, d.y, d.width, d.height });
		/* Its size or its place on the desktop may have changed. */
		[d.compositor displayChanged];
	}
	notifyReconfigured();
}

/*
 * Permanent configurations: each display's mode (by size and refresh
 * rate, since mode IDs may change) and place, by its persistent key,
 * saved when a client configures permanently and applied at startup.
 */
static NSString *const kWSDisplaysPath = @"/var/db/WindowServer/Displays.plist";

/* Every display's mode and place, by persistent key. */
static NSDictionary *
currentConfiguration(void)
{
	NSMutableDictionary *all = [NSMutableDictionary dictionary];

	for (WSDisplay *d in WSDisplays) {
		const SLSDisplayMode *m = d.currentMode;
		if (m == NULL)
			continue;
		all[d.persistentKey] = @{
			@"Width": @(m->pixelWidth), @"Height": @(m->pixelHeight),
			@"RefreshRate": @(m->refreshRate), @"X": @(d.x), @"Y": @(d.y),
		};
	}
	return all;
}

static void
saveDisplays(void)
{
	if (![currentConfiguration() writeToFile:kWSDisplaysPath atomically:YES])
		fprintf(stderr, "WindowServer: cannot save %s\n", kWSDisplaysPath.UTF8String);
}

/* Sets each display of the configuration to its mode and place. */
static void
applyConfiguration(NSDictionary *all)
{
	for (WSDisplay *d in WSDisplays) {
		NSDictionary *saved = all[d.persistentKey];
		if (![saved isKindOfClass:[NSDictionary class]])
			continue;
		/* integerValue and doubleValue: numbers, or strings in a file
		 * written in the old text format. */
		uint32_t width = (uint32_t)[saved[@"Width"] integerValue];
		uint32_t height = (uint32_t)[saved[@"Height"] integerValue];
		double hz = [saved[@"RefreshRate"] doubleValue];
		const SLSDisplayMode *best = NULL;
		for (uint32_t i = 0; i < d.modeCount; i++) {
			const SLSDisplayMode *m = [d modeAtIndex:i];
			if (m->pixelWidth != width || m->pixelHeight != height)
				continue;
			if (best == NULL || fabs(m->refreshRate - hz) < fabs(best->refreshRate - hz))
				best = m;
		}
		if (best != NULL && (d.currentMode == NULL || best->modeID != d.currentMode->modeID) &&
		    [d setModeID:best->modeID] == kSLSErrorSuccess)
			fprintf(stderr, "WindowServer: display 0x%x: saved mode %ux%u\n",
			    d.displayID, width, height);
		d.x = [saved[@"X"] doubleValue];
		d.y = [saved[@"Y"] doubleValue];
		d.mirrorOf = 0;
	}
	/* The main display stays at the origin. */
	WSDisplay *first = WSDisplays.firstObject;
	double dx = first.x, dy = first.y;
	for (WSDisplay *d in WSDisplays) {
		d.x -= dx;
		d.y -= dy;
	}
}

/* How the displays were when the server started: the permanent
 * configuration while none was saved. */
static NSDictionary *startupConfiguration;

void
WSDisplaysRestoreSaved(void)
{
	NSDictionary *saved = [NSDictionary dictionaryWithContentsOfFile:kWSDisplaysPath];

	if (saved != nil) {
		applyConfiguration(saved);
		fprintf(stderr, "WindowServer: applied the saved display configuration\n");
	}
	startupConfiguration = currentConfiguration();
	/* What the displays are now, for the first notice to compare. */
	notifyReconfigured();
}

kern_return_t
wss_display_restore_permanent(mach_port_t server, audit_token_t token, int32_t *result)
{
	NSDictionary *saved = [NSDictionary dictionaryWithContentsOfFile:kWSDisplaysPath];

	/* The caller's app-only changes end here. */
	[[WSConnection connectionForToken:token].appOnlyDisplays removeAllObjects];
	applyConfiguration(saved != nil ? saved : startupConfiguration);
	displaysMoved();
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

kern_return_t
wss_display_configure(mach_port_t server, audit_token_t token, uint32_t option,
    ws_display_config_list_t ops, mach_msg_type_number_t count, int32_t *result,
    int32_t *failed)
{
	WSConnection *connection = [WSConnection connectionForToken:token];

	*failed = -1;
	if (connection == nil) {
		*result = kSLSErrorInvalidConnection;
		return KERN_SUCCESS;
	}
	if (option > kSLSConfigurePermanently) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	/* Every change is checked before any is applied. */
	for (mach_msg_type_number_t i = 0; i < count; i++) {
		WSDisplay *d = WSDisplayWithID(ops[i].display);
		BOOL good = d != nil;

		if (good && ops[i].kind == kSLSConfigOpMode)
			good = [d modeWithID:ops[i].modeID] != NULL;
		else if (good && ops[i].kind == kSLSConfigOpMirror && ops[i].mirrorOf != 0) {
			/* Neither itself nor a display that is a mirror. */
			WSDisplay *primary = WSDisplayWithID(ops[i].mirrorOf);
			good = primary != nil && primary != d && primary.mirrorOf == 0;
		} else if (good && ops[i].kind != kSLSConfigOpOrigin &&
		    ops[i].kind != kSLSConfigOpMirror)
			good = NO;
		if (!good) {
			*result = kSLSErrorIllegalArgument;
			*failed = (int32_t)i;
			return KERN_SUCCESS;
		}
	}
	/*
	 * An app-only change remembers how the display was, to put it back when
	 * the app exits; a session change makes the display the session's.
	 */
	for (mach_msg_type_number_t i = 0; i < count; i++) {
		WSDisplay *d = WSDisplayWithID(ops[i].display);
		NSNumber *key = @(d.displayID);

		if (option != kSLSConfigureForAppOnly)
			[connection.appOnlyDisplays removeObjectForKey:key];
		else if (connection.appOnlyDisplays[key] == nil)
			connection.appOnlyDisplays[key] = @[ @(d.currentMode->modeID), @(d.x), @(d.y),
			    @(d.mirrorOf) ];
	}
	for (mach_msg_type_number_t i = 0; i < count; i++) {
		WSDisplay *d = WSDisplayWithID(ops[i].display);

		if (ops[i].kind == kSLSConfigOpMode) {
			int err = [d setModeID:ops[i].modeID];
			if (err != kSLSErrorSuccess) {
				*result = err;
				*failed = (int32_t)i;
				displaysMoved();
				return KERN_SUCCESS;
			}
		} else if (ops[i].kind == kSLSConfigOpMirror) {
			/* A mirror shows its primary's area of the desktop. */
			WSDisplay *primary = WSDisplayWithID(ops[i].mirrorOf);
			d.mirrorOf = ops[i].mirrorOf;
			if (primary != nil) {
				d.x = primary.x;
				d.y = primary.y;
			}
		} else {
			d.x = ops[i].x;
			d.y = ops[i].y;
		}
	}
	displaysMoved();
	if (option == kSLSConfigurePermanently)
		saveDisplays();
	fprintf(stderr, "WindowServer: pid %d configured %u display change(s), option %u\n",
	    audit_token_to_pid(token), count, option);
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

/* ---- Gamma ---- */

kern_return_t
wss_display_set_transfer_table(mach_port_t server, audit_token_t token, uint32_t display,
    uint32_t count, ws_gamma_table_t table, mach_msg_type_number_t tableCount,
    int32_t *result)
{
	WSDisplay *d = WSDisplayWithID(display);

	if (d == nil || tableCount != 3 * count)
		*result = kSLSErrorIllegalArgument;
	else
		*result = [d setTransferTable:table count:count];
	if (*result == kSLSErrorSuccess) {
		[WSConnection connectionForToken:token].changedTransfers = YES;
		colorsChanged();
	}
	return KERN_SUCCESS;
}

kern_return_t
wss_display_transfer_table(mach_port_t server, uint32_t display, int32_t *result,
    ws_gamma_table_t table, mach_msg_type_number_t *count)
{
	WSDisplay *d = WSDisplayWithID(display);

	*count = 0;
	if (d == nil) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	[d getTransferTable:table];
	*count = 3 * kSLSGammaTableCapacity;
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

kern_return_t
wss_display_set_transfer_formula(mach_port_t server, audit_token_t token, uint32_t display,
    SLSGammaFormula formula, int32_t *result)
{
	WSDisplay *d = WSDisplayWithID(display);

	*result = d ? [d setTransferFormula:&formula] : kSLSErrorIllegalArgument;
	if (*result == kSLSErrorSuccess) {
		[WSConnection connectionForToken:token].changedTransfers = YES;
		colorsChanged();
	}
	return KERN_SUCCESS;
}

kern_return_t
wss_display_transfer_formula(mach_port_t server, uint32_t display, int32_t *result,
    SLSGammaFormula *formula)
{
	WSDisplay *d = WSDisplayWithID(display);

	memset(formula, 0, sizeof(*formula));
	if (d == nil) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	*formula = d.transferFormula;
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

kern_return_t
wss_restore_transfers(mach_port_t server, audit_token_t token)
{
	for (WSDisplay *d in WSDisplays)
		[d restoreTransfer];
	for (WSConnection *c in [WSConnection allConnections])
		c.changedTransfers = NO;
	colorsChanged();
	return KERN_SUCCESS;
}

/* ---- Display fades ---- */

static SLSFadeReservation fadeReservation, nextFadeReservation = 1;
static __weak WSConnection *fadeHolder;
static __weak WSConnection *fadedBy;	/* who set the current blend */
static NSTimeInterval fadeExpires;
static float fadeBlend, fadeColor[3];
/* A fade going on: from fadeFrom to fadeBlend between these times. */
static float fadeFrom;
static NSTimeInterval fadeStart, fadeDuration;

/* The blend now: along the fade going on, if any (IPC queue). */
static float
currentBlend(BOOL *going)
{
	*going = NO;
	if (fadeDuration <= 0)
		return fadeBlend;
	double t = ([NSDate timeIntervalSinceReferenceDate] - fadeStart) / fadeDuration;
	if (t >= 1) {
		fadeDuration = 0;
		return fadeBlend;
	}
	*going = YES;
	return fadeFrom + (float)t * (fadeBlend - fadeFrom);
}

/* ---- What the screen shows: transfer tables and the fade ---- */

/*
 * A display's colour table for the compositor: 256 bytes for each of red,
 * green and blue, its transfer table with the fade's colour mixed in by
 * the fade's blend; nil when it changes nothing (the identity, no fade).
 */
NSData *
WSDisplayColorTable(WSDisplay *d)
{
	const uint32_t cap = kSLSGammaTableCapacity;
	float table[3 * kSLSGammaTableCapacity];
	BOOL going;
	float blend = currentBlend(&going);
	BOOL identity = blend == 0;
	uint8_t lut[3 * 256];

	/* A fade going on draws the next frame too. */
	if (going)
		colorsChanged();
	[d getTransferTable:table];
	for (uint32_t c = 0; c < 3; c++)
		for (uint32_t v = 0; v < 256; v++) {
			float x = table[c * cap + v * (cap - 1) / 255];
			float y = (1 - blend) * x + blend * fadeColor[c];
			if (y < 0)
				y = 0;
			if (y > 1)
				y = 1;
			lut[c * 256 + v] = (uint8_t)lroundf(y * 255);
			if (lut[c * 256 + v] != v)
				identity = NO;
		}
	return identity ? nil : [NSData dataWithBytes:lut length:sizeof(lut)];
}

/* A transfer table or the fade changed: every display is drawn again. */
static void
colorsChanged(void)
{
	for (WSDisplay *d in WSDisplays)
		[(WSCompositor *)d.compositor damageAll];
}

/* The holder of a reservation still in time, or nil. */
static WSConnection *
currentFadeHolder(void)
{
	if (fadeHolder != nil && [NSDate timeIntervalSinceReferenceDate] > fadeExpires) {
		fadeHolder = nil;
		fadeReservation = 0;
	}
	return fadeHolder;
}

kern_return_t
wss_fade_acquire(mach_port_t server, audit_token_t token, double seconds, int32_t *result,
    uint32_t *reservation)
{
	WSConnection *c = [WSConnection connectionForToken:token];

	*reservation = 0;
	if (c == nil)
		*result = kSLSErrorInvalidConnection;
	else if (!isfinite(seconds) || seconds <= 0 || seconds > kSLSMaxFadeReservation)
		*result = kSLSErrorIllegalArgument;
	else if (currentFadeHolder() != nil)
		*result = kSLSErrorNoneAvailable;
	else {
		fadeHolder = c;
		fadeReservation = nextFadeReservation++;
		fadeExpires = [NSDate timeIntervalSinceReferenceDate] + seconds;
		*reservation = fadeReservation;
		*result = kSLSErrorSuccess;
	}
	return KERN_SUCCESS;
}

/* The caller's reservation, still valid, or why not. */
static int
checkFadeReservation(audit_token_t token, uint32_t reservation)
{
	WSConnection *c = [WSConnection connectionForToken:token];

	if (c == nil)
		return kSLSErrorInvalidConnection;
	if (reservation == 0 || reservation != fadeReservation || currentFadeHolder() != c)
		return kSLSErrorIllegalArgument;
	return kSLSErrorSuccess;
}

kern_return_t
wss_fade_release(mach_port_t server, audit_token_t token, uint32_t reservation,
    int32_t *result)
{
	if ((*result = checkFadeReservation(token, reservation)) == kSLSErrorSuccess) {
		fadeHolder = nil;
		fadeReservation = 0;
	}
	return KERN_SUCCESS;
}

static BOOL
unit(float v)
{
	return isfinite(v) && v >= 0 && v <= 1;
}

kern_return_t
wss_fade(mach_port_t server, audit_token_t token, uint32_t reservation, double duration,
    float startBlend, float endBlend, float red, float green, float blue, int32_t *result)
{
	if ((*result = checkFadeReservation(token, reservation)) != kSLSErrorSuccess)
		return KERN_SUCCESS;
	if (!unit(startBlend) || !unit(endBlend) || !unit(red) || !unit(green) || !unit(blue) ||
	    !isfinite(duration) || duration < 0 || duration > kSLSMaxFadeReservation) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	fadeFrom = startBlend;
	fadeStart = [NSDate timeIntervalSinceReferenceDate];
	fadeDuration = duration;
	fadeBlend = endBlend;
	fadedBy = [WSConnection connectionForToken:token];
	fadeColor[0] = red;
	fadeColor[1] = green;
	fadeColor[2] = blue;
	colorsChanged();
	return KERN_SUCCESS;
}

kern_return_t
wss_fade_state(mach_port_t server, float *blend, int32_t *holder)
{
	WSConnection *c = currentFadeHolder();

	*blend = fadeBlend;
	*holder = c ? c.pid : 0;
	return KERN_SUCCESS;
}

/* ---- Display capture ---- */

static int
captureOne(WSDisplay *d, WSConnection *c)
{
	if (d.capturedBy == c)
		return kSLSErrorSuccess;
	if (d.capturedBy != nil)
		return kSLSErrorCannotComplete;
	d.capturedBy = c;
	d.shield = WSWindowCreate(c, kSLSShieldingWindowLevel,
	    (SLSRect){ d.x, d.y, d.width, d.height });
	d.shield.shield = YES;
	WSWindowOrder(d.shield, kSLSOrderAbove, 0);
	return kSLSErrorSuccess;
}

static void
releaseOne(WSDisplay *d)
{
	if (d.shield != nil)
		WSWindowRelease(d.shield);
	d.shield = nil;
	d.capturedBy = nil;
}

kern_return_t
wss_display_capture(mach_port_t server, audit_token_t token, uint32_t display,
    uint32_t options, int32_t *result)
{
	WSConnection *c = [WSConnection connectionForToken:token];

	if (c == nil) {
		*result = kSLSErrorInvalidConnection;
		return KERN_SUCCESS;
	}
	if (display != 0) {
		WSDisplay *d = WSDisplayWithID(display);
		*result = d ? captureOne(d, c) : kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	/* All of them, or none. */
	for (WSDisplay *d in WSDisplays)
		if (d.capturedBy != nil && d.capturedBy != c) {
			*result = kSLSErrorCannotComplete;
			return KERN_SUCCESS;
		}
	for (WSDisplay *d in WSDisplays)
		captureOne(d, c);
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

kern_return_t
wss_display_release(mach_port_t server, audit_token_t token, uint32_t display,
    int32_t *result)
{
	WSConnection *c = [WSConnection connectionForToken:token];

	if (c == nil) {
		*result = kSLSErrorInvalidConnection;
		return KERN_SUCCESS;
	}
	if (display != 0) {
		WSDisplay *d = WSDisplayWithID(display);
		if (d == nil)
			*result = kSLSErrorIllegalArgument;
		else if (d.capturedBy != nil && d.capturedBy != c)
			*result = kSLSErrorCannotComplete;
		else {
			releaseOne(d);
			*result = kSLSErrorSuccess;
		}
		return KERN_SUCCESS;
	}
	for (WSDisplay *d in WSDisplays)
		if (d.capturedBy == c)
			releaseOne(d);
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

kern_return_t
wss_display_capture_state(mach_port_t server, uint32_t display, int32_t *result,
    int32_t *pid, uint32_t *shield)
{
	WSDisplay *d = WSDisplayWithID(display);

	*pid = 0;
	*shield = 0;
	if (d == nil) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	*pid = d.capturedBy ? d.capturedBy.pid : 0;
	*shield = d.shield ? d.shield.windowID : 0;
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

/* ---- Cursor ---- */

/* The cursor: moved by input (WSEvents, on its queue) and by warps (IPC
 * queue): its position, the area drawn last, the shape shown and the box
 * it stays in, under cursorLock. */
static pthread_mutex_t cursorLock = PTHREAD_MUTEX_INITIALIZER;
static double cursorX, cursorY;
static WSCursorShape *shownShapeValue;
static double boxMinX, boxMinY, boxMaxX = 1, boxMaxY = 1;

@implementation WSCursorShape
- (instancetype)initWithPixels:(NSData *)pixels width:(uint32_t)width height:(uint32_t)height
    hotX:(double)hotX hotY:(double)hotY
{
	if ((self = [super init]) != nil) {
		_pixels = pixels;
		_width = width;
		_height = height;
		_hotX = hotX;
		_hotY = hotY;
	}
	return self;
}
@end

/* The area the cursor covers (WSCompositor's arrow is 11 x 17): the one
 * drawn last and the one to draw. With cursorLock held. */
static SLSRect drawnCursorRect;

static void
damageCursorLocked(void)
{
	WSCursorShape *shape = shownShapeValue;
	SLSRect r = shape ? (SLSRect){ cursorX - shape.hotX, cursorY - shape.hotY, shape.width,
	    shape.height } : (SLSRect){ cursorX, cursorY, 11, 17 };

	WSDamageGlobalRect(drawnCursorRect);
	WSDamageGlobalRect(r);
	drawnCursorRect = r;
}

static void
damageCursor(void)
{
	pthread_mutex_lock(&cursorLock);
	damageCursorLocked();
	pthread_mutex_unlock(&cursorLock);
}

/* The front app or its cursor changed (IPC queue): the shape shown is the
 * front app's, or nil for the arrow. */
void
WSCursorShapeChanged(void)
{
	WSCursorShape *shape = WSFrontConnection().cursorShape;

	pthread_mutex_lock(&cursorLock);
	shownShapeValue = shape;
	damageCursorLocked();
	pthread_mutex_unlock(&cursorLock);
}

void
WSCursorSetBounds(double minX, double minY, double maxX, double maxY)
{
	pthread_mutex_lock(&cursorLock);
	boxMinX = minX;
	boxMinY = minY;
	boxMaxX = maxX;
	boxMaxY = maxY;
	pthread_mutex_unlock(&cursorLock);
}

static BOOL
cursorVisible(void)
{
	for (WSConnection *c in [WSConnection allConnections])
		if (c.cursorHideCount > 0)
			return NO;
	return YES;
}

BOOL
WSCursorSnapshot(SLSPoint *position, WSCursorShape **shape)
{
	pthread_mutex_lock(&cursorLock);
	position->x = cursorX;
	position->y = cursorY;
	*shape = shownShapeValue;
	pthread_mutex_unlock(&cursorLock);
	return cursorVisible();
}

kern_return_t
wss_cursor_set_image(mach_port_t server, audit_token_t token, uint32_t width, uint32_t height,
    double hotX, double hotY, ws_pixels_t pixels, mach_msg_type_number_t count, int32_t *result)
{
	WSConnection *c = [WSConnection connectionForToken:token];

	if (c == nil)
		*result = kSLSErrorInvalidConnection;
	else if (width == 0 || height == 0) {
		c.cursorShape = nil;
		*result = kSLSErrorSuccess;
	} else if (width > kSLSMaxCursorSize || height > kSLSMaxCursorSize ||
	    count != width * height * 4 || !(hotX >= 0 && hotX <= width) ||
	    !(hotY >= 0 && hotY <= height))
		*result = kSLSErrorIllegalArgument;
	else {
		c.cursorShape = [[WSCursorShape alloc]
		    initWithPixels:[NSData dataWithBytes:pixels length:count] width:width
		    height:height hotX:hotX hotY:hotY];
		*result = kSLSErrorSuccess;
	}
	if (c != nil && c == WSFrontConnection())
		WSCursorShapeChanged();
	vm_deallocate(mach_task_self(), (vm_address_t)pixels, count);
	return KERN_SUCCESS;
}

kern_return_t
wss_cursor_hide(mach_port_t server, audit_token_t token, uint32_t display, int32_t delta,
    int32_t *result)
{
	WSConnection *c = [WSConnection connectionForToken:token];

	if (c == nil) {
		*result = kSLSErrorInvalidConnection;
		return KERN_SUCCESS;
	}
	if ((display != 0 && WSDisplayWithID(display) == nil) || (delta != 1 && delta != -1)) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	/* Showing more than was hidden does nothing. */
	if (delta > 0 || c.cursorHideCount > 0)
		c.cursorHideCount += delta;
	damageCursor();
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

kern_return_t
wss_cursor_state(mach_port_t server, int32_t *visible, double *x, double *y, int32_t *dx,
    int32_t *dy)
{
	SLSPoint p = WSCursorPosition();

	*visible = cursorVisible();
	*x = p.x;
	*y = p.y;
	*dx = 0;
	*dy = 0;
	return KERN_SUCCESS;
}

SLSPoint
WSCursorPosition(void)
{
	pthread_mutex_lock(&cursorLock);
	SLSPoint p = { cursorX, cursorY };
	pthread_mutex_unlock(&cursorLock);
	return p;
}

/* Any queue: clamped to the box around the displays. */
void
WSCursorMoveTo(double x, double y)
{
	pthread_mutex_lock(&cursorLock);
	x = fmin(fmax(x, boxMinX), boxMaxX - 1);
	y = fmin(fmax(y, boxMinY), boxMaxY - 1);
	if (x != cursorX || y != cursorY) {
		damageCursorLocked();
		cursorX = x;
		cursorY = y;
		damageCursorLocked();
	}
	pthread_mutex_unlock(&cursorLock);
}

kern_return_t
wss_cursor_warp(mach_port_t server, audit_token_t token, double x, double y,
    int32_t *result)
{
	if (!isfinite(x) || !isfinite(y)) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	WSCursorMoveTo(x, y);
	/* The HID system moves on from there. */
	SLSPoint p = WSCursorPosition();
	WSInputSetCursor(p.x, p.y);
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

kern_return_t
wss_cursor_associate(mach_port_t server, audit_token_t token, int32_t connected,
    int32_t *result)
{
	WSConnection *c = [WSConnection connectionForToken:token];

	if (c == nil) {
		*result = kSLSErrorInvalidConnection;
		return KERN_SUCCESS;
	}
	c.cursorAssociated = connected != 0;
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

/* ---- A connection's process exited ---- */

void
WSDisplayServicesConnectionDied(WSConnection *c)
{
	BOOL moved = NO;

	/* Its display streams end. */
	for (WSDisplay *d in WSDisplays)
		[(WSCompositor *)d.compositor removeStreamsOfOwner:c];

	/* Its hide count went with it. */
	if (c.cursorHideCount > 0)
		damageCursor();
	for (WSDisplay *d in WSDisplays)
		if (d.capturedBy == c)
			releaseOne(d);
	/* Its reservation and its fade end with it. */
	if (fadeHolder == c) {
		fadeHolder = nil;
		fadeReservation = 0;
	}
	if (fadedBy == c) {
		fadedBy = nil;
		fadeBlend = 0;
		fadeDuration = 0;
		colorsChanged();
	}
	/* Its app-only changes are undone. */
	for (NSNumber *key in c.appOnlyDisplays) {
		WSDisplay *d = WSDisplayWithID(key.unsignedIntValue);
		NSArray<NSNumber *> *was = c.appOnlyDisplays[key];

		if (d == nil)
			continue;
		[d setModeID:was[0].unsignedIntValue];
		d.x = was[1].doubleValue;
		d.y = was[2].doubleValue;
		d.mirrorOf = was[3].unsignedIntValue;
		moved = YES;
	}
	if (moved)
		displaysMoved();
	/* As at an app's exit on the reference platform: the default transfers. */
	if (c.changedTransfers) {
		for (WSDisplay *d in WSDisplays)
			[d restoreTransfer];
		colorsChanged();
	}
}

/* ---- Display streams ---- */

kern_return_t
wss_display_stream_start(mach_port_t server, audit_token_t token, uint32_t display,
    ws_window_list_t surfaces, mach_msg_type_number_t count,
    SLSDisplayStreamOptions options, mach_port_t notify, int32_t *result, uint32_t *stream)
{
	WSConnection *connection = [WSConnection connectionForToken:token];
	WSDisplay *d = WSDisplayWithID(display);

	*stream = 0;
	if (connection == nil || d == nil || d.compositor == nil) {
		*result = connection == nil ? kSLSErrorInvalidConnection : kSLSErrorIllegalArgument;
		mach_port_deallocate(mach_task_self(), notify);
		return KERN_SUCCESS;
	}
	/* SHORTCUT(M5): other apps' windows show in it, so the built-in policy
	 * of event taps (root or the console's user) decides, as for window
	 * images, until the privacy service's screen recording permission;
	 * taken in WS5. */
	if (!WSMayControlInput(connection)) {
		*result = kSLSErrorCannotComplete;
		mach_port_deallocate(mach_task_self(), notify);
		return KERN_SUCCESS;
	}
	WSDisplayStream *s = [[WSDisplayStream alloc] initWithDisplayID:display surfaces:surfaces
	    count:count options:options notify:notify owner:connection];
	if (s == nil) {
		*result = kSLSErrorIllegalArgument;	/* a surface missing or of another size */
		mach_port_deallocate(mach_task_self(), notify);
		return KERN_SUCCESS;
	}
	[(WSCompositor *)d.compositor addStream:s];
	*stream = s.streamID;
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

kern_return_t
wss_display_stream_stop(mach_port_t server, audit_token_t token, uint32_t stream,
    int32_t *result)
{
	WSConnection *connection = [WSConnection connectionForToken:token];

	*result = kSLSErrorIllegalArgument;
	for (WSDisplay *d in WSDisplays)
		if ([(WSCompositor *)d.compositor removeStreamWithID:stream] != nil)
			*result = kSLSErrorSuccess;
	(void)connection;
	return KERN_SUCCESS;
}
