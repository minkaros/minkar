/*
 * SPDX-License-Identifier: BSD-2-Clause
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
 * loginwindow, minimal (WindowServer plan, section 10.3; WS4b): it owns the
 * session. At start it logs root in on the console and starts the session's
 * applications; it serves the requests of LoginWindowRequest.h: Log Out
 * asks every application to quit (LaunchServices' session), waits for them
 * and starts the session again; Restart and Shut Down do the same, then
 * call reboot(2). An application that does not quit cancels the request.
 *
 * SHORTCUT(Phase 5): no login window, no user but root, no confirmation of
 * its own (the app asking shows one) and no session for other users: the
 * full login window of Phase 5 (Atlas) replaces this; taken in WS4b.
 *
 *	loginwindow [-n]	-n: try it out, without restarting or
 *				shutting down (for the tests)
 */

#include <bsm/libbsm.h>
#include <errno.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <LaunchServices/LSSession.h>
#include "LoginWindowRequest.h"

extern char **environ;

/* The applications of a session, one path per line (an app bundle or an
 * executable); those not installed are left out. */
#define SESSION_APPS	"/System/Library/CoreServices/loginwindow.app/Contents/Resources/SessionApplications"
/* How long the applications have to quit. */
#define QUIT_TIMEOUT	30

static int dryRun;

static const char *
requestName(uint32_t request)
{
	switch (request) {
	case kLoginWindowLogOut:	return "log out";
	case kLoginWindowRestart:	return "restart";
	case kLoginWindowShutDown:	return "shut down";
	}
	return "?";
}

/* An app bundle's executable: Contents/MacOS/ and its name without .app. */
static void
executableFor(const char *path, char *out, size_t size)
{
	size_t len = strlen(path);
	const char *base = strrchr(path, '/');

	base = base ? base + 1 : path;
	if (len > 4 && strcmp(path + len - 4, ".app") == 0)
		snprintf(out, size, "%s/Contents/MacOS/%.*s", path, (int)strlen(base) - 4, base);
	else
		snprintf(out, size, "%s", path);
}

static void
startSession(void)
{
	FILE *list = fopen(SESSION_APPS, "r");
	char line[1024], exe[1200];

	fprintf(stderr, "loginwindow: session of root on the console\n");
	setenv("HOME", "/var/root", 1);
	setenv("USER", "root", 1);
	setenv("LOGNAME", "root", 1);
	if (list == NULL)
		return;
	while (fgets(line, sizeof(line), list) != NULL) {
		line[strcspn(line, "\r\n")] = '\0';
		if (line[0] == '\0' || line[0] == '#')
			continue;
		executableFor(line, exe, sizeof(exe));
		if (access(exe, X_OK) != 0)
			continue;
		char *argv[] = { exe, NULL };
		pid_t pid;
		int err = posix_spawn(&pid, exe, NULL, NULL, argv, environ);
		if (err != 0)
			fprintf(stderr, "loginwindow: cannot start %s: %s\n", exe, strerror(err));
		else
			fprintf(stderr, "loginwindow: started %s (pid %d)\n", exe, pid);
	}
	fclose(list);
}

/* The applications still running, but this process. */
static uint32_t
runningApps(LSSessionApp **apps)
{
	uint32_t count = 0;

	if (LSSessionCopyApps(apps, &count) != kLSSessionSuccess) {
		*apps = NULL;
		return 0;
	}
	for (uint32_t i = 0; i < count; i++)
		if ((*apps)[i].pid == getpid()) {
			(*apps)[i] = (*apps)[--count];
			break;
		}
	return count;
}

/* Asks every application to quit; YES once all have gone. */
static int
quitApplications(const char *what)
{
	LSSessionApp *apps;
	uint32_t count = runningApps(&apps);

	for (uint32_t i = 0; i < count; i++) {
		fprintf(stderr, "loginwindow: asking pid %d (%s) to quit\n", apps[i].pid,
		    apps[i].bundleIdentifier[0] ? apps[i].bundleIdentifier : apps[i].name);
		LSSessionQuit(apps[i].pid);
	}
	free(apps);
	for (int tick = 0; tick < QUIT_TIMEOUT * 4; tick++) {
		count = runningApps(&apps);
		if (count == 0)
			return 1;
		if (tick == QUIT_TIMEOUT * 4 - 1)
			for (uint32_t i = 0; i < count; i++)
				fprintf(stderr, "loginwindow: pid %d (%s) did not quit: %s cancelled\n",
				    apps[i].pid, apps[i].bundleIdentifier[0] ?
				    apps[i].bundleIdentifier : apps[i].name, what);
		free(apps);
		usleep(250000);
	}
	return 0;
}

