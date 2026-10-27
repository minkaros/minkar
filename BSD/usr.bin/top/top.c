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
 *   top [-a | -d | -e | -c mode] [-F | -f] [-h] [-i interval]
 *       [-l samples] [-ncols columns] [-o key | -O skey] [-R | -r] [-S]
 *       [-s delay-secs] [-n nprocs] [-stats keys] [-pid processid]
 *       [-user username] [-U username] [-u]
 *
 * As macOS's top(1) describes it: a summary (processes, load, CPU,
 * memory, VM, networks), then one line per process with the columns
 * -stats names, sorted by -o and -O. With -l, it prints that many
 * samples one after another (logging mode, 0 for ever) instead of
 * redrawing the screen; the first sample has no CPU percentages, as
 * there is nothing to compare with yet. The event counters (faults, cow,
 * msgsent, msgrecv, sysbsd, sysmach, csw, pageins) are absolute, or with
 * -c a since top started, or with -c d since the previous sample.
 * Processes come from libproc, the system's figures from the Mach host
 * interface, sysctl and getifaddrs. Interactively: q quits, ? helps,
 * o and O change the sort keys, s the delay, n the number of processes,
 * U the user.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/if.h>
#include <mach/mach.h>
#include <mach/mach_time.h>

#include <err.h>
#include <errno.h>
#include <ifaddrs.h>
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

enum colkey {
	K_PID, K_COMMAND, K_CPU, K_TIME, K_THREADS, K_WQ, K_MEM, K_VSIZE,
	K_PGRP, K_PPID, K_STATE, K_UID, K_FAULTS, K_COW, K_MSGSENT,
	K_MSGRECV, K_SYSBSD, K_SYSMACH, K_CSW, K_PAGEINS, K_USER, K_NKEYS
};

/* The event counters, in the order of struct pinfo's ev[]. */
#define	EV_FIRST	K_FAULTS
#define	EV_LAST		K_PAGEINS
#define	NEV		(EV_LAST - EV_FIRST + 1)

static const struct column {
	const char	*key;
	const char	*header;
	int		 width;
	bool		 left;		/* text: left-aligned */
	bool		 ascending;	/* the default sort order */
} columns[K_NKEYS] = {
	[K_PID] =	{ "pid",	"PID",		6, false, false },
	[K_COMMAND] =	{ "command",	"COMMAND",	16, true, true },
	[K_CPU] =	{ "cpu",	"%CPU",		5, false, false },
	[K_TIME] =	{ "time",	"TIME",		9, false, false },
	[K_THREADS] =	{ "threads",	"#TH",		4, false, false },
	[K_WQ] =	{ "wq",		"#WQ",		4, false, false },
	[K_MEM] =	{ "mem",	"MEM",		6, false, false },
	[K_VSIZE] =	{ "vsize",	"VSIZE",	6, false, false },
	[K_PGRP] =	{ "pgrp",	"PGRP",		6, false, false },
	[K_PPID] =	{ "ppid",	"PPID",		6, false, false },
	[K_STATE] =	{ "state",	"STATE",	8, true, true },
	[K_UID] =	{ "uid",	"UID",		5, false, false },
	[K_FAULTS] =	{ "faults",	"FAULTS",	8, false, false },
	[K_COW] =	{ "cow",	"COW",		6, false, false },
	[K_MSGSENT] =	{ "msgsent",	"MSGSENT",	8, false, false },
	[K_MSGRECV] =	{ "msgrecv",	"MSGRECV",	8, false, false },
	[K_SYSBSD] =	{ "sysbsd",	"SYSBSD",	8, false, false },
	[K_SYSMACH] =	{ "sysmach",	"SYSMACH",	8, false, false },
	[K_CSW] =	{ "csw",	"CSW",		8, false, false },
	[K_PAGEINS] =	{ "pageins",	"PAGEINS",	7, false, false },
	[K_USER] =	{ "user",	"USER",		10, true, true },
};

/* macOS's keys that Minkar cannot show yet. */
static const char *const unsupported[] = {
	"boosts", "cmprs", "cpu_me", "cpu_others", "cycles", "idlew",
	"instrs", "kprvt", "kshrd", "mregion", "ports", "power", "purg",
	"rprvt", "vprvt", NULL
};

