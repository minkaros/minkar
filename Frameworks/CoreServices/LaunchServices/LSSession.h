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
 * Since interface version 3 the daemon also keeps the applications
 * installed (in /System/Applications, /Applications and its Utilities,
 * /System/Library/CoreServices, and any registered) with the document
 * types they declare, says which applications open a file, and hands
 * documents to an application: at once when it runs, else when it checks
 * in after its launch. LSOpenCFURLRef and NSWorkspace are built on this.
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
#define kLSSessionInterfaceVersion	3

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
	kLSSessionQuitRequest		= 6,	/* to the app itself: quit, as Quit does */
	kLSSessionOpenRequest		= 7,	/* to the app itself: open documents
						   (an LSSessionOpenMessage) */
};

#define kLSSessionMessageID	0x4c534e54	/* 'LSNT' */

typedef struct LSSessionMessage {
	mach_msg_header_t	header;
	uint32_t		kind;
	uint32_t		options;		/* activate request: its options */
	LSSessionApp		app;
} LSSessionMessage;

/* An open request: the URLs (absolute strings, each ending in a NUL),
 * and whether to open them or to show them (reveal, as a file manager
 * does). */
#define kLSSessionMaxURLBytes	16384

enum {
	kLSSessionOpenReveal		= 1 << 0,
};

typedef struct LSSessionOpenMessage {
	mach_msg_header_t	header;
	uint32_t		kind;			/* kLSSessionOpenRequest */
	uint32_t		flags;			/* kLSSessionOpenReveal */
	uint32_t		length;			/* of urls */
	char			urls[kLSSessionMaxURLBytes];
} LSSessionOpenMessage;

/* A file to find applications for, as the caller sees it (the daemon
 * reads no one's files): its name extension and what it is. */
enum {
	kLSSessionFileIsDirectory	= 1 << 0,
	kLSSessionFileIsText		= 1 << 1,	/* no extension it knows: text */
	kLSSessionFileIsExecutable	= 1 << 2,
};

typedef struct LSSessionFile {
	char		extension[64];		/* lower case, "" for none */
	uint32_t	flags;
	uint32_t	roles;			/* LSRolesMask */
} LSSessionFile;

typedef struct LSSessionPath {
	char		path[1024];
} LSSessionPath;

#define kLSSessionMaxHandlers	16

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
/* Asks an application to quit as its Quit item does (its delegate may
 * refuse). Allowed to root and to the application's user (interface
 * version 2). */
int LSSessionQuit(pid_t pid);

/* The applications that open a file, best first, and its type (a
 * uniform type identifier); free(*apps) when done (interface version 3). */
int LSSessionCopyHandlers(const LSSessionFile *file, LSSessionPath **apps, uint32_t *count,
    char uti[128]);
/* The application with a bundle identifier (the one the daemon prefers
 * when there are more). */
int LSSessionCopyAppForBundleIdentifier(const char *bundleIdentifier, LSSessionPath *app);
/* Reads an application again, or adds one from elsewhere. */
int LSSessionRegister(const char *path);
/* Hands URLs to the application at appPath, of the caller's user: now if
 * it runs (and it is asked to activate), else when it checks in within a
 * minute (the caller launches it). *running says which. */
int LSSessionOpen(const char *appPath, const char *urls, uint32_t length, uint32_t flags,
    int *running);

#ifdef __cplusplus
}
#endif

#endif
