/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The ravynOS contributors
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
 * ravyninit: a temporary PID 1 for bring-up (architecture plan, gate M1).
 *
 * It stands in for launchd until launchd runs: it opens the console,
 * reports that userland is alive, checks that a second program can be
 * started, then keeps an interactive shell on the console and reaps
 * orphaned processes. The disk image installs it as /sbin/launchd, the
 * only init path a RELEASE kernel tries.
 *
 * The console lines starting with "ravynOS:" are markers the boot test
 * (Image/boottest.py) waits for; keep them in sync.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CONSOLE		"/dev/console"
#define SHELL		"/bin/sh"

#define MARKER_PID1	"ravynOS: PID 1 running"
#define MARKER_SHELL	"ravynOS: shell running"

/* execve() takes non-const strings, so these are arrays, not literals. */
static char env_path[] = "PATH=/bin:/sbin:/usr/bin:/usr/sbin";
static char env_home[] = "HOME=/";
static char env_shell[] = "SHELL=" SHELL;
static char env_term[] = "TERM=vt100";
static char *const shell_env[] = { env_path, env_home, env_shell, env_term, NULL };

static char arg_shell[] = SHELL;
static char arg_c[] = "-c";
static char arg_i[] = "-i";
static char arg_echo[] = "echo '" MARKER_SHELL "'";

static void
warnmsg(const char *fmt, ...)
{
	va_list ap;

	fputs("ravyninit: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

/*
 * The kernel starts PID 1 with no open files. Without a console there is
 * nowhere to report anything, so give up: the kernel then panics with
 * "initproc exited", which the boot test reports.
 */
static void
open_console(void)
{
	int fd;

	fd = open(CONSOLE, O_RDWR | O_NOCTTY);
	if (fd < 0)
		_exit(1);
	for (int i = 0; i <= STDERR_FILENO; i++) {
		if (fd != i && dup2(fd, i) < 0)
			_exit(1);
	}
	if (fd > STDERR_FILENO)
		close(fd);
	setvbuf(stdout, NULL, _IOLBF, 0);
}

/* Start argv[0]; an interactive child gets the console as its terminal. */
static pid_t
spawn(char *const argv[], int interactive)
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		warnmsg("fork: %s", strerror(errno));
		return -1;
	}
	if (pid == 0) {
		if (interactive) {
			if (setsid() < 0)
				warnmsg("setsid: %s", strerror(errno));
			else if (ioctl(STDIN_FILENO, TIOCSCTTY, 0) < 0)
				warnmsg("console as terminal: %s", strerror(errno));
		}
		execve(argv[0], argv, shell_env);
		warnmsg("exec %s: %s", argv[0], strerror(errno));
		_exit(127);
	}
	return pid;
}

/* Wait for pid, reaping any orphan that exits meanwhile. */
static int
wait_for(pid_t pid)
{
	pid_t done;
	int status;

	for (;;) {
		done = waitpid(-1, &status, 0);
		if (done == pid)
			return status;
		if (done < 0 && errno != EINTR) {
			warnmsg("waitpid: %s", strerror(errno));
			return -1;
		}
	}
}

static void
describe_exit(const char *what, int status)
{
	if (WIFEXITED(status))
		warnmsg("%s exited with status %d", what, WEXITSTATUS(status));
	else if (WIFSIGNALED(status))
		warnmsg("%s killed by signal %d", what, WTERMSIG(status));
}

int
main(int argc, char *argv[])
{
	char *const check[] = { arg_shell, arg_c, arg_echo, NULL };
	char *const login[] = { arg_shell, arg_i, NULL };
	struct utsname uts;
	pid_t pid;
	int status;

	(void)argc;
	(void)argv;

	open_console();
	if (getpid() != 1) {
		warnmsg("must run as process 1");
		return 1;
	}

	if (uname(&uts) == 0)
		printf("%s (%s %s, %s)\n", MARKER_PID1, uts.sysname, uts.release,
		    uts.machine);
	else
		printf("%s\n", MARKER_PID1);

	/* A second program through fork, exec and dyld. */
	pid = spawn(check, 0);
	if (pid > 0) {
		status = wait_for(pid);
		if (status > 0)
			describe_exit(SHELL " check", status);
	}

	for (;;) {
		pid = spawn(login, 1);
		if (pid > 0) {
			status = wait_for(pid);
			if (status >= 0)
				describe_exit(SHELL, status);
		}
		/* Do not spin if the shell cannot start or exits at once. */
		sleep(1);
	}
}
