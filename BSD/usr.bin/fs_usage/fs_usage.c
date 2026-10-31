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
 * fs_usage: file system (and network) activity as it happens, from the
 * kernel's trace (kdebug). The trace is set up for the system calls and
 * the path lookups; each call's start and end are paired by thread, with
 * the path the kernel traced while looking it up, and printed with its
 * time, file descriptor, byte count or error, how long it took and the
 * process. Processes are named from the trace's thread map.
 */

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <sys/kdebug.h>
#include <sys/kdebug_private.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <mach/mach_time.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define NBUFS		200000
#define BSC_BASE	0x040c0000U	/* DBG_BSD, DBG_BSD_EXCP_SC */
#define TS_MASK		0x00ffffffffffffffULL
#define TYPEFILTER_SIZE	8192		/* 256 classes x 256 subclasses */

enum { F_FS = 1, F_NET = 2, F_EXEC = 4, F_PATH = 8 };
enum { A_NONE, A_FD, A_FD_BYTES, A_RET_FD, A_PATH };

/* The calls traced: their number, name, kind and what to show. */
static const struct call {
	int		 num;
	const char	*name;
	int		 kind;
	int		 show;
} calls[] = {
#define C(sys, name, kind, show)	{ sys, name, kind, show },
	C(SYS_read, "read", F_FS, A_FD_BYTES)
	C(SYS_write, "write", F_FS, A_FD_BYTES)
	C(SYS_open, "open", F_FS, A_RET_FD)
	C(SYS_close, "close", F_FS, A_FD)
	C(SYS_link, "link", F_FS, A_NONE)
	C(SYS_unlink, "unlink", F_FS, A_NONE)
	C(SYS_chdir, "chdir", F_FS, A_NONE)
	C(SYS_fchdir, "fchdir", F_FS, A_FD)
	C(SYS_mknod, "mknod", F_FS, A_NONE)
	C(SYS_chmod, "chmod", F_FS, A_NONE)
	C(SYS_chown, "chown", F_FS, A_NONE)
	C(SYS_access, "access", F_FS, A_NONE)
	C(SYS_chflags, "chflags", F_FS, A_NONE)
	C(SYS_fchflags, "fchflags", F_FS, A_FD)
	C(SYS_dup, "dup", F_FS, A_FD)
	C(SYS_symlink, "symlink", F_FS, A_NONE)
	C(SYS_readlink, "readlink", F_FS, A_NONE)
	C(SYS_execve, "execve", F_EXEC, A_NONE)
	C(SYS_umask, "umask", F_FS, A_NONE)
	C(SYS_chroot, "chroot", F_FS, A_NONE)
	C(SYS_dup2, "dup2", F_FS, A_FD)
	C(SYS_fcntl, "fcntl", F_FS, A_FD)
	C(SYS_fsync, "fsync", F_FS, A_FD)
	C(SYS_socket, "socket", F_NET, A_RET_FD)
	C(SYS_connect, "connect", F_NET, A_FD)
	C(SYS_accept, "accept", F_NET, A_RET_FD)
	C(SYS_bind, "bind", F_NET, A_FD)
	C(SYS_listen, "listen", F_NET, A_FD)
	C(SYS_sendto, "sendto", F_NET, A_FD_BYTES)
	C(SYS_recvfrom, "recvfrom", F_NET, A_FD_BYTES)
	C(SYS_sendmsg, "sendmsg", F_NET, A_FD_BYTES)
	C(SYS_recvmsg, "recvmsg", F_NET, A_FD_BYTES)
	C(SYS_readv, "readv", F_FS, A_FD_BYTES)
	C(SYS_writev, "writev", F_FS, A_FD_BYTES)
	C(SYS_fchown, "fchown", F_FS, A_FD)
	C(SYS_fchmod, "fchmod", F_FS, A_FD)
	C(SYS_rename, "rename", F_FS, A_NONE)
	C(SYS_flock, "flock", F_FS, A_FD)
	C(SYS_mkfifo, "mkfifo", F_FS, A_NONE)
	C(SYS_mkdir, "mkdir", F_FS, A_NONE)
	C(SYS_rmdir, "rmdir", F_FS, A_NONE)
	C(SYS_utimes, "utimes", F_FS, A_NONE)
	C(SYS_futimes, "futimes", F_FS, A_FD)
	C(SYS_pread, "pread", F_FS, A_FD_BYTES)
	C(SYS_pwrite, "pwrite", F_FS, A_FD_BYTES)
	C(SYS_statfs, "statfs", F_FS, A_NONE)
	C(SYS_fstatfs, "fstatfs", F_FS, A_FD)
	C(SYS_lseek, "lseek", F_FS, A_FD)
	C(SYS_truncate, "truncate", F_FS, A_NONE)
	C(SYS_ftruncate, "ftruncate", F_FS, A_FD)
	C(SYS_getattrlist, "getattrlist", F_FS, A_NONE)
	C(SYS_setattrlist, "setattrlist", F_FS, A_NONE)
	C(SYS_getdirentriesattr, "getdirentriesattr", F_FS, A_FD)
	C(SYS_exchangedata, "exchangedata", F_FS, A_NONE)
	C(SYS_searchfs, "searchfs", F_FS, A_NONE)
	C(SYS_fgetattrlist, "fgetattrlist", F_FS, A_FD)
	C(SYS_fsetattrlist, "fsetattrlist", F_FS, A_FD)
	C(SYS_getxattr, "getxattr", F_FS, A_NONE)
	C(SYS_fgetxattr, "fgetxattr", F_FS, A_FD)
	C(SYS_setxattr, "setxattr", F_FS, A_NONE)
	C(SYS_fsetxattr, "fsetxattr", F_FS, A_FD)
	C(SYS_removexattr, "removexattr", F_FS, A_NONE)
	C(SYS_listxattr, "listxattr", F_FS, A_NONE)
	C(SYS_posix_spawn, "posix_spawn", F_EXEC, A_NONE)
	C(SYS_stat64, "stat64", F_FS, A_NONE)
	C(SYS_fstat64, "fstat64", F_FS, A_FD)
	C(SYS_lstat64, "lstat64", F_FS, A_NONE)
	C(SYS_getdirentries64, "getdirentries64", F_FS, A_FD_BYTES)
	C(SYS_statfs64, "statfs64", F_FS, A_NONE)
	C(SYS_fstatfs64, "fstatfs64", F_FS, A_FD)
	C(SYS_getfsstat64, "getfsstat64", F_FS, A_NONE)
	C(SYS_read_nocancel, "read", F_FS, A_FD_BYTES)
	C(SYS_write_nocancel, "write", F_FS, A_FD_BYTES)
	C(SYS_open_nocancel, "open", F_FS, A_RET_FD)
	C(SYS_close_nocancel, "close", F_FS, A_FD)
	C(SYS_fcntl_nocancel, "fcntl", F_FS, A_FD)
	C(SYS_fsync_nocancel, "fsync", F_FS, A_FD)
	C(SYS_pread_nocancel, "pread", F_FS, A_FD_BYTES)
	C(SYS_pwrite_nocancel, "pwrite", F_FS, A_FD_BYTES)
	C(SYS_openat, "openat", F_FS, A_RET_FD)
	C(SYS_openat_nocancel, "openat", F_FS, A_RET_FD)
	C(SYS_renameat, "renameat", F_FS, A_NONE)
	C(SYS_faccessat, "faccessat", F_FS, A_NONE)
	C(SYS_fchmodat, "fchmodat", F_FS, A_NONE)
	C(SYS_fchownat, "fchownat", F_FS, A_NONE)
	C(SYS_fstatat64, "fstatat64", F_FS, A_NONE)
	C(SYS_linkat, "linkat", F_FS, A_NONE)
	C(SYS_unlinkat, "unlinkat", F_FS, A_NONE)
	C(SYS_readlinkat, "readlinkat", F_FS, A_NONE)
	C(SYS_symlinkat, "symlinkat", F_FS, A_NONE)
	C(SYS_mkdirat, "mkdirat", F_FS, A_NONE)
	C(SYS_getattrlistat, "getattrlistat", F_FS, A_NONE)
#undef C
};
#define NCALLS	(sizeof(calls) / sizeof(calls[0]))

