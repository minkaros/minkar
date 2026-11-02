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
 * lssession-test: checks launchservicesd (LaunchServices' session) end to
 * end, run as root. This process and a child check in as two apps; the
 * test checks the list, the launch, activate, quit, front-app and terminate
 * messages, and that only root or the window server's user may say which
 * app is in front. Prints "lssession-test: PASS" at the end.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mach/mach.h>
#include <LaunchServices/LSSession.h>

static int failures;

static void
check(int ok, const char *what)
{
	printf("lssession-test: %s: %s\n", ok ? "ok" : "FAIL", what);
	if (!ok)
		failures++;
}

static void
describe(LSSessionApp *app, const char *bundleID, const char *name)
{
	memset(app, 0, sizeof(*app));
	strlcpy(app->bundleIdentifier, bundleID, sizeof(app->bundleIdentifier));
	strlcpy(app->name, name, sizeof(app->name));
	strlcpy(app->path, "/usr/local/bin/lssession-test", sizeof(app->path));
	app->activationPolicy = 0;	/* regular */
}

/* The next message on port, within ms; its kind, or 0. */
static uint32_t
receive(mach_port_t port, int ms, LSSessionMessage *out)
{
	union {
		LSSessionMessage msg;
		uint8_t bytes[sizeof(LSSessionMessage) + MAX_TRAILER_SIZE];
	} buf;

	if (mach_msg(&buf.msg.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(buf), port,
	    ms, MACH_PORT_NULL) != MACH_MSG_SUCCESS)
		return 0;
	if (buf.msg.header.msgh_id != kLSSessionMessageID)
		return 0;
	if (out != NULL)
		*out = buf.msg;
	return buf.msg.kind;
}

/* Waits for a message of one kind about pid, skipping others. */
static int
expect(mach_port_t port, uint32_t kind, pid_t pid, LSSessionMessage *out)
{
	LSSessionMessage msg;
	uint32_t k;

	while ((k = receive(port, 5000, &msg)) != 0)
		if (k == kind && msg.app.pid == pid) {
			if (out != NULL)
				*out = msg;
			return 1;
		}
	return 0;
}

/* The child: an app that waits to be asked to activate itself, then to
 * quit. */
static int
childApp(int ready)
{
	LSSessionApp app;
	LSSessionMessage msg;
	mach_port_t port = MACH_PORT_NULL;
	uint32_t serial = 0;

	describe(&app, "org.minkar.lssession-test.child", "Child");
	if (LSSessionCheckIn(&app, &port, &serial) != kLSSessionSuccess)
		return 2;
	write(ready, "r", 1);
	if (!expect(port, kLSSessionActivateRequest, getpid(), &msg))
		return 3;
	if (msg.options != 3)
		return 4;
	if (!expect(port, kLSSessionQuitRequest, getpid(), &msg))
		return 5;
	return 0;
}

int
main(void)
{
	LSSessionApp app, *apps = NULL;
	LSSessionMessage msg;
	mach_port_t port = MACH_PORT_NULL;
	uint32_t serial = 0, count = 0;
	int pipes[2], status, found = 0;
	pid_t child, other;
	char c;

	describe(&app, "org.minkar.lssession-test", "LSSession Test");
	check(LSSessionCheckIn(&app, &port, &serial) == kLSSessionSuccess && serial > 0,
	    "check in");
	check(LSSessionCopyApps(&apps, &count) == kLSSessionSuccess, "copy apps");
	for (uint32_t i = 0; i < count; i++)
		if (apps[i].pid == getpid() && apps[i].serial == serial &&
		    strcmp(apps[i].bundleIdentifier, "org.minkar.lssession-test") == 0 &&
		    apps[i].launchTime > 0)
			found = 1;
	free(apps);
	check(found, "the list has this app");
	/* Checking in again keeps the serial. */
	uint32_t again = 0;
	check(LSSessionCheckIn(&app, &port, &again) == kLSSessionSuccess && again == serial,
	    "check in again keeps the serial");

	/* A second app. */
	pipe(pipes);
	child = fork();
	if (child == 0) {
		close(pipes[0]);
		_exit(childApp(pipes[1]));
	}
	close(pipes[1]);
	check(read(pipes[0], &c, 1) == 1, "the child checked in");
	check(expect(port, kLSSessionAppLaunched, child, &msg) &&
	    strcmp(msg.app.bundleIdentifier, "org.minkar.lssession-test.child") == 0,
	    "launched message");

	/* Front app: root may say. */
	check(LSSessionSetFront(child) == kLSSessionSuccess, "set front");
	check(expect(port, kLSSessionAppActivated, child, &msg) &&
	    (msg.app.flags & kLSSessionAppActive), "activated message");
	check(LSSessionCopyApps(&apps, &count) == kLSSessionSuccess && count >= 2 &&
	    apps[0].pid == child, "the front app is listed first");
	free(apps);

	/* Others may not. */
	other = fork();
	if (other == 0) {
		if (setuid(-2) != 0)
			_exit(2);
		_exit(LSSessionSetFront(getppid()) == kLSSessionNotAllowed ? 0 : 1);
	}
	waitpid(other, &status, 0);
	check(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	    "set front refused to another user");

	check(LSSessionActivate(getpid() + 100000, 0) == kLSSessionNoSuchApp,
	    "activate an unknown app");
	check(LSSessionActivate(child, 3) == kLSSessionSuccess, "activate");

	/* Quit: another user may not ask a root app; root may. */
	other = fork();
	if (other == 0) {
		if (setuid(-2) != 0)
			_exit(2);
		_exit(LSSessionQuit(child) == kLSSessionNotAllowed ? 0 : 1);
	}
	waitpid(other, &status, 0);
	check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "quit refused to another user");
	check(LSSessionQuit(getpid() + 100000) == kLSSessionNoSuchApp, "quit an unknown app");
	check(LSSessionQuit(child) == kLSSessionSuccess, "quit");
	waitpid(child, &status, 0);
	check(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	    "the child was asked to activate, then to quit");
	check(expect(port, kLSSessionAppTerminated, child, NULL), "terminated message");
	check(LSSessionCopyApps(&apps, &count) == kLSSessionSuccess, "copy apps after");
	found = 0;
	for (uint32_t i = 0; i < count; i++)
		if (apps[i].pid == child)
			found = 1;
	free(apps);
	check(!found, "the child left the list");

	printf("lssession-test: %s\n", failures ? "FAIL" : "PASS");
	return failures ? 1 : 0;
}
