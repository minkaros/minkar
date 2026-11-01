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
 * The new window server (WindowServer plan, WS1): a daemon that only
 * serves, laid out as on the reference platform, in
 * SkyLight.framework/Resources. Requests come over the MIG interface
 * (IPC/WindowServer.defs) on one serial queue; each connection records the
 * client's PID and user from the request's audit token.
 *
 * Built next to the legacy server, which stays the default until WS4.
 */

#import <Foundation/Foundation.h>
#include <bsm/libbsm.h>
#include <dispatch/dispatch.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include "SkyLight.h"
#include "WindowServerServer.h"	/* MIG server side */
#import "WSDisplay.h"

static int32_t nextConnection = 1;

/* The displays, found at startup; the first is the main display. */
static NSArray<WSDisplay *> *displays;

kern_return_t
wss_connect(mach_port_t server, audit_token_t token, uint32_t *version,
    int32_t *connection)
{
	*version = kSLSInterfaceVersion;
	*connection = nextConnection++;
	fprintf(stderr, "WindowServer: connection %d from pid %d (uid %u)\n",
	    *connection, audit_token_to_pid(token), audit_token_to_euid(token));
	return KERN_SUCCESS;
}

/* ---- Displays ---- */

static WSDisplay *
displayWithID(uint32_t ID)
{
	for (WSDisplay *d in displays)
		if (d.displayID == ID)
			return d;
	return nil;
}

kern_return_t
wss_main_display(mach_port_t server, uint32_t *display)
{
	*display = displays.firstObject.displayID;
	return KERN_SUCCESS;
}

kern_return_t
wss_display_list(mach_port_t server, uint32_t which, ws_display_list_t list,
    mach_msg_type_number_t *count)
{
	uint32_t want = (which == 0) ? kSLSDisplayOnline : kSLSDisplayActive;
	mach_msg_type_number_t n = 0;

	for (WSDisplay *d in displays)
		if ((d.flags & want) && n < 32)
			list[n++] = d.displayID;
	*count = n;
	return KERN_SUCCESS;
}

kern_return_t
wss_display_info(mach_port_t server, uint32_t display, int32_t *result,
    SLSDisplayInfo *info)
{
	WSDisplay *d = displayWithID(display);

	memset(info, 0, sizeof(*info));
	if (d == nil) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	[d getInfo:info];
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

int
main(int argc, char *argv[])
{
	mach_port_t service = MACH_PORT_NULL;
	kern_return_t kr;

	setvbuf(stderr, NULL, _IOLBF, 0);
	kr = bootstrap_check_in(bootstrap_port, kSLSServiceName, &service);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "WindowServer: cannot check in %s: 0x%x\n", kSLSServiceName, kr);
		return 1;
	}

	displays = [WSDisplay availableDisplays];
	for (WSDisplay *d in displays)
		fprintf(stderr, "WindowServer: display 0x%x %ux%u (%.0fx%.0f mm)%s\n",
		    d.displayID, d.width, d.height, d.widthMM, d.heightMM,
		    (d.flags & kSLSDisplayMain) ? " main" : "");

	/* The IPC queue: requests are served in order, one at a time. */
	dispatch_queue_t ipc = dispatch_queue_create("org.minkar.WindowServer.ipc",
	    DISPATCH_QUEUE_SERIAL);
	dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
	    service, 0, ipc);
	dispatch_source_set_event_handler(source, ^{
		mach_msg_server_once(WindowServer_server, wss_WindowServer_subsystem.maxsize,
		    service, MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) |
		    MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0));
	});
	dispatch_resume(source);
	fprintf(stderr, "WindowServer: serving %s (interface version %d)\n",
	    kSLSServiceName, kSLSInterfaceVersion);
	dispatch_main();
}