/* A call in progress on a thread. */
struct pending {
	uint64_t		 thread;
	const struct call	*call;
	uint64_t		 start;
	uint64_t		 arg1;
	char			 path[MAXPATHLEN];
	size_t			 pathlen;
	bool			 lookup;	/* a path lookup in progress */
	struct pending		*next;
};

struct thread_name {
	uint64_t		 thread;
	int			 pid;
	char			 command[20];
};

static int kinds = F_FS | F_NET;
static bool wide;
static pid_t *pids, *excluded;
static int npids, nexcluded;
static char **commands, **excluded_cmds;
static int ncommands, nexcluded_cmds;
static struct pending *pending;
static struct thread_name *threads;
static int nthreads, capthreads;
static mach_timebase_info_data_t timebase;
static uint64_t base_abs;
static struct timeval base_tv;
static volatile sig_atomic_t done;

/* ---- The trace --------------------------------------------------------------- */

static void
kd(int op, int value, void *buf, size_t *len, void *newp, size_t newlen)
{
	int mib[4] = { CTL_KERN, KERN_KDEBUG, op, value };

	if (sysctl(mib, value >= 0 ? 4 : 3, buf, len, newp, newlen) != 0) {
		if ((errno == EPERM || errno == EACCES) && geteuid() != 0)
			errx(1, "the kernel's trace needs root");
		if (errno == EBUSY)
			errx(1, "the kernel's trace is in use by another program");
		err(1, "kdebug %d", op);
	}
}

