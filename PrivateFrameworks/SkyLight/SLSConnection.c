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

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/sysctl.h>
#include <servers/bootstrap.h>
#include "SkyLight.h"
#include "WindowServer.h"	/* MIG user side */

static pthread_once_t connectOnce = PTHREAD_ONCE_INIT;
static mach_port_t serverPort = MACH_PORT_NULL;
static SLSConnectionID mainConnection = 0;
/*
 * A port only this process holds the receive right to: the server keeps a
 * send right and is told when it dies, that is when the process exits, to
 * release what the connection held.
 */
static mach_port_t clientPort = MACH_PORT_NULL;

/* The -textconsole boot argument: no window server at all, so no wait. */
static int
textConsole(void)
{
	char args[1024] = "";
	size_t len = sizeof(args);

	if (sysctlbyname("kern.bootargs", args, &len, NULL, 0) != 0)
		return 0;
	return strstr(args, "-textconsole") != NULL;
}

static void
openMainConnection(void)
{
	uint32_t version = 0;
	int32_t connection = 0;

	/* The server may be starting (launchd restarting it): try for a
	 * while, so an app started then does not run without one. */
	for (int tries = 0; ; tries++) {
		if (bootstrap_look_up(bootstrap_port, kSLSServiceName, &serverPort) == KERN_SUCCESS)
			break;
		serverPort = MACH_PORT_NULL;
		if (tries >= 40 || getenv("SLS_NO_WAIT") != NULL || textConsole())
			return;
		usleep(250000);
	}
	if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &clientPort) !=
	    KERN_SUCCESS)
		return;
	if (ws_connect(serverPort, clientPort, &version, &connection) != KERN_SUCCESS ||
	    version != kSLSInterfaceVersion)
		return;
	mainConnection = connection;
	/* The event port also hears when the server goes away, so a process
	 * waiting for events wakes and SLSReceiveEvents says the connection
	 * is gone. */
	mach_port_t previous = MACH_PORT_NULL;
	if (mach_port_request_notification(mach_task_self(), serverPort, MACH_NOTIFY_DEAD_NAME, 1,
	    clientPort, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous) == KERN_SUCCESS &&
	    previous != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), previous);
	/* launchd keeps the service's port across a restart of the server, so
	 * that port does not die: the server's send right to this port, the
	 * only one, goes with it (no more senders). */
	previous = MACH_PORT_NULL;
	if (mach_port_request_notification(mach_task_self(), clientPort, MACH_NOTIFY_NO_SENDERS, 1,
	    clientPort, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous) == KERN_SUCCESS &&
	    previous != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), previous);
}

SLSConnectionID
SLSMainConnectionID(void)
{
	pthread_once(&connectOnce, openMainConnection);
	return mainConnection;
}

mach_port_t
SLSServerPort(void)
{
	pthread_once(&connectOnce, openMainConnection);
	return serverPort;
}

mach_port_t
SLSEventPort(void)
{
	pthread_once(&connectOnce, openMainConnection);
	return clientPort;
}
