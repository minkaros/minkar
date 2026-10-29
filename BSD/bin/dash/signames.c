/*-
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Signal names for dash (kill -l, trap), built from Darwin's <signal.h>
 * so the numbers are the target's. dash's own generator runs on the build
 * machine and would give Linux's numbers; the names are written without
 * "SIG", and slot 0 is the exit trap.
 */

#include <signal.h>
#include <stddef.h>

char *signal_names[NSIG + 1] = {
	[0] = "EXIT",
	[SIGHUP] = "HUP",
	[SIGINT] = "INT",
	[SIGQUIT] = "QUIT",
	[SIGILL] = "ILL",
	[SIGTRAP] = "TRAP",
	[SIGABRT] = "ABRT",
	[SIGEMT] = "EMT",
	[SIGFPE] = "FPE",
	[SIGKILL] = "KILL",
	[SIGBUS] = "BUS",
	[SIGSEGV] = "SEGV",
	[SIGSYS] = "SYS",
	[SIGPIPE] = "PIPE",
	[SIGALRM] = "ALRM",
	[SIGTERM] = "TERM",
	[SIGURG] = "URG",
	[SIGSTOP] = "STOP",
	[SIGTSTP] = "TSTP",
	[SIGCONT] = "CONT",
	[SIGCHLD] = "CHLD",
	[SIGTTIN] = "TTIN",
	[SIGTTOU] = "TTOU",
	[SIGIO] = "IO",
	[SIGXCPU] = "XCPU",
	[SIGXFSZ] = "XFSZ",
	[SIGVTALRM] = "VTALRM",
	[SIGPROF] = "PROF",
	[SIGWINCH] = "WINCH",
	[SIGINFO] = "INFO",
	[SIGUSR1] = "USR1",
	[SIGUSR2] = "USR2",
	[NSIG] = NULL,
};
