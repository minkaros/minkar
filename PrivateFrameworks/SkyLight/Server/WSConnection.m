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

#include <mach/mach.h>
#include <mach/notify.h>
#include <stdio.h>
#import "WSConnection.h"

static NSMutableArray<WSConnection *> *connections;
static mach_port_t notifyPort = MACH_PORT_NULL;
/* Kept for the server's life: under ARC a dispatch source is an object, and
 * the last reference going away cancels it. */
static dispatch_source_t notifySource;
static SLSConnectionID nextConnectionID = 1;

@implementation WSConnection {
	int _pidVersion;
}

+ (void)startOnQueue:(dispatch_queue_t)queue
{
	connections = [NSMutableArray array];
	if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &notifyPort) !=
	    KERN_SUCCESS) {
		fprintf(stderr, "WindowServer: no notification port; clients are never released\n");
		return;
	}
	notifySource = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, notifyPort, 0,
	    queue);
	dispatch_source_set_event_handler(notifySource, ^{
		union {
			mach_dead_name_notification_t dead;
			uint8_t bytes[256];
		} msg;

		memset(&msg, 0, sizeof(msg));
		if (mach_msg(&msg.dead.not_header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0,
		    sizeof(msg), notifyPort, 0, MACH_PORT_NULL) != MACH_MSG_SUCCESS)
			return;
		if (msg.dead.not_header.msgh_id == MACH_NOTIFY_DEAD_NAME)
			[self portDied:msg.dead.not_port];
	});
	dispatch_resume(notifySource);
}

+ (void)portDied:(mach_port_t)name
{
	WSConnection *dead = nil;

	for (WSConnection *c in connections)
		if (c.port == name)
			dead = c;
	/* One reference from the notification, one from our send right. */
	mach_port_deallocate(mach_task_self(), name);
	if (dead == nil)
		return;
	mach_port_deallocate(mach_task_self(), name);
	[connections removeObject:dead];

	unsigned windows = WSWindowsConnectionDied(dead);
	WSDisplayServicesConnectionDied(dead);
	fprintf(stderr, "WindowServer: connection %d (pid %d) closed, %u window(s) released\n",
	    dead.connectionID, dead.pid, windows);
}

+ (WSConnection *)connectionForPort:(mach_port_t)port token:(audit_token_t)token
{
	for (WSConnection *c in connections)
		if (c.port == port) {
			/* The same client again: drop the extra reference. */
			mach_port_deallocate(mach_task_self(), port);
			return c;
		}

	WSConnection *c = [[WSConnection alloc] init];
	c->_connectionID = nextConnectionID++;
	c->_pid = audit_token_to_pid(token);
	c->_pidVersion = audit_token_to_pidversion(token);
	c->_uid = audit_token_to_euid(token);
	c->_port = port;
	c->_cursorAssociated = YES;
	c->_appOnlyDisplays = [NSMutableDictionary dictionary];
	c->_eventQueue = [NSMutableData data];

	mach_port_t previous = MACH_PORT_NULL;
	/* sync 1: told at once if the client is already gone. */
	if (mach_port_request_notification(mach_task_self(), port, MACH_NOTIFY_DEAD_NAME, 1,
	    notifyPort, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous) != KERN_SUCCESS)
		fprintf(stderr, "WindowServer: connection %d: no dead-name notification\n",
		    c.connectionID);
	if (previous != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), previous);
	[connections addObject:c];
	return c;
}

+ (WSConnection *)connectionForToken:(audit_token_t)token
{
	pid_t pid = audit_token_to_pid(token);
	int version = audit_token_to_pidversion(token);

	for (WSConnection *c in [connections reverseObjectEnumerator])
		if (c.pid == pid && c->_pidVersion == version)
			return c;
	return nil;
}

+ (NSArray<WSConnection *> *)allConnections
{
	return [connections copy];
}

@end
