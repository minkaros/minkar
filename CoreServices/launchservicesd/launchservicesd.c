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
 * launchservicesd: LaunchServices' session (WindowServer plan, WS4a). It
 * keeps the running applications: each checks in from NSApplication with
 * a port of its own, and leaves when that port dies. The window server
 * says which one is in front. Every app hears on its port when an app is
 * launched, terminates, or becomes or stops being the front one, and an
 * app asked to activate hears it there too (LaunchServices/LSSession.h).
 *
 * One session, the console's: the daemon runs as root. Sessions per user
 * come with the login window (Phase 5).
 */

#include <bsm/libbsm.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach/notify.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "LSSession.h"
#include "LSSessionServer.h"	/* MIG, generated */

/* The window server's user, and root, may say which app is in front. */
#define WINDOWSERVER_UID	88

typedef struct {
	LSSessionApp	app;
	mach_port_t	port;		/* send right to the app's port */
} Client;

static Client clients[kLSSessionMaxApps];
static unsigned clientCount;
static uint32_t nextSerial = 1;
static pid_t frontPID;
static mach_port_t notifyPort = MACH_PORT_NULL;

static Client *
clientForPID(pid_t pid)
{
	for (unsigned i = 0; i < clientCount; i++)
		if (clients[i].app.pid == pid)
			return &clients[i];
	return NULL;
}

static void
fillFlags(LSSessionApp *app)
{
	if (app->pid == frontPID)
		app->flags |= kLSSessionAppActive;
	else
		app->flags &= ~kLSSessionAppActive;
}

/* Never waits: an app that does not read its port loses messages, it
 * cannot hold up the session. */
static void
sendMessage(Client *to, uint32_t kind, uint32_t options, const LSSessionApp *app)
{
	LSSessionMessage msg;

	memset(&msg, 0, sizeof(msg));
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
	msg.header.msgh_size = sizeof(msg);
	msg.header.msgh_remote_port = to->port;
	msg.header.msgh_id = kLSSessionMessageID;
	msg.kind = kind;
	msg.options = options;
	msg.app = *app;
	fillFlags(&msg.app);
	mach_msg(&msg.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(msg), 0,
	    MACH_PORT_NULL, 0, MACH_PORT_NULL);
}

static void
broadcast(uint32_t kind, const LSSessionApp *app, pid_t except)
{
	for (unsigned i = 0; i < clientCount; i++)
		if (clients[i].app.pid != except)
			sendMessage(&clients[i], kind, 0, app);
}

static void
removeClient(Client *c)
{
	LSSessionApp app = c->app;

	mach_port_deallocate(mach_task_self(), c->port);
	*c = clients[--clientCount];
	if (frontPID == app.pid)
		frontPID = 0;
	fprintf(stderr, "launchservicesd: pid %d (%s) terminated\n", app.pid,
	    app.bundleIdentifier[0] ? app.bundleIdentifier : app.name);
	broadcast(kLSSessionAppTerminated, &app, 0);
}

/* An app's port died: the app is gone. */
static void
portDied(mach_port_t name)
{
	for (unsigned i = 0; i < clientCount; i++)
		if (clients[i].port == name) {
			/* The notification's reference; removeClient drops ours. */
			mach_port_deallocate(mach_task_self(), name);
			removeClient(&clients[i]);
			return;
		}
	mach_port_deallocate(mach_task_self(), name);
}

kern_return_t
lsd_check_in(mach_port_t server, audit_token_t token, mach_port_t port,
    LSSessionApp app, int32_t *result, uint32_t *serial)
{
	pid_t pid = audit_token_to_pid(token);
	Client *c = clientForPID(pid);
	mach_port_t previous = MACH_PORT_NULL;
	int isNew = (c == NULL);

	if (isNew) {
		if (clientCount == kLSSessionMaxApps) {
			mach_port_deallocate(mach_task_self(), port);
			*result = kLSSessionNotAllowed;
			*serial = 0;
			return KERN_SUCCESS;
		}
		c = &clients[clientCount++];
		memset(c, 0, sizeof(*c));
		c->app.serial = nextSerial++;
		c->app.launchTime = (uint64_t)time(NULL);
	} else if (c->port != port) {
		mach_port_deallocate(mach_task_self(), c->port);
	} else {
		/* The same port again: keep one reference. */
		mach_port_deallocate(mach_task_self(), port);
	}
	app.pid = pid;
	app.serial = c->app.serial;
	app.launchTime = c->app.launchTime;
	app.bundleIdentifier[sizeof(app.bundleIdentifier) - 1] = '\0';
	app.name[sizeof(app.name) - 1] = '\0';
	app.path[sizeof(app.path) - 1] = '\0';
	c->app = app;
	fillFlags(&c->app);
	if (c->port != port) {
		c->port = port;
		if (mach_port_request_notification(mach_task_self(), port,
		    MACH_NOTIFY_DEAD_NAME, 1, notifyPort, MACH_MSG_TYPE_MAKE_SEND_ONCE,
		    &previous) != KERN_SUCCESS) {
			/* Already dead: the app is gone. */
			removeClient(c);
			*result = kLSSessionNoSuchApp;
			*serial = 0;
			return KERN_SUCCESS;
		}
		if (previous != MACH_PORT_NULL)
			mach_port_deallocate(mach_task_self(), previous);
	}
	*result = kLSSessionSuccess;
	*serial = c->app.serial;
	if (isNew) {
		fprintf(stderr, "launchservicesd: pid %d (%s) checked in as app %u\n", pid,
		    app.bundleIdentifier[0] ? app.bundleIdentifier : app.name, app.serial);
		broadcast(kLSSessionAppLaunched, &c->app, pid);
		/* It may already be in front: the window server can say so first. */
		if (pid == frontPID)
			broadcast(kLSSessionAppActivated, &c->app, 0);
	}
	return KERN_SUCCESS;
}