static void
trace_off(void)
{
	int mib[4] = { CTL_KERN, KERN_KDEBUG, KERN_KDENABLE, 0 };
	size_t len = 0;

	sysctl(mib, 4, NULL, &len, NULL, 0);
	mib[2] = KERN_KDREMOVE;
	sysctl(mib, 3, NULL, &len, NULL, 0);
}

static void
on_signal(int sig)
{
	(void)sig;
	done = 1;
}

static void
trace_on(void)
{
	uint8_t filter[TYPEFILTER_SIZE];
	size_t len = 0;

	trace_off();
	kd(KERN_KDSETBUF, NBUFS, NULL, &len, NULL, 0);
	kd(KERN_KDSETUP, -1, NULL, &len, NULL, 0);
	memset(filter, 0, sizeof(filter));
	/* class.subclass: DBG_BSD.DBG_BSD_EXCP_SC and DBG_FSYSTEM.DBG_FSRW */
	filter[(DBG_BSD << 8 | DBG_BSD_EXCP_SC) / 8] |=
	    (uint8_t)(1 << ((DBG_BSD << 8 | DBG_BSD_EXCP_SC) % 8));
	filter[(DBG_FSYSTEM << 8 | DBG_FSRW) / 8] |=
	    (uint8_t)(1 << ((DBG_FSYSTEM << 8 | DBG_FSRW) % 8));
	/* DBG_TRACE: new threads and execs name the processes. */
	memset(filter + (DBG_TRACE << 8) / 8, 0xff, 256 / 8);
	/* The node is read-only: the kernel copies the filter in from "old". */
	len = sizeof(filter);
	kd(KERN_KDSET_TYPEFILTER, -1, filter, &len, NULL, 0);
	len = 0;
	kd(KERN_KDENABLE, KDEBUG_ENABLE_TRACE, NULL, &len, NULL, 0);
}

/* A thread's entry, added when new; names learned are kept after exit. */
static struct thread_name *
thread_entry(uint64_t thread)
{
	int i;

	for (i = 0; i < nthreads; i++)
		if (threads[i].thread == thread)
			return (&threads[i]);
	if (nthreads == capthreads) {
		capthreads = capthreads ? capthreads * 2 : 1024;
		threads = realloc(threads, sizeof(*threads) * (size_t)capthreads);
		if (threads == NULL)
			err(1, NULL);
	}
	memset(&threads[nthreads], 0, sizeof(threads[nthreads]));
	threads[nthreads].thread = thread;
	return (&threads[nthreads++]);
}

