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
 * LaunchServices' session: the client side of launchservicesd (see
 * LSSession.h). Plain C over the MIG interface, so the window server and
 * AppKit can both use it.
 */

#include <errno.h>
#include <pthread.h>
#include <servers/bootstrap.h>
#include <stdlib.h>
#include <string.h>
#include "LSSession.h"
#include "LSSessionUser.h"	/* MIG, generated */

static pthread_mutex_t serverLock = PTHREAD_MUTEX_INITIALIZER;
static mach_port_t server = MACH_PORT_NULL;

/* The daemon's port, looked up once; again after the daemon restarted. */
static mach_port_t
lookUpServer(int again)
{
	mach_port_t port;

	pthread_mutex_lock(&serverLock);
	if (again && server != MACH_PORT_NULL) {
		mach_port_deallocate(mach_task_self(), server);
		server = MACH_PORT_NULL;
	}
	if (server == MACH_PORT_NULL &&
	    bootstrap_look_up(bootstrap_port, kLSSessionServiceName, &server) != KERN_SUCCESS)
		server = MACH_PORT_NULL;
	port = server;
	pthread_mutex_unlock(&serverLock);
	return port;
}

/* Calls one routine, which names the daemon's port s; once more on a new
 * port if the daemon went away. */
#define LSS_CALL(call)							\
	do {								\
		kern_return_t kr_ = KERN_FAILURE;			\
		for (int again_ = 0; again_ < 2; again_++) {		\
			mach_port_t s = lookUpServer(again_);		\
			if (s == MACH_PORT_NULL)			\
				return kLSSessionNoService;		\
			kr_ = (call);					\
			if (kr_ != MACH_SEND_INVALID_DEST &&		\
			    kr_ != MIG_SERVER_DIED)			\
				break;					\
		}							\
		if (kr_ != KERN_SUCCESS)				\
			return kLSSessionNoService;			\
	} while (0)

int
LSSessionCheckIn(const LSSessionApp *app, mach_port_t *port, uint32_t *serial)
{
	int32_t result = kLSSessionSuccess;
	uint32_t given = 0;

	if (app == NULL || port == NULL)
		return kLSSessionIllegalArgument;
	if (*port == MACH_PORT_NULL) {
		mach_port_limits_t limits = { .mpl_qlimit = MACH_PORT_QLIMIT_LARGE };

		if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, port) !=
		    KERN_SUCCESS)
			return kLSSessionNoService;
		/* The daemon never waits for an app: room for a burst. */
		mach_port_set_attributes(mach_task_self(), *port, MACH_PORT_LIMITS_INFO,
		    (mach_port_info_t)&limits, MACH_PORT_LIMITS_INFO_COUNT);
	}
	LSS_CALL(lss_check_in(s, *port, *app, &result, &given));
	if (serial != NULL)
		*serial = given;
	return result;
}

int
LSSessionCopyApps(LSSessionApp **apps, uint32_t *count)
{
	LSSessionApp list[kLSSessionMaxApps];
	mach_msg_type_number_t n = kLSSessionMaxApps;

	if (apps == NULL || count == NULL)
		return kLSSessionIllegalArgument;
	LSS_CALL(lss_copy_apps(s, list, &n));
	*apps = malloc((n ? n : 1) * sizeof(LSSessionApp));
	if (*apps == NULL)
		return kLSSessionNoService;
	memcpy(*apps, list, n * sizeof(LSSessionApp));
	*count = n;
	return kLSSessionSuccess;
}

int
LSSessionSetFront(pid_t pid)
{
	int32_t result = kLSSessionSuccess;

	LSS_CALL(lss_set_front(s, pid, &result));
	return result;
}

int
LSSessionActivate(pid_t pid, uint32_t options)
{
	int32_t result = kLSSessionSuccess;

	LSS_CALL(lss_activate(s, pid, options, &result));
	return result;
}

int
LSSessionQuit(pid_t pid)
{
	int32_t result = kLSSessionSuccess;

	LSS_CALL(lss_quit(s, pid, &result));
	return result;
}

/* -- Interface version 3: the applications installed and opening -- */

int
LSSessionCopyHandlers(const LSSessionFile *file, LSSessionPath **apps, uint32_t *count,
    char uti[128])
{
	LSSessionPath list[kLSSessionMaxHandlers];
	mach_msg_type_number_t n = kLSSessionMaxHandlers;
	ls_uti_t type;
	int32_t result = kLSSessionSuccess;

	if (file == NULL || apps == NULL || count == NULL)
		return kLSSessionIllegalArgument;
	memset(&type, 0, sizeof(type));
	LSS_CALL(lss_copy_handlers(s, *file, &result, &type, list, &n));
	if (uti != NULL) {
		memcpy(uti, type.uti, sizeof(type.uti));
		uti[127] = '\0';
	}
	*apps = malloc((n ? n : 1) * sizeof(LSSessionPath));
	if (*apps == NULL)
		return kLSSessionNoService;
	memcpy(*apps, list, n * sizeof(LSSessionPath));
	for (mach_msg_type_number_t i = 0; i < n; i++)
		(*apps)[i].path[sizeof((*apps)[i].path) - 1] = '\0';
	*count = n;
	return result;
}

int
LSSessionCopyAppForBundleIdentifier(const char *bundleIdentifier, LSSessionPath *app)
{
	LSSessionPath id;
	int32_t result = kLSSessionSuccess;

	if (bundleIdentifier == NULL || app == NULL ||
	    strlen(bundleIdentifier) >= sizeof(id.path))
		return kLSSessionIllegalArgument;
	memset(&id, 0, sizeof(id));
	strlcpy(id.path, bundleIdentifier, sizeof(id.path));
	LSS_CALL(lss_copy_app_for_bundle_id(s, id, &result, app));
	app->path[sizeof(app->path) - 1] = '\0';
	return result;
}

int
LSSessionRegister(const char *path)
{
	LSSessionPath p;
	int32_t result = kLSSessionSuccess;

	if (path == NULL || strlen(path) >= sizeof(p.path))
		return kLSSessionIllegalArgument;
	memset(&p, 0, sizeof(p));
	strlcpy(p.path, path, sizeof(p.path));
	LSS_CALL(lss_register_app(s, p, &result));
	return result;
}

int
LSSessionOpen(const char *appPath, const char *urls, uint32_t length, uint32_t flags,
    int *running)
{
	LSSessionPath app;
	int32_t result = kLSSessionSuccess, isRunning = 0;

	if (appPath == NULL || strlen(appPath) >= sizeof(app.path) ||
	    (urls == NULL && length != 0) || length > kLSSessionMaxURLBytes)
		return kLSSessionIllegalArgument;
	memset(&app, 0, sizeof(app));
	strlcpy(app.path, appPath, sizeof(app.path));
	LSS_CALL(lss_open_urls(s, app, (char *)urls, length, flags, &result, &isRunning));
	if (running != NULL)
		*running = isRunning;
	return result;
}
