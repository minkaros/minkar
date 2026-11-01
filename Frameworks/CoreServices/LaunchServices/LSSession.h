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
 * LaunchServices' session (WindowServer plan, WS4a): the client side of
 * launchservicesd, which keeps the list of running applications. Each app
 * checks in from NSApplication with a port of its own; the daemon tells
 * every app on that port when an app is launched, terminates or becomes
 * the front one, and asks an app to activate itself. The window server
 * tells the daemon which app is in front. NSRunningApplication and
 * NSWorkspace's notifications are built on this.
 *
 * One session for now, the console's (launchservicesd runs as root);
 * sessions per user come with the login window (Phase 5).
 */

#ifndef LAUNCHSERVICES_LSSESSION_H
#define LAUNCHSERVICES_LSSESSION_H

#include <mach/mach.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kLSSessionServiceName		"org.minkar.coreservices.launchservicesd"
#define kLSSessionInterfaceVersion	1

/* An application, as the daemon keeps it. */
typedef struct LSSessionApp {
	int32_t		pid;
	uint32_t	serial;			/* given at check-in, never reused */
	uint64_t	launchTime;		/* seconds since 1970 */
	uint32_t	flags;			/* kLSSessionAppActive */
	int32_t		activationPolicy;	/* NSApplicationActivationPolicy */
	char		bundleIdentifier[128];
	char		name[64];
	char		path[512];		/* the bundle, or the executable */
} LSSessionApp;

#define kLSSessionAppWords	182
#ifndef __cplusplus
_Static_assert(sizeof(LSSessionApp) == kLSSessionAppWords * 4, "LSSessionApp size");
#endif

enum {
	kLSSessionAppActive = 1 << 0,
};

#define kLSSessionMaxApps	64

/* What the daemon sends to an app's port: a header, then this. */
enum {
	kLSSessionAppLaunched		= 1,	/* another app checked in */
	kLSSessionAppTerminated		= 2,
	kLSSessionAppActivated		= 3,	/* it became the front app */
	kLSSessionAppDeactivated	= 4,
	kLSSessionActivateRequest	= 5,	/* to the app itself: activate */
};

#define kLSSessionMessageID	0x4c534e54	/* 'LSNT' */

typedef struct LSSessionMessage {
	mach_msg_header_t	header;
	uint32_t		kind;
	uint32_t		options;		/* activate request: its options */
	LSSessionApp		app;
} LSSessionMessage;

enum {
	kLSSessionSuccess		= 0,
	kLSSessionNoService		= -1,
	kLSSessionNotAllowed		= -2,
	kLSSessionNoSuchApp		= -3,
	kLSSessionIllegalArgument	= -4,
};

/*
 * Checks the calling process in as an application, described by app (its
 * pid and serial are filled in by the daemon). *port gets a receive right
 * the daemon's messages arrive on. Checking in again updates the record.
 */
int LSSessionCheckIn(const LSSessionApp *app, mach_port_t *port, uint32_t *serial);
/* The running applications, front one first; free(*apps) when done. */
int LSSessionCopyApps(LSSessionApp **apps, uint32_t *count);
/* The window server tells the daemon which app is in front (0: none). */
int LSSessionSetFront(pid_t pid);
/* Asks an application to activate itself (its options are
 * NSApplicationActivationOptions). */
int LSSessionActivate(pid_t pid, uint32_t options);

#ifdef __cplusplus
}
#endif

#endif