/* The current thread map: which process each thread is. */
static void
read_thread_map(void)
{
	size_t len = 0;
	int mib[3] = { CTL_KERN, KERN_KDEBUG, KERN_KDREADCURTHRMAP };
	kd_threadmap *map;
	int i, n;

	if (sysctl(mib, 3, NULL, &len, NULL, 0) != 0 || len == 0)
		return;
	len += len / 2;
	if ((map = malloc(len)) == NULL)
		return;
	if (sysctl(mib, 3, map, &len, NULL, 0) != 0) {
		free(map);
		return;
	}
	n = (int)(len / sizeof(*map));
	for (i = 0; i < n; i++) {
		struct thread_name *t;

		if (map[i].valid == 0)
			continue;
		t = thread_entry(map[i].thread);
		t->pid = map[i].valid;
		strlcpy(t->command, map[i].command, sizeof(t->command));
	}
	free(map);
}

static const struct thread_name *
thread_name(uint64_t thread)
{
	int i;

	for (i = 0; i < nthreads; i++)
		if (threads[i].thread == thread && threads[i].pid != 0)
			return (&threads[i]);
	return (NULL);
}

/* A name from an event's four arguments. */
static void
event_string(const kd_buf *e, char *buf, size_t len)
{
	uint64_t a[4] = { e->arg1, e->arg2, e->arg3, e->arg4 };
	char tmp[sizeof(a) + 1];
	const char *base;

	memcpy(tmp, a, sizeof(a));
	tmp[sizeof(a)] = '\0';
	base = strrchr(tmp, '/') ? strrchr(tmp, '/') + 1 : tmp;
	strlcpy(buf, base, len);
}

/* ---- Filters ---------------------------------------------------------------- */

static bool
wanted(const struct thread_name *t)
{
	int i;

	if (t == NULL)
		return (npids == 0 && ncommands == 0);
	if (t->pid == getpid())
		return (false);
	for (i = 0; i < nexcluded; i++)
		if (t->pid == excluded[i])
			return (false);
	for (i = 0; i < nexcluded_cmds; i++)
		if (strcmp(t->command, excluded_cmds[i]) == 0)
			return (false);
	if (npids == 0 && ncommands == 0)
		return (true);
	for (i = 0; i < npids; i++)
		if (t->pid == pids[i])
			return (true);
	for (i = 0; i < ncommands; i++)
		if (strcmp(t->command, commands[i]) == 0)
			return (true);
	return (false);
}

/* ---- Events ----------------------------------------------------------------- */

static const struct call *
find_call(uint32_t code)
{
	size_t i;
	int num = (int)((code - BSC_BASE) >> 2);

	for (i = 0; i < NCALLS; i++)
		if (calls[i].num == num)
			return (&calls[i]);
	return (NULL);
}

static struct pending *
find_pending(uint64_t thread, bool remove)
{
	struct pending **pp, *p;

	for (pp = &pending; (p = *pp) != NULL; pp = &p->next)
		if (p->thread == thread) {
			if (remove)
				*pp = p->next;
			return (p);
		}
	return (NULL);
}

static double
seconds(uint64_t abs)
{
	return ((double)abs * timebase.numer / timebase.denom / 1e9);
}