struct pinfo {
	pid_t		pid;
	pid_t		ppid;
	pid_t		pgrp;
	uid_t		uid;
	char		comm[2 * MAXCOMLEN + 1];
	uint64_t	cputime;	/* user + system, nanoseconds */
	uint64_t	rss;		/* bytes */
	uint64_t	vsize;		/* bytes */
	int		threads;
	int		wq;		/* workqueue threads */
	int		running;	/* threads running */
	uint32_t	status;
	int64_t		ev[NEV];	/* faults ... pageins */
	double		pcpu;
};

struct sample {
	struct pinfo	*procs;
	size_t		 nprocs;
	uint64_t	 when;		/* nanoseconds, monotonic */
	host_cpu_load_info_data_t cpu;
};

static enum colkey stats[K_NKEYS];	/* the columns shown, in order */
static int	nstats;
static enum colkey sortkey = K_CPU, sortkey2 = K_PID;
static bool	sortasc, sortasc2;
static char	evmode = 'n';		/* n, e: absolute; a, d: see above */
static int	maxprocs = -1;		/* all that fit (or all, with -l) */
static int	ncols = -1;		/* -ncols, in logging mode */
static int	delay = 1;
static uid_t	only_uid;
static bool	only_user;
static pid_t	*only_pids;
static int	nonly_pids;
static bool	logging;
static bool	show_swap;
static struct termios saved_tty;
static bool	tty_saved;
static struct sample first;		/* for -c a */
static bool	have_first;

static void	usage(int) __dead2;

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

/* A column key, or -1; macOS's unsupported keys are an error. */
static int
find_key(const char *key)
{
	for (int i = 0; i < K_NKEYS; i++)
		if (strcmp(columns[i].key, key) == 0)
			return (i);
	/* Old names. */
	if (strcmp(key, "th") == 0)
		return (K_THREADS);
	if (strcmp(key, "rsize") == 0 || strcmp(key, "rshrd") == 0)
		return (K_MEM);
	for (int i = 0; unsupported[i] != NULL; i++)
		if (strcmp(unsupported[i], key) == 0)
			errx(EX_USAGE, "%s: not supported yet", key);
	return (-1);
}

/* [+|-]key: the key and whether it sorts ascending. */
static bool
parse_sort(const char *arg, enum colkey *key, bool *asc)
{
	int k;
	char sign = 0;

	if (*arg == '+' || *arg == '-')
		sign = *arg++;
	if ((k = find_key(arg)) < 0)
		return (false);
	*key = (enum colkey)k;
	*asc = sign == '+' ? true : sign == '-' ? false : columns[k].ascending;
	return (true);
}

static void
parse_stats(const char *arg)
{
	char *copy, *p, *key;
	int k;

	if ((copy = strdup(arg)) == NULL)
		err(EX_OSERR, "strdup");
	nstats = 0;
	for (p = copy; (key = strsep(&p, ", ")) != NULL;) {
		if (*key == '\0')
			continue;
		if ((k = find_key(key)) < 0)
			errx(EX_USAGE, "unknown key: %s", key);
		if (nstats < K_NKEYS)
			stats[nstats++] = (enum colkey)k;
	}
	free(copy);
	if (nstats == 0)
		errx(EX_USAGE, "-stats: no keys");
}

