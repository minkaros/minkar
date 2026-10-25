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
 * top: show the processes using the most CPU (or memory), refreshed.
 *
 *   top [-l samples] [-s delay] [-n nprocs] [-o cpu|mem|pid|time]
 *       [-U user]
 *
 * With -l, it prints that many samples one after another (logging
 * mode, 0 for ever) instead of redrawing the screen; the first sample
 * has no CPU percentages, as there is nothing to compare with yet.
 * Processes come from libproc, the system's figures from the Mach host
 * interface and sysctl. Interactively, q quits.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <mach/mach.h>
#include <mach/mach_time.h>

#include <err.h>
#include <errno.h>
#include <libproc.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

enum sortkey { SORT_CPU, SORT_MEM, SORT_PID, SORT_TIME };

struct pinfo {
	pid_t		pid;
	uid_t		uid;
	char		comm[2 * MAXCOMLEN + 1];
	uint64_t	cputime;	/* user + system, nanoseconds */
	uint64_t	rss;		/* bytes */
	int		threads;
	int		running;	/* threads running */
	uint32_t	status;
	double		pcpu;
};

struct sample {
	struct pinfo	*procs;
	size_t		 nprocs;
	uint64_t	 when;		/* nanoseconds, monotonic */
	host_cpu_load_info_data_t cpu;
};

static enum sortkey sortkey = SORT_CPU;
static int	maxprocs = -1;		/* all that fit (or all, with -l) */
static uid_t	only_uid;
static bool	only_user;
static bool	logging;
static struct termios saved_tty;
static bool	tty_saved;

static void	usage(void) __dead2;

static uint64_t
now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec);
}

/* Mach absolute time units to nanoseconds. */
static uint64_t
abs_to_ns(uint64_t t)
{
	static mach_timebase_info_data_t tb;

	if (tb.denom == 0)
		mach_timebase_info(&tb);
	return (t * tb.numer / tb.denom);
}

static void
take_sample(struct sample *s)
{
	pid_t *pids;
	int n, cap;
	mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;

	memset(s, 0, sizeof(*s));
	s->when = now_ns();
	if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
	    (host_info_t)&s->cpu, &count) != KERN_SUCCESS)
		memset(&s->cpu, 0, sizeof(s->cpu));

	cap = proc_listallpids(NULL, 0);
	if (cap <= 0)
		err(EX_OSERR, "proc_listallpids");
	cap += 32;
	if ((pids = calloc((size_t)cap, sizeof(pid_t))) == NULL)
		err(EX_OSERR, "calloc");
	n = proc_listallpids(pids, cap * (int)sizeof(pid_t));
	if (n < 0)
		err(EX_OSERR, "proc_listallpids");
	if ((s->procs = calloc((size_t)n + 1, sizeof(struct pinfo))) == NULL)
		err(EX_OSERR, "calloc");

	for (int i = 0; i < n; i++) {
		struct proc_taskallinfo ti;
		struct pinfo *p = &s->procs[s->nprocs];

		/* Processes owned by others need root for task info. */
		if (proc_pidinfo(pids[i], PROC_PIDTASKALLINFO, 0, &ti,
		    sizeof(ti)) != (int)sizeof(ti)) {
			struct proc_bsdinfo bi;

			if (proc_pidinfo(pids[i], PROC_PIDTBSDINFO, 0, &bi,
			    sizeof(bi)) != (int)sizeof(bi))
				continue;
			memset(&ti, 0, sizeof(ti));
			ti.pbsd = bi;
		}
		if (only_user && ti.pbsd.pbi_uid != only_uid)
			continue;
		p->pid = (pid_t)ti.pbsd.pbi_pid;
		p->uid = ti.pbsd.pbi_uid;
		snprintf(p->comm, sizeof(p->comm), "%s",
		    ti.pbsd.pbi_name[0] != '\0' ? ti.pbsd.pbi_name :
		    ti.pbsd.pbi_comm);
		p->cputime = abs_to_ns(ti.ptinfo.pti_total_user +
		    ti.ptinfo.pti_total_system);
		p->rss = ti.ptinfo.pti_resident_size;
		p->threads = ti.ptinfo.pti_threadnum;
		p->running = ti.ptinfo.pti_numrunning;
		p->status = ti.pbsd.pbi_status;
		s->nprocs++;
	}
	free(pids);
}

static void
free_sample(struct sample *s)
{

	free(s->procs);
	s->procs = NULL;
	s->nprocs = 0;
}

static const struct pinfo *
find_proc(const struct sample *s, pid_t pid)
{

	for (size_t i = 0; i < s->nprocs; i++)
		if (s->procs[i].pid == pid)
			return (&s->procs[i]);
	return (NULL);
}

