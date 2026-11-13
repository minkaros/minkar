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

/* The CGDirectDisplay API over the window server's interface. */

#include <string.h>
#include "SkyLight.h"
#include "WindowServer.h"	/* MIG user side */

SLSDisplayID
SLSMainDisplayID(void)
{
	uint32_t display = 0;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL || ws_main_display(server, &display) != KERN_SUCCESS)
		return 0;
	return display;
}

static int
displayList(uint32_t which, uint32_t max, SLSDisplayID *displays, uint32_t *count)
{
	uint32_t list[32];
	mach_msg_type_number_t n = 32;
	mach_port_t server = SLSServerPort();

	if (count == NULL)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL || ws_display_list(server, which, list, &n) != KERN_SUCCESS)
		return kSLSErrorFailure;
	/* With no array, only the count (as CGGetActiveDisplayList does). */
	if (displays == NULL) {
		*count = n;
		return kSLSErrorSuccess;
	}
	if (n > max)
		n = max;
	memcpy(displays, list, n * sizeof(SLSDisplayID));
	*count = n;
	return kSLSErrorSuccess;
}

int
SLSGetOnlineDisplayList(uint32_t max, SLSDisplayID *displays, uint32_t *count)
{
	return displayList(0, max, displays, count);
}

int
SLSGetActiveDisplayList(uint32_t max, SLSDisplayID *displays, uint32_t *count)
{
	return displayList(1, max, displays, count);
}

int
SLSGetDisplayInfo(SLSDisplayID display, SLSDisplayInfo *info)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (info == NULL)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL ||
	    ws_display_info(server, display, &result, info) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSGetDisplayMenuBarHeight(SLSDisplayID display, uint32_t *height)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (height == NULL)
		return kSLSErrorIllegalArgument;
	if (server == MACH_PORT_NULL ||
	    ws_display_menu_bar_height(server, display, &result, height) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSSetDisplayDockRect(SLSDisplayID display, SLSIntRect rect)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_display_set_dock_rect(server, display, rect, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

int
SLSGetDisplayDockRect(SLSDisplayID display, SLSIntRect *rect)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (rect == NULL)
		return kSLSErrorIllegalArgument;
	memset(rect, 0, sizeof(*rect));
	if (server == MACH_PORT_NULL ||
	    ws_display_dock_rect(server, display, rect, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	return result;
}

/*
 * The online displays whose bounds contain the point, or meet the rect
 * (bounds include their origin, not their far edges).
 */
static int
displaysMeeting(double x, double y, double width, double height, int isPoint,
    uint32_t max, SLSDisplayID *displays, uint32_t *count)
{
	SLSDisplayID online[32];
	uint32_t n = 0, found = 0;
	int err;

	if (count == NULL)
		return kSLSErrorIllegalArgument;
	if ((err = SLSGetOnlineDisplayList(32, online, &n)) != kSLSErrorSuccess)
		return err;
	for (uint32_t i = 0; i < n; i++) {
		SLSDisplayInfo d;
		int meets;

		if (SLSGetDisplayInfo(online[i], &d) != kSLSErrorSuccess)
			continue;
		if (isPoint)
			meets = x >= d.x && x < d.x + d.width && y >= d.y && y < d.y + d.height;
		else
			meets = x < d.x + d.width && x + width > d.x &&
			    y < d.y + d.height && y + height > d.y;
		if (!meets)
			continue;
		if (displays != NULL && found < max)
			displays[found] = online[i];
		found++;
	}
	*count = (displays != NULL && found > max) ? max : found;
	return kSLSErrorSuccess;
}

int
SLSGetDisplaysWithPoint(double x, double y, uint32_t max, SLSDisplayID *displays,
    uint32_t *count)
{
	return displaysMeeting(x, y, 0, 0, 1, max, displays, count);
}

int
SLSGetDisplaysWithRect(double x, double y, double width, double height,
    uint32_t max, SLSDisplayID *displays, uint32_t *count)
{
	return displaysMeeting(x, y, width, height, 0, max, displays, count);
}