static bool
selected(const struct proc_bsdinfo *bi)
{
	if (only_user && bi->pbi_uid != only_uid)
		return (false);
	if (nonly_pids > 0) {
		for (int i = 0; i < nonly_pids; i++)
			if ((pid_t)bi->pbi_pid == only_pids[i])
				return (true);
		return (false);
	}
	return (true);
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
		struct proc_workqueueinfo wq;
		struct pinfo *p = &s->procs[s->nprocs];
		const struct proc_taskinfo *t = &ti.ptinfo;

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
		if (!selected(&ti.pbsd))
			continue;
		p->pid = (pid_t)ti.pbsd.pbi_pid;
		p->ppid = (pid_t)ti.pbsd.pbi_ppid;
		p->pgrp = (pid_t)ti.pbsd.pbi_pgid;
		p->uid = ti.pbsd.pbi_uid;
		snprintf(p->comm, sizeof(p->comm), "%s",
		    ti.pbsd.pbi_name[0] != '\0' ? ti.pbsd.pbi_name :
		    ti.pbsd.pbi_comm);
		p->cputime = abs_to_ns(t->pti_total_user + t->pti_total_system);
		p->rss = t->pti_resident_size;
		p->vsize = t->pti_virtual_size;
		p->threads = t->pti_threadnum;
		p->running = t->pti_numrunning;
		p->status = ti.pbsd.pbi_status;
		p->ev[K_FAULTS - EV_FIRST] = t->pti_faults;
		p->ev[K_COW - EV_FIRST] = t->pti_cow_faults;
		p->ev[K_MSGSENT - EV_FIRST] = t->pti_messages_sent;
		p->ev[K_MSGRECV - EV_FIRST] = t->pti_messages_received;
		p->ev[K_SYSBSD - EV_FIRST] = t->pti_syscalls_unix;
		p->ev[K_SYSMACH - EV_FIRST] = t->pti_syscalls_mach;
		p->ev[K_CSW - EV_FIRST] = t->pti_csw;
		p->ev[K_PAGEINS - EV_FIRST] = t->pti_pageins;
		if (proc_pidinfo(pids[i], PROC_PIDWORKQUEUEINFO, 0, &wq,
		    sizeof(wq)) == (int)sizeof(wq))
			p->wq = (int)wq.pwq_nthreads;
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

	if (s == NULL)
		return (NULL);
	for (size_t i = 0; i < s->nprocs; i++)
		if (s->procs[i].pid == pid)
			return (&s->procs[i]);
	return (NULL);
}

static const char *
state_name(const struct pinfo *p)
{
	if (p->status == SZOMB)
		return ("zombie");
	if (p->status == SSTOP)
		return ("stopped");
	if (p->running > 0)
		return ("running");
	return ("sleeping");
}

static const char *
user_name(uid_t uid, char *buf, size_t len)
{
	struct passwd *pw = getpwuid(uid);

	if (pw != NULL)
		snprintf(buf, len, "%s", pw->pw_name);
	else
		snprintf(buf, len, "%d", (int)uid);
	return (buf);
}

static int
compare_key(const struct pinfo *p, const struct pinfo *q, enum colkey k)
{
#define	CMP(a, b)	((a) < (b) ? -1 : (a) > (b))
	char ub[32], vb[32];

	switch (k) {
	case K_PID:	return (CMP(p->pid, q->pid));
	case K_COMMAND:	return (strcmp(p->comm, q->comm));
	case K_CPU:	return (CMP(p->pcpu, q->pcpu));
	case K_TIME:	return (CMP(p->cputime, q->cputime));
	case K_THREADS:	return (CMP(p->threads, q->threads));
	case K_WQ:	return (CMP(p->wq, q->wq));
	case K_MEM:	return (CMP(p->rss, q->rss));
	case K_VSIZE:	return (CMP(p->vsize, q->vsize));
	case K_PGRP:	return (CMP(p->pgrp, q->pgrp));
	case K_PPID:	return (CMP(p->ppid, q->ppid));
	case K_STATE:	return (strcmp(state_name(p), state_name(q)));
	case K_UID:	return (CMP((int)p->uid, (int)q->uid));
	case K_USER:	return (strcmp(user_name(p->uid, ub, sizeof(ub)),
			    user_name(q->uid, vb, sizeof(vb))));
	default:
		return (CMP(p->ev[k - EV_FIRST], q->ev[k - EV_FIRST]));
	}
#undef CMP
}

static int
compare(const void *a, const void *b)
{
	const struct pinfo *p = a, *q = b;
	int c;

	if ((c = compare_key(p, q, sortkey)) != 0)
		return (sortasc ? c : -c);
	if ((c = compare_key(p, q, sortkey2)) != 0)
		return (sortasc2 ? c : -c);
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

static int
screen_cols(void)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
		return (ws.ws_col);
	return (80);
}