static int
compare(const void *a, const void *b)
{
	const struct pinfo *p = a, *q = b;

	switch (sortkey) {
	case SORT_MEM:
		if (p->rss != q->rss)
			return (p->rss < q->rss ? 1 : -1);
		break;
	case SORT_TIME:
		if (p->cputime != q->cputime)
			return (p->cputime < q->cputime ? 1 : -1);
		break;
	case SORT_CPU:
		if (p->pcpu != q->pcpu)
			return (p->pcpu < q->pcpu ? 1 : -1);
		if (p->cputime != q->cputime)
			return (p->cputime < q->cputime ? 1 : -1);
		break;
	case SORT_PID:
		break;
	}
	return (p->pid < q->pid ? -1 : p->pid > q->pid);
}

static void
format_bytes(uint64_t b, char *buf, size_t len)
{
	const char *units = "BKMGT";
	double v = (double)b;
	int u = 0;

	while (v >= 1024 && u < 4) {
		v /= 1024;
		u++;
	}
	if (u == 0)
		snprintf(buf, len, "%lluB", (unsigned long long)b);
	else
		snprintf(buf, len, v < 10 ? "%.1f%c" : "%.0f%c", v, units[u]);
}

static void
format_time(uint64_t ns, char *buf, size_t len)
{
	uint64_t cs = ns / 10000000ULL;		/* hundredths */
	uint64_t s = cs / 100;

	if (s >= 3600)
		snprintf(buf, len, "%llu:%02llu:%02llu",
		    (unsigned long long)(s / 3600),
		    (unsigned long long)(s / 60 % 60),
		    (unsigned long long)(s % 60));
	else
		snprintf(buf, len, "%02llu:%02llu.%02llu",
		    (unsigned long long)(s / 60), (unsigned long long)(s % 60),
		    (unsigned long long)(cs % 100));
}

static int
screen_rows(void)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0)
		return (ws.ws_row);
	return (24);
}

static void
print_header(const struct sample *cur, const struct sample *prev)
{
	double load[3];
	size_t running = 0, threads = 0;
	uint64_t memsize = 0;
	size_t len = sizeof(memsize);
	vm_statistics64_data_t vm;
	mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
	time_t t = time(NULL);
	char tbuf[32];

	for (size_t i = 0; i < cur->nprocs; i++) {
		if (cur->procs[i].running > 0)
			running++;
		threads += (size_t)cur->procs[i].threads;
	}
	strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&t));
	printf("Processes: %zu total, %zu running, %zu sleeping, %zu threads"
	    "   %s\n", cur->nprocs, running, cur->nprocs - running, threads,
	    tbuf);

	if (getloadavg(load, 3) == 3)
		printf("Load Avg: %.2f, %.2f, %.2f  ", load[0], load[1],
		    load[2]);
	if (prev != NULL) {
		uint64_t d[CPU_STATE_MAX], total = 0;

		for (int i = 0; i < CPU_STATE_MAX; i++) {
			d[i] = (uint64_t)(cur->cpu.cpu_ticks[i] -
			    prev->cpu.cpu_ticks[i]);
			total += d[i];
		}
		if (total > 0)
			printf("CPU usage: %.1f%% user, %.1f%% sys, "
			    "%.1f%% idle",
			    100.0 * (double)(d[CPU_STATE_USER] +
			    d[CPU_STATE_NICE]) / (double)total,
			    100.0 * (double)d[CPU_STATE_SYSTEM] / (double)total,
			    100.0 * (double)d[CPU_STATE_IDLE] / (double)total);
	}
	printf("\n");

	if (sysctlbyname("hw.memsize", &memsize, &len, NULL, 0) == 0 &&
	    host_statistics64(mach_host_self(), HOST_VM_INFO64,
	    (host_info64_t)&vm, &count) == KERN_SUCCESS) {
		uint64_t page = (uint64_t)vm_kernel_page_size;
		uint64_t freeb = (uint64_t)vm.free_count * page;
		char used[16], fr[16], wired[16];

		format_bytes(memsize > freeb ? memsize - freeb : 0, used,
		    sizeof(used));
		format_bytes((uint64_t)vm.wire_count * page, wired,
		    sizeof(wired));
		format_bytes(freeb, fr, sizeof(fr));
		printf("PhysMem: %s used (%s wired), %s unused.\n", used,
		    wired, fr);
	}
	printf("\n%-6s %-16s %5s %9s %4s %6s %-8s %s\n", "PID", "COMMAND",
	    "%CPU", "TIME", "#TH", "MEM", "USER", "STATE");
}

