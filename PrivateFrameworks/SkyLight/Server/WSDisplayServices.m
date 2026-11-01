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
#include <stdio.h>
#import "WSServer.h"
#import "WSConnection.h"
#import "WSWindowList.h"
#import "WSCompositor.h"
#include "WindowServerServer.h"	/* MIG server side */

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

/* Keeps the main display at the global origin, the others where they were
 * relative to it, and the shielding windows over their displays. */
static void
displaysMoved(void)
{
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
	// SHORTCUT(WS5): a permanent change lasts for the session and is not
	// saved for the next one, until real mode changes (virtio-gpu) store
	// the configuration; taken in WS1.
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
	if (*result == kSLSErrorSuccess)
		[WSConnection connectionForToken:token].changedTransfers = YES;
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
	if (*result == kSLSErrorSuccess)
		[WSConnection connectionForToken:token].changedTransfers = YES;
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
	return KERN_SUCCESS;
}

/* ---- Display fades ---- */

/* SHORTCUT(WS4a): the blend is only kept and returned here; nothing fades
 * the screen yet (the compositor applies it in WS4a); taken in WS1. */
static SLSFadeReservation fadeReservation, nextFadeReservation = 1;
static __weak WSConnection *fadeHolder;
static __weak WSConnection *fadedBy;	/* who set the current blend */
static NSTimeInterval fadeExpires;
static float fadeBlend, fadeColor[3];

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
wss_fade(mach_port_t server, audit_token_t token, uint32_t reservation, float startBlend,
    float endBlend, float red, float green, float blue, int32_t *result)
{
	if ((*result = checkFadeReservation(token, reservation)) != kSLSErrorSuccess)
		return KERN_SUCCESS;
	if (!unit(startBlend) || !unit(endBlend) || !unit(red) || !unit(green) || !unit(blue)) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	fadeBlend = endBlend;
	fadedBy = [WSConnection connectionForToken:token];
	fadeColor[0] = red;
	fadeColor[1] = green;
	fadeColor[2] = blue;
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

/* The cursor: moved by input (WSEvents) and by warps. */
static double cursorX, cursorY;

/* The area the cursor image covers (WSCompositor's arrow, 11 x 17). */
static void
damageCursor(void)
{
	WSDamageGlobalRect((SLSRect){ cursorX, cursorY, 11, 17 });
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
WSCursorSnapshot(SLSPoint *position)
{
	position->x = cursorX;
	position->y = cursorY;
	return cursorVisible();
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
	*visible = cursorVisible();
	*x = cursorX;
	*y = cursorY;
	*dx = 0;
	*dy = 0;
	return KERN_SUCCESS;
}

SLSPoint
WSCursorPosition(void)
{
	return (SLSPoint){ cursorX, cursorY };
}

/* The box around the displays, where the cursor stays. */
static void
displaysBox(double *minX, double *minY, double *maxX, double *maxY)
{
	BOOL first = YES;

	*minX = *minY = *maxX = *maxY = 0;
	for (WSDisplay *d in WSDisplays) {
		if (first || d.x < *minX) *minX = d.x;
		if (first || d.y < *minY) *minY = d.y;
		if (first || d.x + d.width > *maxX) *maxX = d.x + d.width;
		if (first || d.y + d.height > *maxY) *maxY = d.y + d.height;
		first = NO;
	}
}

void
WSCursorMoveTo(double x, double y)
{
	double minX, minY, maxX, maxY;

	displaysBox(&minX, &minY, &maxX, &maxY);
	x = fmin(fmax(x, minX), maxX - 1);
	y = fmin(fmax(y, minY), maxY - 1);
	if (x == cursorX && y == cursorY)
		return;
	damageCursor();
	cursorX = x;
	cursorY = y;
	damageCursor();
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
	WSInputSetCursor(cursorX, cursorY);
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
	if (c.changedTransfers)
		for (WSDisplay *d in WSDisplays)
			[d restoreTransfer];
}