/* Bytes and packets in and out, over the interfaces but loopback. */
static bool
network_totals(uint64_t *ipk, uint64_t *ib, uint64_t *opk, uint64_t *ob)
{
	struct ifaddrs *ifa, *i;

	*ipk = *ib = *opk = *ob = 0;
	if (getifaddrs(&ifa) == -1)
		return (false);
	for (i = ifa; i != NULL; i = i->ifa_next) {
		const struct if_data *d = i->ifa_data;

		if (i->ifa_addr == NULL || i->ifa_addr->sa_family != AF_LINK ||
		    d == NULL || (i->ifa_flags & IFF_LOOPBACK))
			continue;
		*ipk += d->ifi_ipackets;
		*ib += d->ifi_ibytes;
		*opk += d->ifi_opackets;
		*ob += d->ifi_obytes;
	}
	freeifaddrs(ifa);
	return (true);
}

static void
print_header(const struct sample *cur, const struct sample *prev)
{
	double load[3];
	size_t running = 0, threads = 0;
	uint64_t memsize = 0, vsize = 0;
	size_t len = sizeof(memsize);
	vm_statistics64_data_t vm;
	mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
	time_t t = time(NULL);
	char tbuf[32];
	bool have_vm;

	for (size_t i = 0; i < cur->nprocs; i++) {
		if (cur->procs[i].running > 0)
			running++;
		threads += (size_t)cur->procs[i].threads;
		vsize += cur->procs[i].vsize;
	}
	strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&t));
	printf("Processes: %zu total, %zu running, %zu sleeping, %zu threads"
	    "   %s\n", cur->nprocs, running, cur->nprocs - running, threads,
	    tbuf);

	if (getloadavg(load, 3) == 3)
		printf("Load Avg: %.2f, %.2f, %.2f", load[0], load[1],
		    load[2]);
	if (prev != NULL) {
		uint64_t d[CPU_STATE_MAX], total = 0;

		for (int i = 0; i < CPU_STATE_MAX; i++) {
			d[i] = (uint64_t)(cur->cpu.cpu_ticks[i] -
			    prev->cpu.cpu_ticks[i]);
			total += d[i];
		}
		if (total > 0)
			printf("  CPU usage: %.2f%% user, %.2f%% sys, "
			    "%.2f%% idle",
			    100.0 * (double)(d[CPU_STATE_USER] +
			    d[CPU_STATE_NICE]) / (double)total,
			    100.0 * (double)d[CPU_STATE_SYSTEM] / (double)total,
			    100.0 * (double)d[CPU_STATE_IDLE] / (double)total);
	}
	printf("\n");

	have_vm = host_statistics64(mach_host_self(), HOST_VM_INFO64,
	    (host_info64_t)&vm, &count) == KERN_SUCCESS;
	if (have_vm && sysctlbyname("hw.memsize", &memsize, &len, NULL,
	    0) == 0) {
		uint64_t page = (uint64_t)vm_kernel_page_size;
		uint64_t freeb = (uint64_t)vm.free_count * page;
		char used[16], fr[16], wired[16], cmp[16];

		format_bytes(memsize > freeb ? memsize - freeb : 0, used,
		    sizeof(used));
		format_bytes((uint64_t)vm.wire_count * page, wired,
		    sizeof(wired));
		format_bytes((uint64_t)vm.compressor_page_count * page, cmp,
		    sizeof(cmp));
		format_bytes(freeb, fr, sizeof(fr));
		printf("PhysMem: %s used (%s wired, %s compressor), "
		    "%s unused.\n", used, wired, cmp, fr);
	}
	if (have_vm) {
		char vs[16];

		format_bytes(vsize, vs, sizeof(vs));
		printf("VM: %s vsize, %llu swapins, %llu swapouts.\n", vs,
		    (unsigned long long)vm.swapins,
		    (unsigned long long)vm.swapouts);
	}
	if (show_swap) {
		struct xsw_usage sw;
		size_t swlen = sizeof(sw);
		char used[16], fr[16];

		if (sysctlbyname("vm.swapusage", &sw, &swlen, NULL, 0) == -1)
			memset(&sw, 0, sizeof(sw));
		format_bytes(sw.xsu_used, used, sizeof(used));
		format_bytes(sw.xsu_avail, fr, sizeof(fr));
		printf("Swap: %s + %s free.\n", used, fr);
	}
	{
		uint64_t ipk, ib, opk, ob;
		char ibs[16], obs[16];

		if (network_totals(&ipk, &ib, &opk, &ob)) {
			format_bytes(ib, ibs, sizeof(ibs));
			format_bytes(ob, obs, sizeof(obs));
			printf("Networks: packets: %llu/%s in, %llu/%s out.\n",
			    (unsigned long long)ipk, ibs,
			    (unsigned long long)opk, obs);
		}
	}
}