static void
serve(uint32_t request)
{
	const char *what = requestName(request);

	fprintf(stderr, "loginwindow: %s requested\n", what);
	if (!quitApplications(what))
		return;
	switch (request) {
	case kLoginWindowLogOut:
		fprintf(stderr, "loginwindow: session ended\n");
		startSession();
		break;
	case kLoginWindowRestart:
	case kLoginWindowShutDown:
		fprintf(stderr, "loginwindow: session ended, %s%s\n",
		    request == kLoginWindowRestart ? "restarting" : "shutting down",
		    dryRun ? " (not: -n)" : "");
		if (dryRun) {
			startSession();
			break;
		}
		sync();
		reboot(request == kLoginWindowRestart ? RB_AUTOBOOT : RB_HALT);
		fprintf(stderr, "loginwindow: reboot: %s\n", strerror(errno));
		break;
	}
}

/* The -textconsole boot argument: no window server, so no session. */
static int
textConsole(void)
{
	char args[1024] = "";
	size_t len = sizeof(args);

	if (sysctlbyname("kern.bootargs", args, &len, NULL, 0) != 0)
		return 0;
	for (char *word = strtok(args, " "); word != NULL; word = strtok(NULL, " "))
		if (strcmp(word, "-textconsole") == 0)
			return 1;
	return 0;
}

/* Root and the console's owner may ask. */
static int
allowed(const audit_token_t *token)
{
	uid_t uid = audit_token_to_euid(*token);
	struct stat st;

	return uid == 0 || (stat("/dev/console", &st) == 0 && st.st_uid == uid);
}

int
main(int argc, char *argv[])
{
	mach_port_t service = MACH_PORT_NULL;
	kern_return_t kr;

	setvbuf(stderr, NULL, _IOLBF, 0);
	if (argc > 1 && strcmp(argv[1], "-n") == 0)
		dryRun = 1;
	if (textConsole()) {
		fprintf(stderr, "loginwindow: -textconsole: no graphical session\n");
		return 0;
	}
	kr = bootstrap_check_in(bootstrap_port, kLoginWindowServiceName, &service);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "loginwindow: cannot check in %s: 0x%x\n", kLoginWindowServiceName, kr);
		return 1;
	}
	fprintf(stderr, "loginwindow: serving %s%s\n", kLoginWindowServiceName,
	    dryRun ? " (trying out: no restart or shut down)" : "");
	startSession();

	for (;;) {
		union {
			LoginWindowRequest req;
			uint8_t bytes[sizeof(LoginWindowRequest) + MAX_TRAILER_SIZE];
		} msg;

		kr = mach_msg(&msg.req.header, MACH_RCV_MSG |
		    MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
		    MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT), 0, sizeof(msg), service,
		    MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
		if (kr != MACH_MSG_SUCCESS) {
			fprintf(stderr, "loginwindow: receive: 0x%x\n", kr);
			continue;
		}
		mach_msg_audit_trailer_t *trailer = (mach_msg_audit_trailer_t *)
		    ((uint8_t *)&msg + round_msg(msg.req.header.msgh_size));
		if (msg.req.header.msgh_id != kLoginWindowRequestID ||
		    msg.req.header.msgh_size < sizeof(LoginWindowRequest)) {
			mach_msg_destroy(&msg.req.header);
			continue;
		}
		if (!allowed(&trailer->msgh_audit)) {
			fprintf(stderr, "loginwindow: %s refused to uid %d\n",
			    requestName(msg.req.request), audit_token_to_euid(trailer->msgh_audit));
			mach_msg_destroy(&msg.req.header);
			continue;
		}
		uint32_t request = msg.req.request;
		mach_msg_destroy(&msg.req.header);
		serve(request);
	}
}
