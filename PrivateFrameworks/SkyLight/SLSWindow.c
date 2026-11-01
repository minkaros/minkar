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

/* Windows and the window list over the window server's interface. */

#include <string.h>
#include "SkyLight.h"
#include "WindowServer.h"	/* MIG user side */

/* SLSSurface.c */
void _SLSForgetSurface(SLSWindowID window);
void _SLSFrameChanged(SLSWindowID window, double width, double height);

int
SLSNewWindow(int32_t level, SLSRect frame, SLSWindowID *window)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (window == NULL)
		return kSLSErrorIllegalArgument;
	*window = 0;
	if (server == MACH_PORT_NULL ||
	    ws_window_create(server, level, frame, &result, window) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSReleaseWindow(SLSWindowID window)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	_SLSForgetSurface(window);
	if (server == MACH_PORT_NULL ||
	    ws_window_release(server, window, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSSetWindowFrame(SLSWindowID window, SLSRect frame)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_window_set_frame(server, window, frame, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	/* A new size gets a new surface (a commit to the old one is refused). */
	if (result == kSLSErrorSuccess)
		_SLSFrameChanged(window, frame.width, frame.height);
	return result;
}

int
SLSSetWindowLevel(SLSWindowID window, int32_t level)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_window_set_level(server, window, level, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSOrderWindow(SLSWindowID window, int32_t place, SLSWindowID relativeTo)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_window_order(server, window, place, relativeTo, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSWindowListCopy(uint32_t option, SLSWindowID relativeTo, uint32_t max,
    SLSWindowID *windows, uint32_t *count)
{
	ws_window_list_t list;
	mach_msg_type_number_t n = kSLSMaxWindowList;
	mach_port_t server = SLSServerPort();

	if (count == NULL)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL ||
	    ws_window_list(server, option, relativeTo, list, &n) != KERN_SUCCESS)
		return kSLSErrorFailure;
	if (windows == NULL) {
		*count = n;
		return kSLSErrorSuccess;
	}
	if (n > max)
		n = max;
	memcpy(windows, list, n * sizeof(SLSWindowID));
	*count = n;
	return kSLSErrorSuccess;
}

int
SLSGetWindowInfo(SLSWindowID window, SLSWindowInfo *info)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (info == NULL)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL ||
	    ws_window_info(server, window, &result, info) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSSetWindowAlpha(SLSWindowID window, double alpha)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_window_set_alpha(server, window, alpha, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSGetServerStats(SLSServerStats *stats)
{
	mach_port_t server = SLSServerPort();

	if (stats == NULL)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL || ws_server_stats(server, stats) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return kSLSErrorSuccess;
}

int
SLSSetWindowStatusItem(SLSWindowID window, int isItem)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_window_set_status_item(server, window, isItem != 0, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}
