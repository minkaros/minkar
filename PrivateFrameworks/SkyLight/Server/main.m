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
#import "WSServer.h"
#import "WSConnection.h"
#import "WSCompositor.h"
#import "WSInput.h"

dispatch_queue_t WSIPCQueue;

kern_return_t
wss_connect(mach_port_t server, audit_token_t token, mach_port_t client,
    uint32_t *version, int32_t *connection)
{
	WSConnection *c = [WSConnection connectionForPort:client token:token];

	*version = kSLSInterfaceVersion;
	*connection = c.connectionID;
	fprintf(stderr, "WindowServer: connection %d from pid %d (uid %u)\n",
	    c.connectionID, c.pid, c.uid);
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

	WSDisplays = [WSDisplay availableDisplays];
	for (WSDisplay *d in WSDisplays)
		fprintf(stderr, "WindowServer: display 0x%x %ux%u (%.0fx%.0f mm)%s, %u mode(s)\n",
		    d.displayID, d.width, d.height, d.widthMM, d.heightMM,
		    (d.flags & kSLSDisplayMain) ? " main" : "", d.modeCount);

	/* The IPC queue: requests are served in order, one at a time. */
	dispatch_queue_t ipc = dispatch_queue_create("org.minkar.WindowServer.ipc",
	    DISPATCH_QUEUE_SERIAL);
	WSIPCQueue = ipc;

	/* A compositor per display it can draw on; each starts with a frame
	 * that clears its screen. */
	for (WSDisplay *d in WSDisplays) {
		WSCompositor *c = [[WSCompositor alloc] initWithDisplay:d];
		d.compositor = c;
		[c start];
	}
	/* Static: under ARC the source is an object, and ARC may release a local
	 * after its last use even though dispatch_main() never returns. */
	static dispatch_source_t source;
	source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, service, 0, ipc);
	dispatch_source_set_event_handler(source, ^{
		mach_msg_server_once(WindowServer_server, wss_WindowServer_subsystem.maxsize,
		    service, MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) |
		    MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0));
	});
	/* Input, read and routed on the IPC queue: routing needs the window list.
	 * SHORTCUT(WS4a): input runs on the IPC queue, until the plan's own
	 * high-priority input queue, with a model snapshot input can read
	 * without the IPC queue; taken in WS3. */
	static WSInput *input;
	input = [WSInput startOnQueue:ipc];

	/* Before the first request: connect needs the dead-name port. */
	[WSConnection startOnQueue:ipc];
	dispatch_resume(source);
	fprintf(stderr, "WindowServer: serving %s (interface version %d)\n",
	    kSLSServiceName, kSLSInterfaceVersion);
	dispatch_main();
}

kern_return_t
wss_server_stats(mach_port_t server, SLSServerStats *stats)
{
	[WSCompositor getStats:stats];
	return KERN_SUCCESS;
}