static void
print_sample(struct sample *cur, const struct sample *prev)
{
	double dt = prev != NULL ? (double)(cur->when - prev->when) : 0;
	int limit;

	for (size_t i = 0; i < cur->nprocs; i++) {
		struct pinfo *p = &cur->procs[i];
		const struct pinfo *old;

		p->pcpu = 0;
		if (dt > 0 && (old = find_proc(prev, p->pid)) != NULL &&
		    p->cputime >= old->cputime)
			p->pcpu = 100.0 * (double)(p->cputime - old->cputime) /
			    dt;
	}
	qsort(cur->procs, cur->nprocs, sizeof(struct pinfo), compare);

	print_header(cur, prev);
	limit = maxprocs;
	if (limit < 0)
		limit = logging ? (int)cur->nprocs : screen_rows() - 6;
	for (size_t i = 0; i < cur->nprocs && (int)i < limit; i++) {
		const struct pinfo *p = &cur->procs[i];
		struct passwd *pw = getpwuid(p->uid);
		char user[16], mem[16], tm[24];
		const char *state;

		if (pw != NULL)
			snprintf(user, sizeof(user), "%s", pw->pw_name);
		else
			snprintf(user, sizeof(user), "%u", p->uid);
		format_bytes(p->rss, mem, sizeof(mem));
		format_time(p->cputime, tm, sizeof(tm));
		if (p->status == SZOMB)
			state = "zombie";
		else if (p->status == SSTOP)
			state = "stopped";
		else if (p->running > 0)
			state = "running";
		else
			state = "sleeping";
		printf("%-6d %-16.16s %5.1f %9s %4d %6s %-8.8s %s\n", p->pid,
		    p->comm, p->pcpu, tm, p->threads, mem, user, state);
	}
	fflush(stdout);
}

static void
restore_tty(void)
{

	if (tty_saved)
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty);
	if (!logging)
		printf("\n");
}

static void
on_signal(int sig)
{

	(void)sig;
	restore_tty();
	_exit(0);
}

/* Wait up to ms milliseconds; true if the user asked to quit. */
static bool
wait_or_quit(int ms)
{
	struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
	char c;

	if (logging) {
		usleep((useconds_t)ms * 1000);
		return (false);
	}
	if (poll(&pfd, 1, ms) > 0 && read(STDIN_FILENO, &c, 1) == 1)
		return (c == 'q' || c == 'Q' || c == 4 /* ^D */);
	return (false);
}

int
main(int argc, char *argv[])
{
	struct sample prev, cur;
	const char *errstr;
	int ch, samples = -1, delay = 1;
	bool have_prev = false;

	while ((ch = getopt(argc, argv, "l:n:o:s:U:")) != -1) {
		switch (ch) {
		case 'l':
			samples = (int)strtonum(optarg, 0, INT32_MAX, &errstr);
			if (errstr != NULL)
				errx(EX_USAGE, "samples is %s: %s", errstr,
				    optarg);
			logging = true;
			break;
		case 'n':
			maxprocs = (int)strtonum(optarg, 0, INT32_MAX, &errstr);
			if (errstr != NULL)
				errx(EX_USAGE, "nprocs is %s: %s", errstr,
				    optarg);
			break;
		case 'o':
			if (strcmp(optarg, "cpu") == 0)
				sortkey = SORT_CPU;
			else if (strcmp(optarg, "mem") == 0)
				sortkey = SORT_MEM;
			else if (strcmp(optarg, "pid") == 0)
				sortkey = SORT_PID;
			else if (strcmp(optarg, "time") == 0)
				sortkey = SORT_TIME;
			else
				errx(EX_USAGE, "unknown sort key: %s", optarg);
			break;
		case 's':
			delay = (int)strtonum(optarg, 0, 86400, &errstr);
			if (errstr != NULL)
				errx(EX_USAGE, "delay is %s: %s", errstr,
				    optarg);
			break;
		case 'U': {
			struct passwd *pw = getpwnam(optarg);

			if (pw != NULL)
				only_uid = pw->pw_uid;
			else {
				only_uid = (uid_t)strtonum(optarg, 0,
				    UINT32_MAX - 1, &errstr);
				if (errstr != NULL)
					errx(EX_USAGE, "unknown user: %s",
					    optarg);
			}
			only_user = true;
			break;
		}
		default:
			usage();
		}
	}
	if (optind != argc)
		usage();
	if (!logging && !isatty(STDOUT_FILENO))
		logging = true;		/* nothing to redraw */

	if (!logging && isatty(STDIN_FILENO) &&
	    tcgetattr(STDIN_FILENO, &saved_tty) == 0) {
		struct termios raw = saved_tty;

		raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
		raw.c_cc[VMIN] = 1;
		raw.c_cc[VTIME] = 0;
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
		tty_saved = true;
	}
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	signal(SIGHUP, on_signal);

	/* Interactively, a quick first sample gives %CPU from the start. */
	if (!logging) {
		take_sample(&prev);
		have_prev = true;
		usleep(200000);
	}
	for (int n = 0; samples <= 0 || n < samples; n++) {
		take_sample(&cur);
		if (!logging)
			printf("\033[H\033[2J");
		else if (n > 0)
			printf("\n");
		print_sample(&cur, have_prev ? &prev : NULL);
		if (have_prev)
			free_sample(&prev);
		prev = cur;
		have_prev = true;
		if (samples > 0 && n + 1 >= samples)
			break;
		if (wait_or_quit(delay * 1000))
			break;
	}
	if (have_prev)
		free_sample(&prev);
	restore_tty();
	return (0);
}

static void
usage(void)
{

	fprintf(stderr, "usage: top [-l samples] [-s delay] [-n nprocs] "
	    "[-o cpu|mem|pid|time] [-U user]\n");
	exit(EX_USAGE);
}
