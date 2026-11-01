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

/* ---- Display modes and configuration ---- */

kern_return_t
wss_display_modes(mach_port_t server, uint32_t display, int32_t *result,
    ws_display_mode_list_t modes, mach_msg_type_number_t *count)
{
	WSDisplay *d = displayWithID(display);

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
	WSDisplay *d = displayWithID(display);
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
 * relative to it. */
static void
moveMainToOrigin(void)
{
	WSDisplay *first = displays.firstObject;
	double dx = first.x, dy = first.y;

	for (WSDisplay *d in displays) {
		d.x -= dx;
		d.y -= dy;
	}
}

kern_return_t
wss_display_configure(mach_port_t server, audit_token_t token, uint32_t option,
    ws_display_config_list_t ops, mach_msg_type_number_t count, int32_t *result,
    int32_t *failed)
{
	*failed = -1;
	if (option > kSLSConfigurePermanently) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	/* Every change is checked before any is applied. */
	for (mach_msg_type_number_t i = 0; i < count; i++) {
		WSDisplay *d = displayWithID(ops[i].display);
		BOOL good = d != nil;

		if (good && ops[i].kind == kSLSConfigOpMode)
			good = [d modeWithID:ops[i].modeID] != NULL;
		else if (good && ops[i].kind != kSLSConfigOpOrigin)
			good = NO;
		if (!good) {
			*result = kSLSErrorIllegalArgument;
			*failed = (int32_t)i;
			return KERN_SUCCESS;
		}
	}
	for (mach_msg_type_number_t i = 0; i < count; i++) {
		WSDisplay *d = displayWithID(ops[i].display);

		if (ops[i].kind == kSLSConfigOpMode) {
			int err = [d setModeID:ops[i].modeID];
			if (err != kSLSErrorSuccess) {
				*result = err;
				*failed = (int32_t)i;
				moveMainToOrigin();
				return KERN_SUCCESS;
			}
		} else {
			d.x = ops[i].x;
			d.y = ops[i].y;
		}
	}
	moveMainToOrigin();
	// SHORTCUT(WS1): every option lasts for the session. App-only changes
	// are not undone when the app exits, and permanent ones are not saved.
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
	WSDisplay *d = displayWithID(display);

	if (d == nil || tableCount != 3 * count)
		*result = kSLSErrorIllegalArgument;
	else
		*result = [d setTransferTable:table count:count];
	return KERN_SUCCESS;
}

kern_return_t
wss_display_transfer_table(mach_port_t server, uint32_t display, int32_t *result,
    ws_gamma_table_t table, mach_msg_type_number_t *count)
{
	WSDisplay *d = displayWithID(display);

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
	WSDisplay *d = displayWithID(display);

	*result = d ? [d setTransferFormula:&formula] : kSLSErrorIllegalArgument;
	return KERN_SUCCESS;
}

kern_return_t
wss_display_transfer_formula(mach_port_t server, uint32_t display, int32_t *result,
    SLSGammaFormula *formula)
{
	WSDisplay *d = displayWithID(display);

	memset(formula, 0, sizeof(*formula));
	if (d == nil) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	*formula = d.transferFormula;
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

// SHORTCUT(WS1): transfers a client set are not restored when it exits;
// that needs the client's death notification (connections, later in WS1).
kern_return_t
wss_restore_transfers(mach_port_t server, audit_token_t token)
{
	for (WSDisplay *d in displays)
		[d restoreTransfer];
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
		fprintf(stderr, "WindowServer: display 0x%x %ux%u (%.0fx%.0f mm)%s, %u mode(s)\n",
		    d.displayID, d.width, d.height, d.widthMM, d.heightMM,
		    (d.flags & kSLSDisplayMain) ? " main" : "", d.modeCount);

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
