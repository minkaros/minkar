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
 * caffeinate: keep the machine from sleeping while something runs, as on
 * macOS: power assertions (IOPMAssertionCreateWithName) held while a
 * command runs, until a process exits, for a time, or until interrupted.
 * Where no power manager answers (Minkar has no idle sleep yet), there is
 * nothing to hold off, and caffeinate waits all the same.
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/wait.h>

#include <IOKit/pwr_mgt/IOPMLib.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

#define	MAXASSERT	5

static IOPMAssertionID held[MAXASSERT];
static int nheld;

static void
usage(void)
{

	fprintf(stderr,
	    "usage: caffeinate [-disum] [-t timeout] [-w pid] [utility arguments...]\n");
	exit(1);
}

/* type NULL: declare that the user is active (-u). */
static void
hold(CFStringRef type, CFStringRef why)
{
	IOPMAssertionID id;

	if (type == NULL) {
		if (IOPMAssertionDeclareUserActivity(why,
		    kIOPMUserActiveLocal, &id) == kIOReturnSuccess)
			held[nheld++] = id;
		return;
	}
	if (IOPMAssertionCreateWithName(type, kIOPMAssertionLevelOn, why,
	    &id) == kIOReturnSuccess)
		held[nheld++] = id;
}

static void
release(void)
{

	while (nheld > 0)
		(void)IOPMAssertionRelease(held[--nheld]);
}

static long
number(const char *s, long max)
{
	char *end;
	long n;

	errno = 0;
	n = strtol(s, &end, 10);
	if (*s == '\0' || *end != '\0' || errno != 0 || n <= 0 || n > max)
		usage();
	return (n);
}

int
main(int argc, char *argv[])
{
	CFStringRef why;
	bool d = false, i = false, s = false, u = false, m = false;
	long timeout = 0, waitpid_ = 0;
	struct kevent ev[2];
	int ch, kq, n, status = 0;
	pid_t child = 0;
	char name[64];

	while ((ch = getopt(argc, argv, "dimsut:w:")) != -1)
		switch (ch) {
		case 'd':
			d = true;
			break;
		case 'i':
			i = true;
			break;
		case 'm':
			m = true;
			break;
		case 's':
			s = true;
			break;
		case 'u':
			u = true;
			break;
		case 't':
			timeout = number(optarg, INT_MAX);
			break;
		case 'w':
			waitpid_ = number(optarg, INT_MAX);
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (!d && !i && !m && !s && !u)
		i = true;
	if (u && timeout == 0 && argc == 0 && waitpid_ == 0)
		timeout = 5;	/* -u alone: a short burst of user activity */

	snprintf(name, sizeof(name), "caffeinate command-line tool");
	why = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
	if (d)
		hold(kIOPMAssertionTypePreventUserIdleDisplaySleep, why);
	if (i)
		hold(kIOPMAssertionTypePreventUserIdleSystemSleep, why);
	if (m)
		hold(kIOPMAssertPreventDiskIdle, why);
	if (s)
		hold(kIOPMAssertionTypePreventSystemSleep, why);
	if (u)
		hold(NULL, why);

	if ((kq = kqueue()) == -1)
		err(1, "kqueue");
	n = 0;
	if (argc > 0) {
		int error = posix_spawnp(&child, argv[0], NULL, NULL, argv,
		    environ);

		if (error != 0) {
			release();
			errc(1, error, "%s", argv[0]);
		}
		EV_SET(&ev[n++], child, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);
	}
	if (waitpid_ != 0) {
		EV_SET(&ev[n++], waitpid_, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0,
		    NULL);
	}
	if (n > 0 && kevent(kq, ev, n, NULL, 0, NULL) == -1) {
		if (errno == ESRCH)
			errx(1, "%ld: no such process", waitpid_);
		err(1, "kevent");
	}
	/* Interrupted: release the assertions on the way out. */
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	EV_SET(&ev[0], SIGINT, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	EV_SET(&ev[1], SIGTERM, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	(void)kevent(kq, ev, 2, NULL, 0, NULL);

	/* With a command, its end is what counts, not the timeout. */
	if (child != 0)
		timeout = 0;
	for (;;) {
		struct timespec ts = { timeout, 0 };
		struct kevent got;
		int r = kevent(kq, NULL, 0, &got, 1, timeout ? &ts : NULL);

		if (r == -1 && errno == EINTR)
			continue;
		if (r <= 0)
			break;		/* the time is up */
		if (got.filter == EVFILT_SIGNAL)
			break;
		if (got.filter == EVFILT_PROC && (pid_t)got.ident == child) {
			(void)waitpid(child, &status, 0);
			child = 0;
			break;
		}
		if (got.filter == EVFILT_PROC)
			break;		/* the -w process ended */
	}
	release();
	if (argc > 0 && child == 0)
		return (WIFEXITED(status) ? WEXITSTATUS(status) :
		    128 + WTERMSIG(status));
	return (0);
}