static void
print_call(struct pending *p, uint64_t end, uint64_t err_code, uint64_t ret)
{
	const struct thread_name *t = thread_name(p->thread);
	char when[32], info[64] = "", proc[48];
	double elapsed = seconds(end - p->start);
	double at = seconds(p->start - base_abs);
	time_t secs = base_tv.tv_sec + (time_t)at;
	long usecs = (long)((at - (double)(long)at) * 1e6) + base_tv.tv_usec;
	struct tm tm;

	if (!wanted(t))
		return;
	if ((kinds & F_PATH) && p->pathlen == 0)
		return;
	if (usecs >= 1000000) {
		secs++;
		usecs -= 1000000;
	}
	localtime_r(&secs, &tm);
	strftime(when, sizeof(when), "%H:%M:%S", &tm);
	if (wide)
		snprintf(when + strlen(when), sizeof(when) - strlen(when),
		    ".%06ld", usecs);
	if (err_code != 0) {
		snprintf(info, sizeof(info), "[%3d]", (int)err_code);
	} else {
		switch (p->call->show) {
		case A_FD:
			snprintf(info, sizeof(info), "F=%-3d", (int)p->arg1);
			break;
		case A_FD_BYTES:
			snprintf(info, sizeof(info), "F=%-3d B=0x%llx",
			    (int)p->arg1, (unsigned long long)ret);
			break;
		case A_RET_FD:
			snprintf(info, sizeof(info), "F=%-3d", (int)ret);
			break;
		}
	}
	if (t != NULL)
		snprintf(proc, sizeof(proc), "%s.%d", t->command, t->pid);
	else
		snprintf(proc, sizeof(proc), "%llx",
		    (unsigned long long)p->thread);
	p->path[p->pathlen] = '\0';
	/* The kernel traces the path from the root without its slash. */
	if (p->pathlen > 0 && p->path[0] != '/' && p->pathlen + 1 <
	    sizeof(p->path)) {
		memmove(p->path + 1, p->path, p->pathlen + 1);
		p->path[0] = '/';
		p->pathlen++;
	}
	if (wide)
		printf("%-15s  %-17s %-20s %-60s %8.6f   %s\n", when,
		    p->call->name, info, p->path, elapsed, proc);
	else
		printf("%-8s  %-12s %-14s %-24.24s %8.6f   %s\n", when,
		    p->call->name, info, p->path, elapsed, proc);
}

/* The path chunks of a lookup: 24 bytes in the first event, 32 after. */
static void
add_path(struct pending *p, const kd_buf *e, bool first)
{
	uint64_t a[4] = { e->arg1, e->arg2, e->arg3, e->arg4 };
	int i;

	/* A new lookup in the same call (through a symbolic link, say):
	 * the last one is the path resolved. */
	if (first)
		p->pathlen = 0;
	for (i = first ? 1 : 0; i < 4; i++) {
		const char *c = (const char *)&a[i];
		size_t j;

		for (j = 0; j < sizeof(a[i]) && c[j] != '\0' &&
		    p->pathlen + 1 < sizeof(p->path); j++)
			p->path[p->pathlen++] = c[j];
	}
}

static void
event(const kd_buf *e)
{
	uint32_t code = e->debugid & KDBG_EVENTID_MASK;
	uint32_t func = e->debugid & (DBG_FUNC_START | DBG_FUNC_END);
	uint64_t ts = e->timestamp & TS_MASK;
	uint64_t thread = e->arg5;
	struct pending *p;

	if (code == (TRACE_DATA_NEWTHREAD & KDBG_EVENTID_MASK)) {
		struct thread_name *child = thread_entry(e->arg1);
		const struct thread_name *parent = thread_name(thread);

		child->pid = (int)e->arg2;
		if (parent != NULL && child->command[0] == '\0')
			strlcpy(child->command, parent->command,
			    sizeof(child->command));
		return;
	}
	if (code == (TRACE_DATA_EXEC & KDBG_EVENTID_MASK)) {
		thread_entry(thread)->pid = (int)e->arg1;
		return;
	}
	if (code == (TRACE_STRING_EXEC & KDBG_EVENTID_MASK)) {
		struct thread_name *t = thread_entry(thread);

		event_string(e, t->command, sizeof(t->command));
		return;
	}
	if ((code & 0xffff0000U) == BSC_BASE) {
		const struct call *c = find_call(code);

		if (c == NULL || !(c->kind & (kinds & ~F_PATH)))
			return;
		if (func == DBG_FUNC_START) {
			free(find_pending(thread, true));
			if ((p = calloc(1, sizeof(*p))) == NULL)
				return;
			p->thread = thread;
			p->call = c;
			p->start = ts;
			p->arg1 = e->arg1;
			p->next = pending;
			pending = p;
		} else if (func == DBG_FUNC_END &&
		    (p = find_pending(thread, true)) != NULL) {
			if (p->call == c)
				print_call(p, ts, e->arg1, e->arg2);
			free(p);
		}
		return;
	}
	if (code == (VFS_LOOKUP & KDBG_EVENTID_MASK) &&
	    (p = find_pending(thread, false)) != NULL) {
		if (func & DBG_FUNC_START) {
			p->lookup = true;
			add_path(p, e, true);
		} else if (p->lookup) {
			add_path(p, e, false);
		}
		if (func & DBG_FUNC_END)
			p->lookup = false;
	}
}