/* How many of the columns fit: -ncols in logging mode, else the screen. */
static int
columns_shown(void)
{
	int n, used;

	if (logging)
		return (ncols > 0 && ncols < nstats ? ncols : nstats);
	used = 0;
	for (n = 0; n < nstats; n++) {
		used += columns[stats[n]].width + (n > 0);
		if (used > screen_cols() && n > 0)
			break;
	}
	return (n);
}

/* last: no padding after a left-aligned last column. */
static void
print_cell(const struct pinfo *p, enum colkey k, const struct sample *prev,
    bool last)
{
	const struct column *c = &columns[k];
	char buf[64];
	const struct pinfo *old;
	int64_t v;

	switch (k) {
	case K_PID:	snprintf(buf, sizeof(buf), "%d", p->pid); break;
	case K_COMMAND:	snprintf(buf, sizeof(buf), "%.16s", p->comm); break;
	case K_CPU:	snprintf(buf, sizeof(buf), "%.1f", p->pcpu); break;
	case K_TIME:	format_time(p->cputime, buf, sizeof(buf)); break;
	case K_THREADS:	snprintf(buf, sizeof(buf), "%d", p->threads); break;
	case K_WQ:	snprintf(buf, sizeof(buf), "%d", p->wq); break;
	case K_MEM:	format_bytes(p->rss, buf, sizeof(buf)); break;
	case K_VSIZE:	format_bytes(p->vsize, buf, sizeof(buf)); break;
	case K_PGRP:	snprintf(buf, sizeof(buf), "%d", p->pgrp); break;
	case K_PPID:	snprintf(buf, sizeof(buf), "%d", p->ppid); break;
	case K_STATE:	snprintf(buf, sizeof(buf), "%s", state_name(p)); break;
	case K_UID:	snprintf(buf, sizeof(buf), "%d", (int)p->uid); break;
	case K_USER:	user_name(p->uid, buf, sizeof(buf)); break;
	default:
		v = p->ev[k - EV_FIRST];
		if (evmode == 'd' && (old = find_proc(prev, p->pid)) != NULL)
			v -= old->ev[k - EV_FIRST];
		else if (evmode == 'a' && have_first &&
		    (old = find_proc(&first, p->pid)) != NULL)
			v -= old->ev[k - EV_FIRST];
		snprintf(buf, sizeof(buf), "%lld", (long long)v);
		break;
	}
	if (c->left && last)
		printf("%.*s", c->width, buf);
	else
		printf(c->left ? "%-*.*s" : "%*.*s", c->width, c->width, buf);
}