kern_return_t
lsd_copy_apps(mach_port_t server, ls_app_list_t apps, mach_msg_type_number_t *count)
{
	unsigned n = 0;
	Client *front = clientForPID(frontPID);

	if (front != NULL)
		apps[n++] = front->app;
	/* Then in check-in order. */
	uint32_t last = 0;
	for (;;) {
		Client *next = NULL;
		for (unsigned i = 0; i < clientCount; i++)
			if (clients[i].app.serial > last && &clients[i] != front &&
			    (next == NULL || clients[i].app.serial < next->app.serial))
				next = &clients[i];
		if (next == NULL)
			break;
		apps[n++] = next->app;
		last = next->app.serial;
	}
	for (unsigned i = 0; i < n; i++)
		fillFlags(&apps[i]);
	*count = n;
	return KERN_SUCCESS;
}

kern_return_t
lsd_set_front(mach_port_t server, audit_token_t token, int32_t pid, int32_t *result)
{
	uid_t uid = audit_token_to_euid(token);
	Client *old, *new;

	if (uid != 0 && uid != WINDOWSERVER_UID) {
		*result = kLSSessionNotAllowed;
		return KERN_SUCCESS;
	}
	*result = kLSSessionSuccess;
	if (pid == frontPID)
		return KERN_SUCCESS;
	old = clientForPID(frontPID);
	new = clientForPID(pid);
	frontPID = pid;
	if (old != NULL)
		broadcast(kLSSessionAppDeactivated, &old->app, 0);
	if (new != NULL)
		broadcast(kLSSessionAppActivated, &new->app, 0);
	return KERN_SUCCESS;
}

kern_return_t
lsd_activate(mach_port_t server, audit_token_t token, int32_t pid, uint32_t options,
    int32_t *result)
{
	Client *c = clientForPID(pid);

	if (c == NULL) {
		*result = kLSSessionNoSuchApp;
		return KERN_SUCCESS;
	}
	sendMessage(c, kLSSessionActivateRequest, options, &c->app);
	*result = kLSSessionSuccess;
	return KERN_SUCCESS;
}

int
main(int argc, char *argv[])
{
	mach_port_t service = MACH_PORT_NULL;
	kern_return_t kr;

	setvbuf(stderr, NULL, _IOLBF, 0);
	kr = bootstrap_check_in(bootstrap_port, kLSSessionServiceName, &service);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "launchservicesd: cannot check in %s: 0x%x\n",
		    kLSSessionServiceName, kr);
		return 1;
	}
	if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &notifyPort) !=
	    KERN_SUCCESS) {
		fprintf(stderr, "launchservicesd: no notification port\n");
		return 1;
	}

	/* One queue: requests and notifications are served in order. */
	dispatch_queue_t queue = dispatch_queue_create("org.minkar.launchservicesd",
	    DISPATCH_QUEUE_SERIAL);
	dispatch_source_t notifySource = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
	    notifyPort, 0, queue);
	dispatch_source_set_event_handler(notifySource, ^{
		union {
			mach_dead_name_notification_t dead;
			uint8_t bytes[sizeof(mach_dead_name_notification_t) + MAX_TRAILER_SIZE];
		} msg;

		while (mach_msg(&msg.dead.not_header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0,
		    sizeof(msg), notifyPort, 0, MACH_PORT_NULL) == MACH_MSG_SUCCESS)
			if (msg.dead.not_header.msgh_id == MACH_NOTIFY_DEAD_NAME)
				portDied(msg.dead.not_port);
	});
	dispatch_resume(notifySource);

	dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
	    service, 0, queue);
	dispatch_source_set_event_handler(source, ^{
		mach_msg_server_once(LSSession_server, lsd_LSSession_subsystem.maxsize, service,
		    MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) |
		    MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0));
	});
	dispatch_resume(source);
	fprintf(stderr, "launchservicesd: serving %s (interface version %d)\n",
	    kLSSessionServiceName, kLSSessionInterfaceVersion);
	dispatch_main();
}