/* ---- Options ------------------------------------------------------------------ */

static void
usage(void)
{
	fprintf(stderr, "usage: fs_usage [-e] [-w] [-f mode] [-t seconds] "
	    "[pid | cmd [pid | cmd] ...]\n"
	    "  -e    exclude the processes named (and fs_usage itself)\n"
	    "  -w    wide output\n"
	    "  -f    filesys, network, pathname or exec\n"
	    "  -t    stop after that many seconds\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	kd_buf *buf;
	bool exclude = false;
	int ch, timeout = 0, i, nmodes = 0;
	time_t until = 0;

	while ((ch = getopt(argc, argv, "ewf:t:bR:S:E:")) != -1) {
		switch (ch) {
		case 'e':
			exclude = true;
			break;
		case 'w':
			wide = true;
			break;
		case 'f':
			if (nmodes++ == 0)
				kinds = 0;
			if (strcmp(optarg, "filesys") == 0)
				kinds |= F_FS;
			else if (strcmp(optarg, "network") == 0)
				kinds |= F_NET;
			else if (strcmp(optarg, "exec") == 0)
				kinds |= F_EXEC;
			else if (strcmp(optarg, "pathname") == 0)
				kinds |= F_FS | F_PATH;
			else if (strcmp(optarg, "diskio") == 0 ||
			    strcmp(optarg, "cachehit") == 0)
				kinds |= F_FS;
			else
				usage();
			break;
		case 't':
			timeout = atoi(optarg);
			if (timeout <= 0)
				usage();
			break;
		case 'b': case 'R': case 'S': case 'E':
			errx(1, "-%c is not supported", ch);
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	for (i = 0; i < argc; i++) {
		char *end;
		long v = strtol(argv[i], &end, 10);

		if (*end == '\0' && v > 0) {
			pid_t **list = exclude ? &excluded : &pids;
			int *n = exclude ? &nexcluded : &npids;

			*list = realloc(*list, sizeof(pid_t) * (size_t)(*n + 1));
			(*list)[(*n)++] = (pid_t)v;
		} else {
			char ***list = exclude ? &excluded_cmds : &commands;
			int *n = exclude ? &nexcluded_cmds : &ncommands;

			*list = realloc(*list, sizeof(char *) * (size_t)(*n + 1));
			(*list)[(*n)++] = argv[i];
		}
	}
	if (geteuid() != 0)
		errx(1, "the kernel's trace needs root");

	mach_timebase_info(&timebase);
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	signal(SIGHUP, on_signal);
	signal(SIGQUIT, on_signal);
	setvbuf(stdout, NULL, _IOLBF, 0);
	if ((buf = malloc(NBUFS * sizeof(*buf))) == NULL)
		err(1, NULL);
	trace_on();
	gettimeofday(&base_tv, NULL);
	base_abs = mach_absolute_time();
	read_thread_map();
	if (timeout > 0)
		until = time(NULL) + timeout;

	while (!done && (until == 0 || time(NULL) < until)) {
		size_t len = NBUFS * sizeof(*buf);
		int mib[3] = { CTL_KERN, KERN_KDEBUG, KERN_KDREADTR };
		size_t n, j;

		usleep(100000);
		if (sysctl(mib, 3, buf, &len, NULL, 0) != 0) {
			warn("reading the trace");
			break;
		}
		n = len;	/* the number of events */
		if (n > 0)
			read_thread_map();	/* for threads new since */
		for (j = 0; j < n; j++)
			event(&buf[j]);
	}
	trace_off();
	return (0);
}