static void
print_sample(struct sample *cur, const struct sample *prev)
{
	double dt = prev != NULL ? (double)(cur->when - prev->when) : 0;
	int limit, shown;

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
	shown = columns_shown();
	printf("\n");
	for (int j = 0; j < shown; j++) {
		const struct column *c = &columns[stats[j]];

		printf(j > 0 ? " " : "");
		if (c->left && j == shown - 1)
			printf("%s", c->header);
		else
			printf(c->left ? "%-*s" : "%*s", c->width, c->header);
	}
	printf("\n");
	limit = maxprocs;
	if (limit < 0)
		limit = logging ? (int)cur->nprocs : screen_rows() - 9;
	for (size_t i = 0; i < cur->nprocs && (int)i < limit; i++) {
		for (int j = 0; j < shown; j++) {
			printf(j > 0 ? " " : "");
			print_cell(&cur->procs[i], stats[j], prev,
			    j == shown - 1);
		}
		printf("\n");
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

/* Asks a question on the last line, in cooked mode. */
static bool
prompt(const char *question, char *buf, size_t len)
{
	char *nl;
	bool ok;

	if (tty_saved)
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty);
	printf("\033[%d;1H\033[K%s", screen_rows(), question);
	fflush(stdout);
	ok = fgets(buf, (int)len, stdin) != NULL;
	if (tty_saved) {
		struct termios raw = saved_tty;

		raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
		raw.c_cc[VMIN] = 1;
		raw.c_cc[VTIME] = 0;
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
	}
	if (ok && (nl = strchr(buf, '\n')) != NULL)
		*nl = '\0';
	return (ok && *buf != '\0');
}

static void
show_help(void)
{
	char c;

	printf("\033[H\033[2J"
	    "top: interactive commands\n\n"
	    "  ?   show this help\n"
	    "  q   quit\n"
	    "  o   set the primary sort key ([+|-]key)\n"
	    "  O   set the secondary sort key\n"
	    "  s   set the delay between updates (seconds)\n"
	    "  n   set the number of processes shown\n"
	    "  U   show only one user's processes (empty: all)\n\n"
	    "keys: pid command cpu time threads wq mem vsize pgrp ppid state\n"
	    "      uid user faults cow msgsent msgrecv sysbsd sysmach csw\n"
	    "      pageins\n\n"
	    "Press any key to continue.");
	fflush(stdout);
	(void)read(STDIN_FILENO, &c, 1);
}

static void
set_user(const char *name)
{
	struct passwd *pw = getpwnam(name);
	const char *errstr;

	if (pw != NULL)
		only_uid = pw->pw_uid;
	else {
		only_uid = (uid_t)strtonum(name, 0, UINT32_MAX - 1, &errstr);
		if (errstr != NULL)
			errx(EX_USAGE, "unknown user: %s", name);
	}
	only_user = true;
}

/* One interactive command; true to quit. */
static bool
command(char c)
{
	char buf[64];
	const char *errstr;
	int v;

	switch (c) {
	case 'q': case 'Q': case 4:	/* ^D */
		return (true);
	case '?': case 'h':
		show_help();
		break;
	case 'o':
		if (prompt("primary key [+|-]key: ", buf, sizeof(buf)) &&
		    !parse_sort(buf, &sortkey, &sortasc))
			break;
		break;
	case 'O':
		if (prompt("secondary key [+|-]key: ", buf, sizeof(buf)))
			(void)parse_sort(buf, &sortkey2, &sortasc2);
		break;
	case 's':
		if (prompt("delay between updates (seconds): ", buf,
		    sizeof(buf))) {
			v = (int)strtonum(buf, 0, 86400, &errstr);
			if (errstr == NULL)
				delay = v;
		}
		break;
	case 'n':
		if (prompt("number of processes: ", buf, sizeof(buf))) {
			v = (int)strtonum(buf, 0, INT32_MAX, &errstr);
			if (errstr == NULL)
				maxprocs = v;
		}
		break;
	case 'U':
		if (prompt("user (empty for all): ", buf, sizeof(buf))) {
			struct passwd *pw = getpwnam(buf);

			if (pw != NULL) {
				only_uid = pw->pw_uid;
				only_user = true;
			}
		} else
			only_user = false;
		break;
	}
	return (false);
}

/* Wait up to ms milliseconds, running commands; true to quit. */
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
		return (command(c));
	return (false);
}

static int
number(const char *what, const char *arg, long long max)
{
	const char *errstr;
	int v;

	v = (int)strtonum(arg, 0, max, &errstr);
	if (errstr != NULL)
		errx(EX_USAGE, "%s is %s: %s", what, errstr, arg);
	return (v);
}

#define	TOP_OPTS	"ac:deFfhi:l:n:O:o:RrSs:U:u"

/* Whether getopt takes the next word as this option word's argument. */
static bool
takes_next(const char *word)
{
	const char *c, *o;

	if (word[0] != '-' || word[1] == '\0' || strcmp(word, "--") == 0)
		return (false);
	for (c = word + 1; *c != '\0'; c++)
		if ((o = strchr(TOP_OPTS, *c)) != NULL && o[1] == ':')
			return (c[1] == '\0');	/* else attached */
	return (false);
}

/*
 * macOS's single-dash long options; removes them from argv. A word that
 * is another option's argument (-o -pid) is left alone.
 */
