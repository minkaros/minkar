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
	if (ws_next_events(server, list, &n) != KERN_SUCCESS)
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

SLSWindowID
SLSGetKeyWindow(void)
{
	uint32_t window = 0;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL || ws_key_window(server, &window) != KERN_SUCCESS)
		return 0;
	return window;
}
