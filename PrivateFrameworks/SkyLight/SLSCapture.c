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

/* Display fades, capture and the cursor over the window server's interface. */

#include <math.h>
#include <unistd.h>
#include "SkyLight.h"
#include "WindowServer.h"	/* MIG user side */

int
SLSAcquireFadeReservation(double seconds, SLSFadeReservation *reservation)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (reservation == NULL)
		return kSLSErrorIllegalArgument;
	*reservation = 0;
	if (server == MACH_PORT_NULL ||
	    ws_fade_acquire(server, seconds, &result, reservation) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSReleaseFadeReservation(SLSFadeReservation reservation)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_fade_release(server, reservation, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSFadeDisplays(SLSFadeReservation reservation, double duration, float startBlend,
    float endBlend, float red, float green, float blue, int synchronous)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (!isfinite(duration) || duration < 0 || duration > kSLSMaxFadeReservation)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL || ws_fade(server, reservation, startBlend, endBlend,
	    red, green, blue, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	if (result == kSLSErrorSuccess && synchronous && duration > 0)
		usleep((useconds_t)(duration * 1e6));
	return result;
}

int
SLSGetFadeState(float *blend, int32_t *holder)
{
	mach_port_t server = SLSServerPort();

	if (blend == NULL || holder == NULL)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL || ws_fade_state(server, blend, holder) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return kSLSErrorSuccess;
}

int
SLSCaptureDisplay(SLSDisplayID display, uint32_t options)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_display_capture(server, display, options, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSReleaseDisplay(SLSDisplayID display)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_display_release(server, display, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

static int
captureState(SLSDisplayID display, int32_t *pid, uint32_t *shield)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_display_capture_state(server, display, &result, pid, shield) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSDisplayCapturedBy(SLSDisplayID display, int32_t *pid)
{
	uint32_t shield = 0;

	if (pid == NULL)
		return kSLSErrorIllegalArgument;
	return captureState(display, pid, &shield);
}

uint32_t
SLSShieldingWindowID(SLSDisplayID display)
{
	int32_t pid = 0;
	uint32_t shield = 0;

	if (captureState(display, &pid, &shield) != kSLSErrorSuccess)
		return 0;
	return shield;
}

static int
hide(SLSDisplayID display, int32_t delta)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_cursor_hide(server, display, delta, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSHideCursor(SLSDisplayID display)
{
	return hide(display, 1);
}

int
SLSShowCursor(SLSDisplayID display)
{
	return hide(display, -1);
}

static int
cursorState(int32_t *visible, SLSPoint *point, int32_t *dx, int32_t *dy)
{
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL || ws_cursor_state(server, visible, &point->x, &point->y,
	    dx, dy) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return kSLSErrorSuccess;
}

int
SLSCursorIsVisible(int *visible)
{
	int32_t v = 0, dx, dy;
	SLSPoint p;
	int err;

	if (visible == NULL)
		return kSLSErrorIllegalArgument;
	if ((err = cursorState(&v, &p, &dx, &dy)) == kSLSErrorSuccess)
		*visible = v;
	return err;
}

int
SLSGetCursorPosition(SLSPoint *point)
{
	int32_t v, dx, dy;

	if (point == NULL)
		return kSLSErrorIllegalArgument;
	return cursorState(&v, point, &dx, &dy);
}

int
SLSGetLastMouseDelta(int32_t *dx, int32_t *dy)
{
	int32_t v, x, y;
	SLSPoint p;
	int err;

	if (dx == NULL || dy == NULL)
		return kSLSErrorIllegalArgument;
	if ((err = cursorState(&v, &p, &x, &y)) == kSLSErrorSuccess) {
		*dx = x;
		*dy = y;
	}
	return err;
}

int
SLSWarpCursorPosition(SLSPoint point)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_cursor_warp(server, point.x, point.y, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSAssociateMouseAndCursorPosition(int connected)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_cursor_associate(server, connected != 0, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}