static int
long_options(int argc, char *argv[])
{
	int i, j;

	for (i = j = 1; i < argc; i++) {
		const char *o = argv[i];

		if (takes_next(o) && i + 1 < argc) {
			argv[j++] = argv[i++];
			argv[j++] = argv[i];
			continue;
		}
		if (strcmp(o, "--") == 0) {
			while (i < argc)
				argv[j++] = argv[i++];
			break;
		}
		if (strcmp(o, "-pid") == 0 || strcmp(o, "-user") == 0 ||
		    strcmp(o, "-stats") == 0 || strcmp(o, "-ncols") == 0) {
			if (i + 1 >= argc)
				usage(EX_USAGE);
			const char *arg = argv[++i];

			if (o[1] == 'p') {
				only_pids = reallocarray(only_pids,
				    (size_t)nonly_pids + 1, sizeof(pid_t));
				if (only_pids == NULL)
					err(EX_OSERR, "reallocarray");
				only_pids[nonly_pids++] =
				    (pid_t)number("pid", arg, 99999);
			} else if (o[1] == 'u')
				set_user(arg);
			else if (o[1] == 's')
				parse_stats(arg);
			else
				ncols = number("columns", arg, 1000);
			continue;
		}
		argv[j++] = argv[i];
	}
	argv[j] = NULL;
	return (j);
}

int
main(int argc, char *argv[])
{
	struct sample prev, cur;
	int ch, samples = -1;
	bool have_prev = false;

	/* macOS's columns, those Minkar has. */
	parse_stats("pid,command,cpu,time,threads,wq,mem,pgrp,ppid,state,"
	    "uid,faults,cow,msgsent,msgrecv,sysbsd,sysmach,csw,pageins,user");
	argc = long_options(argc, argv);
	while ((ch = getopt(argc, argv, TOP_OPTS)) != -1) {
		switch (ch) {
		case 'a': case 'd': case 'e':
			evmode = (char)ch;
			break;
		case 'c':
			if (strlen(optarg) != 1 ||
			    strchr("adne", optarg[0]) == NULL)
				errx(EX_USAGE, "unknown mode: %s", optarg);
			evmode = optarg[0];
			break;
		case 'F': case 'f':		/* frameworks: not shown */
		case 'R': case 'r':		/* memory object map: idem */
			break;
		case 'h':
			usage(0);
		case 'i':
			(void)number("interval", optarg, INT32_MAX);
			break;
		case 'l':
			samples = number("samples", optarg, INT32_MAX);
			logging = true;
			break;
		case 'n':
			maxprocs = number("nprocs", optarg, INT32_MAX);
			break;
		case 'O':
			if (!parse_sort(optarg, &sortkey2, &sortasc2))
				errx(EX_USAGE, "unknown sort key: %s", optarg);
			break;
		case 'o':
			if (!parse_sort(optarg, &sortkey, &sortasc))
				errx(EX_USAGE, "unknown sort key: %s", optarg);
			break;
		case 'S':
			show_swap = true;
			break;
		case 's':
			delay = number("delay", optarg, 86400);
			break;
		case 'U':
			set_user(optarg);
			break;
		case 'u':
			/* -o cpu -O time */
			sortkey = K_CPU;
			sortasc = false;
			sortkey2 = K_TIME;
			sortasc2 = false;
			break;
		default:
			usage(EX_USAGE);
		}
	}
	if (optind != argc)
		usage(EX_USAGE);
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
		if (!have_first) {
			/* -c a counts from here. */
			first.nprocs = cur.nprocs;
			first.procs = calloc(cur.nprocs + 1,
			    sizeof(struct pinfo));
			if (first.procs != NULL) {
				memcpy(first.procs, cur.procs,
				    cur.nprocs * sizeof(struct pinfo));
				have_first = true;
			}
		}
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
usage(int status)
{

	fprintf(status == 0 ? stdout : stderr,
	    "usage: top [-a | -d | -e | -c mode] [-F | -f] [-h] [-i interval]\n"
	    "           [-l samples] [-ncols columns] [-o key | -O skey]\n"
	    "           [-R | -r] [-S] [-s delay-secs] [-n nprocs]\n"
	    "           [-stats keys] [-pid processid] [-user username]\n"
	    "           [-U username] [-u]\n");
	exit(status);
}
