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

/* Events and the key window over the window server's interface. */

#include <mach/mach.h>
#include <string.h>
#include "SkyLight.h"
#include "WindowServer.h"	/* MIG user side */
#include "WindowServerTypes.h"

/* Takes the "events pending" messages off the port; waits for one first
 * when wait is set. */
static void
takePending(mach_port_t port, int wait)
{
	struct {
		mach_msg_header_t header;
		mach_msg_trailer_t trailer;
	} msg;
	mach_msg_option_t options = MACH_RCV_MSG | (wait ? 0 : MACH_RCV_TIMEOUT);

	while (mach_msg(&msg.header, options, 0, sizeof(msg), port, 0, MACH_PORT_NULL) ==
	    MACH_MSG_SUCCESS) {
		if (msg.header.msgh_bits & MACH_MSGH_BITS_COMPLEX)
			mach_msg_destroy(&msg.header);
		options = MACH_RCV_MSG | MACH_RCV_TIMEOUT;
	}
}

int
SLSReceiveEvents(SLSEvent *events, uint32_t max, uint32_t *count, int wait)
{
	ws_event_list_t list;
	mach_msg_type_number_t n = kSLSMaxEvents;
	mach_port_t server = SLSServerPort(), port = SLSEventPort();

	if (count == NULL || (events == NULL && max != 0))
		return kSLSErrorIllegalArgument;
	*count = 0;
	if (server == MACH_PORT_NULL || port == MACH_PORT_NULL)
		return kSLSErrorFailure;
	takePending(port, wait);
	kern_return_t kr = ws_next_events(server, list, &n);
	if (kr == MACH_SEND_INVALID_DEST || kr == MIG_SERVER_DIED)
		return kSLSErrorInvalidConnection;	/* the server is gone */
	if (kr != KERN_SUCCESS)
		return kSLSErrorFailure;
	if (n > max)
		n = max;	/* the rest are lost: callers pass kSLSMaxEvents */
	memcpy(events, list, n * sizeof(SLSEvent));
	*count = n;
	return kSLSErrorSuccess;
}

int
SLSSetKeyWindow(SLSWindowID window)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_set_key_window(server, window, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSSetFrontProcess(void)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL || ws_set_front_process(server, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSGetFrontProcess(pid_t *pid)
{
	int32_t front = 0;
	mach_port_t server = SLSServerPort();

	if (pid == NULL)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL || ws_front_process(server, &front) != KERN_SUCCESS)
		return kSLSErrorFailure;
	*pid = front;
	return kSLSErrorSuccess;
}

int
SLSCapturePointer(SLSWindowID window)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_capture_pointer(server, window, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

SLSWindowID
SLSGetPointerCapture(void)
{
	uint32_t window = 0;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL || ws_pointer_capture(server, &window) != KERN_SUCCESS)
		return 0;
	return window;
}

SLSWindowID
SLSGetKeyWindow(void)
{
	uint32_t window = 0;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL || ws_key_window(server, &window) != KERN_SUCCESS)
		return 0;
	return window;
}

int
SLSWindowStartMove(SLSWindowID window)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_window_start_move(server, window, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSWindowSetDragRegion(SLSWindowID window, const SLSIntRect *drag, uint32_t dragCount,
    const SLSIntRect *exclude, uint32_t excludeCount)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();
	ws_region_t d, x;

	if (dragCount > kSLSMaxRegionRects || excludeCount > kSLSMaxRegionRects ||
	    (dragCount != 0 && drag == NULL) || (excludeCount != 0 && exclude == NULL))
		return kSLSErrorIllegalArgument;
	if (dragCount)
		memcpy(d, drag, dragCount * sizeof(SLSIntRect));
	if (excludeCount)
		memcpy(x, exclude, excludeCount * sizeof(SLSIntRect));
	if (server == MACH_PORT_NULL ||
	    ws_window_set_drag_region(server, window, d, dragCount, x, excludeCount,
		&result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSClaimSymbolicHotKey(uint32_t hotKey, int claim)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_hotkey_claim(server, hotKey, claim != 0, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSGetSymbolicHotKey(uint32_t hotKey, uint16_t *keyCode, uint32_t *modifiers, int *enabled)
{
	int32_t result = kSLSErrorFailure, on = 0;
	uint32_t code = 0, flags = 0;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_hotkey_get(server, hotKey, &result, &code, &flags, &on) != KERN_SUCCESS)
		return kSLSErrorFailure;
	if (keyCode != NULL)
		*keyCode = (uint16_t)code;
	if (modifiers != NULL)
		*modifiers = flags;
	if (enabled != NULL)
		*enabled = on;
	return result;
}

int
SLSEventTapCreate(uint32_t location, uint32_t placement, uint32_t options, uint64_t mask,
    mach_port_t port, uint32_t *tap)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (tap == NULL || port == MACH_PORT_NULL)
		return kSLSErrorIllegalArgument;
	*tap = 0;
	if (server == MACH_PORT_NULL || ws_tap_create(server, location, placement, options,
	    mask, port, &result, tap) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSEventTapEnable(uint32_t tap, int enable)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_tap_enable(server, tap, enable != 0, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSEventTapRelease(uint32_t tap)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL || ws_tap_release(server, tap, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSGetEventTapList(uint32_t max, SLSEventTapInfo *taps, uint32_t *count)
{
	ws_tap_list_t list;
	mach_msg_type_number_t n = kSLSMaxEventTaps;
	mach_port_t server = SLSServerPort();

	if (count == NULL)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL || ws_tap_list(server, list, &n) != KERN_SUCCESS)
		return kSLSErrorFailure;
	if (taps == NULL) {
		*count = n;
		return kSLSErrorSuccess;
	}
	if (n > max)
		n = max;
	memcpy(taps, list, n * sizeof(SLSEventTapInfo));
	*count = n;
	return kSLSErrorSuccess;
}

int
SLSPostEvent(uint32_t location, const SLSEvent *event)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (event == NULL)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL ||
	    ws_post_event(server, location, *event, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSGetHIDState(uint32_t which, SLSHIDState *state)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (state == NULL)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL || ws_hid_state(server, which, &result, state) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}
