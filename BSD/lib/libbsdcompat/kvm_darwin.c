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
 * FreeBSD's kvm_getprocs() and friends on Darwin (see kvm_darwin.h).
 *
 * A process's run state is worked out from its threads, as macOS's
 * ps(1) manual describes it: R if a thread can run, U if one is in an
 * uninterruptible wait, S if one has slept for less than about 20
 * seconds, I if all have slept longer, T if all are stopped. %CPU is the
 * sum of the threads' recent CPU usage.
 */

#define	_MINKAR_KVM_IMPL
#include <kvm_darwin.h>

#include <sys/proc_info.h>
#include <mach/mach_time.h>
#include <mach/thread_info.h>

#include <errno.h>
#include <libproc.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	MAXSLEEP	20	/* seconds: S below, I above */

struct minkar_kvm {
	struct minkar_kinfo	*procs;
	char			**argv;
	char			**envv;
	char			*argbuf;
	char			err[128];
};

kvm_t *
kvm_openfiles(const char *execf, const char *corefile, const char *swapf,
    int flags, char *errbuf)
{
	kvm_t *kd;

	(void)execf;
	(void)corefile;
	(void)swapf;
	(void)flags;
	if ((kd = calloc(1, sizeof(*kd))) == NULL && errbuf != NULL)
		strlcpy(errbuf, strerror(errno), _POSIX2_LINE_MAX);
	return (kd);
}

int
kvm_close(kvm_t *kd)
{
	free(kd->procs);
	free(kd->argv);
	free(kd->envv);
	free(kd->argbuf);
	free(kd);
	return (0);
}

char *
kvm_geterr(kvm_t *kd)
{
	return (kd->err);
}

static uint64_t
abs_to_usec(uint64_t t)
{
	static mach_timebase_info_data_t tb;

	if (tb.denom == 0)
		mach_timebase_info(&tb);
	return (t * tb.numer / tb.denom / 1000);
}

static void
usec_to_tv(uint64_t usec, struct timeval *tv)
{
	tv->tv_sec = (time_t)(usec / 1000000);
	tv->tv_usec = (suseconds_t)(usec % 1000000);
}

static void
fill_base(struct minkar_kinfo *ki, const struct kinfo_proc *kp)
{
	const struct extern_proc *p = &kp->kp_proc;
	const struct eproc *e = &kp->kp_eproc;
	int i;

	memset(ki, 0, sizeof(*ki));
	ki->ki_pid = p->p_pid;
	ki->ki_ppid = e->e_ppid;
	ki->ki_pgid = e->e_pgid;
	ki->ki_tpgid = e->e_tpgid;
	ki->ki_sid = getsid(ki->ki_pid);
	if (ki->ki_sid == -1)
		ki->ki_sid = 0;
	if (ki->ki_tpgid > 0 && (ki->ki_tsid = getsid(ki->ki_tpgid)) == -1)
		ki->ki_tsid = 0;
	ki->ki_jobc = e->e_jobc;
	ki->ki_tdev = e->e_tdev;
	ki->ki_uid = e->e_ucred.cr_uid;
	ki->ki_ruid = e->e_pcred.p_ruid;
	ki->ki_svuid = e->e_pcred.p_svuid;
	ki->ki_rgid = e->e_pcred.p_rgid;
	ki->ki_svgid = e->e_pcred.p_svgid;
	ki->ki_ngroups = e->e_ucred.cr_ngroups;
	if (ki->ki_ngroups > KI_NGROUPS)
		ki->ki_ngroups = KI_NGROUPS;
	for (i = 0; i < ki->ki_ngroups; i++)
		ki->ki_groups[i] = e->e_ucred.cr_groups[i];
	ki->ki_tsize = e->e_xsize;
	ki->ki_xstat = p->p_xstat;
	ki->ki_acflag = p->p_acflag;
	ki->ki_estcpu = p->p_estcpu;
	ki->ki_slptime = p->p_slptime;
	ki->ki_swtime = p->p_swtime;
	ki->ki_start = p->p_starttime;
	/* Unsigned: P_* uses the top bit, ps prints the flags in hex. */
	ki->ki_flag = (long)(u_int)p->p_flag;
	/* The kernel (pid 0) is the process pgrep -S is about. */
	if (ki->ki_pid == 0)
		ki->ki_flag |= P_SYSTEM;
	if (e->e_flag & EPROC_CTTY)
		ki->ki_kiflag |= KI_CTTY;
	if (e->e_flag & EPROC_SLEADER)
		ki->ki_kiflag |= KI_SLEADER;
	ki->ki_traceflag = p->p_traceflag;
	ki->ki_stat = p->p_stat;
	ki->ki_nice = p->p_nice;
	ki->ki_oncpu = NOCPU;
	ki->ki_lastcpu = NOCPU;
	ki->ki_pri.pri_level = p->p_priority;
	ki->ki_pri.pri_user = p->p_usrpri;
	ki->ki_numthreads = 1;
	ki->ki_siglist = (sigset_t)p->p_siglist;
	ki->ki_sigmask = p->p_sigmask;
	ki->ki_sigignore = p->p_sigignore;
	ki->ki_sigcatch = p->p_sigcatch;
	ki->ki_wchan = p->p_wchan;
	ki->ki_paddr = e->e_paddr;
	ki->ki_tracep = p->p_tracep;
	ki->ki_ru = p->p_ru;
	ki->ki_sess = e->e_sess;
	ki->ki_tsess = e->e_tsess;
	strlcpy(ki->ki_wmesg, e->e_wmesg, sizeof(ki->ki_wmesg));
	strlcpy(ki->ki_login, e->e_login, sizeof(ki->ki_login));
	strlcpy(ki->ki_comm, p->p_comm, sizeof(ki->ki_comm));
	strlcpy(ki->ki_loginclass, "default", sizeof(ki->ki_loginclass));
}

/* Sizes, times and counts from the task (only what the caller may see). */
static void
fill_task(struct minkar_kinfo *ki)
{
	struct proc_taskallinfo ti;
	struct proc_workqueueinfo wq;
	struct rusage_info_v2 ri;
	uint64_t user, sys;

	if (proc_pidinfo(ki->ki_pid, PROC_PIDTASKALLINFO, 0, &ti,
	    sizeof(ti)) == (int)sizeof(ti)) {
		ki->ki_size = (vm_size_t)ti.ptinfo.pti_virtual_size;
		ki->ki_rssize = (long)(ti.ptinfo.pti_resident_size /
		    (uint64_t)getpagesize());
		user = abs_to_usec(ti.ptinfo.pti_total_user);
		sys = abs_to_usec(ti.ptinfo.pti_total_system);
		usec_to_tv(user, &ki->ki_rusage.ru_utime);
		usec_to_tv(sys, &ki->ki_rusage.ru_stime);
		ki->ki_runtime = user + sys;
		ki->ki_rusage.ru_minflt = ti.ptinfo.pti_faults;
		ki->ki_rusage.ru_majflt = ti.ptinfo.pti_pageins;
		ki->ki_rusage.ru_nvcsw = ti.ptinfo.pti_csw;
		ki->ki_rusage.ru_msgsnd = ti.ptinfo.pti_messages_sent;
		ki->ki_rusage.ru_msgrcv = ti.ptinfo.pti_messages_received;
		ki->ki_pri.pri_level = (u_char)ti.ptinfo.pti_priority;
		ki->ki_numthreads = ti.ptinfo.pti_threadnum;
	}
	if (proc_pidinfo(ki->ki_pid, PROC_PIDWORKQUEUEINFO, 0, &wq,
	    sizeof(wq)) == (int)sizeof(wq)) {
		ki->ki_wq_threads = wq.pwq_nthreads;
		ki->ki_wq_running = wq.pwq_runthreads;
		ki->ki_wq_blocked = wq.pwq_blockedthreads;
		ki->ki_wq_state = wq.pwq_state;
	}
	if (proc_pid_rusage(ki->ki_pid, RUSAGE_INFO_V2,
	    (rusage_info_t *)&ri) == 0) {
		user = abs_to_usec(ri.ri_child_user_time);
		sys = abs_to_usec(ri.ri_child_system_time);
		usec_to_tv(user, &ki->ki_childutime);
		usec_to_tv(sys, &ki->ki_childstime);
		usec_to_tv(user + sys, &ki->ki_childtime);
	}
}

/* A thread's state letter and its rank: the process takes the lowest. */
static char
thread_state(const struct proc_threadinfo *th, int *rank)
{
	switch (th->pth_run_state) {
	case TH_STATE_RUNNING:
		*rank = 1;
		return ('R');
	case TH_STATE_UNINTERRUPTIBLE:
		*rank = 2;
		return ('U');
	case TH_STATE_WAITING:
		if (th->pth_sleep_time <= MAXSLEEP) {
			*rank = 3;
			return ('S');
		}
		*rank = 4;
		return ('I');
	case TH_STATE_STOPPED:
		*rank = 5;
		return ('T');
	default:
		*rank = 6;
		return ('?');
	}
}

static void
fill_thread(struct minkar_kinfo *ki, const struct proc_threadinfo *th,
    int index)
{
	uint64_t user, sys;
	int rank;

	ki->ki_tid = index;
	strlcpy(ki->ki_tdname, th->pth_name, sizeof(ki->ki_tdname));
	ki->ki_pctcpu = (fixpt_t)((uint64_t)th->pth_cpu_usage * FSCALE /
	    TH_USAGE_SCALE);
	user = abs_to_usec(th->pth_user_time);
	sys = abs_to_usec(th->pth_system_time);
	usec_to_tv(user, &ki->ki_rusage.ru_utime);
	usec_to_tv(sys, &ki->ki_rusage.ru_stime);
	ki->ki_runtime = user + sys;
	ki->ki_pri.pri_level = (u_char)th->pth_curpri;
	ki->ki_runstate = thread_state(th, &rank);
	ki->ki_slptime = th->pth_sleep_time;
}

/*
 * The process's threads: its state, %CPU and sleep time; with
 * KERN_PROC_INC_THREAD, also one record per thread after *out. Returns
 * how many thread records were added (0 or more).
 */
static int
fill_threads(struct minkar_kinfo *ki, int perthread,
    struct minkar_kinfo **out, size_t *room, size_t *used)
{
	struct proc_threadinfo th;
	uint64_t *handles;
	size_t size;
	int n, i, rank, best, added;
	uint64_t usage;
	u_int slept;
	char st;

	size = (size_t)(ki->ki_numthreads + 32) * sizeof(uint64_t);
	if ((handles = malloc(size)) == NULL)
		return (0);
	n = proc_pidinfo(ki->ki_pid, PROC_PIDLISTTHREADS, 0, handles,
	    (int)size);
	n = n > 0 ? n / (int)sizeof(uint64_t) : 0;
	best = INT_MAX;
	usage = 0;
	slept = UINT_MAX;
	added = 0;
	for (i = 0; i < n; i++) {
		if (proc_pidinfo(ki->ki_pid, PROC_PIDTHREADINFO, handles[i],
		    &th, sizeof(th)) != (int)sizeof(th))
			continue;
		usage += (uint64_t)th.pth_cpu_usage;
		st = thread_state(&th, &rank);
		if (rank < best) {
			best = rank;
			ki->ki_runstate = st;
		}
		if (th.pth_run_state == TH_STATE_WAITING &&
		    (u_int)th.pth_sleep_time < slept)
			slept = (u_int)th.pth_sleep_time;
		if (perthread) {
			if (*used == *room) {
				struct minkar_kinfo *grown;

				grown = realloc(*out,
				    (*room * 2 + 16) * sizeof(**out));
				if (grown == NULL)
					break;
				*out = grown;
				*room = *room * 2 + 16;
			}
			/* A thread starts as a copy of its process. */
			(*out)[*used] = *ki;
			fill_thread(&(*out)[*used], &th, i + 1);
			(*used)++;
			added++;
		}
	}
	free(handles);
	if (n > 0) {
		ki->ki_pctcpu = (fixpt_t)(usage * FSCALE / TH_USAGE_SCALE);
		ki->ki_slptime = slept == UINT_MAX ? 0 : slept;
	}
	return (added);
}

struct minkar_kinfo *
kvm_getprocs(kvm_t *kd, int op, int arg, int *cnt)
{
	int what = op & ~KERN_PROC_INC_THREAD;
	int perthread = (op & KERN_PROC_INC_THREAD) != 0;
	int mib[4] = { CTL_KERN, KERN_PROC, what, arg };
	struct kinfo_proc *kp = NULL;
	struct minkar_kinfo *out = NULL, proc;
	size_t len = 0, room, used;
	int i, n, tries;

	*cnt = 0;
	free(kd->procs);
	kd->procs = NULL;
	/* The table can grow between the size query and the read. */
	for (tries = 0; tries < 8; tries++) {
		if (sysctl(mib, what == KERN_PROC_ALL ? 3 : 4, NULL, &len,
		    NULL, 0) == -1)
			goto fail;
		len += len / 8 + sizeof(*kp);
		free(kp);
		if ((kp = malloc(len)) == NULL)
			goto fail;
		if (sysctl(mib, what == KERN_PROC_ALL ? 3 : 4, kp, &len,
		    NULL, 0) == 0)
			break;
		if (errno != ENOMEM)
			goto fail;
	}
	if (tries == 8)
		goto fail;
	n = (int)(len / sizeof(*kp));
	room = (size_t)(n > 0 ? n : 1);
	used = 0;
	if ((out = calloc(room, sizeof(*out))) == NULL)
		goto fail;
	for (i = 0; i < n; i++) {
		fill_base(&proc, &kp[i]);
		if (proc.ki_stat != SZOMB)
			fill_task(&proc);
		if (perthread && proc.ki_stat != SZOMB) {
			if (fill_threads(&proc, 1, &out, &room, &used) > 0)
				continue;
		} else if (proc.ki_stat != SZOMB)
			fill_threads(&proc, 0, NULL, NULL, NULL);
		if (used == room) {
			struct minkar_kinfo *grown;

			if ((grown = realloc(out,
			    (room * 2) * sizeof(*out))) == NULL)
				goto fail;
			out = grown;
			room *= 2;
		}
		out[used++] = proc;
	}
	free(kp);
	kd->procs = out;
	*cnt = (int)used;
	return (kd->procs);
fail:
	strlcpy(kd->err, strerror(errno), sizeof(kd->err));
	free(kp);
	free(out);
	return (NULL);
}

/*
 * KERN_PROCARGS2 gives argc, the executable path, NUL padding, then argc
 * NUL-terminated arguments, then the environment, then more strings
 * (the "apple" ones) after an empty one. Returns the length read.
 */
static size_t
read_procargs(kvm_t *kd, pid_t pid)
{
	int mib[3] = { CTL_KERN, KERN_PROCARGS2, pid };
	size_t len;
	int argmax;

	len = sizeof(argmax);
	if (sysctlbyname("kern.argmax", &argmax, &len, NULL, 0) == -1)
		return (0);
	free(kd->argbuf);
	if ((kd->argbuf = malloc((size_t)argmax)) == NULL)
		return (0);
	len = (size_t)argmax;
	if (sysctl(mib, 3, kd->argbuf, &len, NULL, 0) == -1 ||
	    len <= sizeof(int))
		return (0);
	return (len);
}

/* Splits the strings after the path: argv first, then the environment. */
static char **
split_procargs(kvm_t *kd, size_t len, int env)
{
	char *p, *end, **list;
	int argc, i, n;

	memcpy(&argc, kd->argbuf, sizeof(argc));
	p = kd->argbuf + sizeof(int);
	end = kd->argbuf + len;
	while (p < end && *p != '\0')		/* executable path */
		p++;
	while (p < end && *p == '\0')		/* padding */
		p++;
	if (argc <= 0)
		return (NULL);
	if (env) {
		for (i = 0; i < argc && p < end; i++)
			p += strnlen(p, (size_t)(end - p)) + 1;
		/* Count the environment, up to the empty string. */
		n = 0;
		for (char *q = p; q < end && *q != '\0';
		    q += strnlen(q, (size_t)(end - q)) + 1)
			n++;
	} else
		n = argc;
	if ((list = calloc((size_t)n + 1, sizeof(char *))) == NULL)
		return (NULL);
	for (i = 0; i < n && p < end && (env == 0 || *p != '\0'); i++) {
		list[i] = p;
		p += strnlen(p, (size_t)(end - p)) + 1;
	}
	if (p > end || (env == 0 && i == 0)) {
		free(list);
		return (NULL);
	}
	list[i] = NULL;
	return (list);
}

char **
kvm_getargv(kvm_t *kd, const struct minkar_kinfo *ki, int nchr)
{
	size_t len;

	(void)nchr;
	free(kd->argv);
	kd->argv = NULL;
	if ((len = read_procargs(kd, ki->ki_pid)) == 0)
		return (NULL);
	kd->argv = split_procargs(kd, len, 0);
	return (kd->argv);
}

char **
kvm_getenvv(kvm_t *kd, const struct minkar_kinfo *ki, int nchr)
{
	size_t len;

	(void)nchr;
	free(kd->envv);
	kd->envv = NULL;
	if ((len = read_procargs(kd, ki->ki_pid)) == 0)
		return (NULL);
	kd->envv = split_procargs(kd, len, 1);
	return (kd->envv);
}

const char *
kvm_getcomm(kvm_t *kd, const struct minkar_kinfo *ki, char *buf, size_t len)
{
	size_t size;
	char *path, *base;

	strlcpy(buf, ki->ki_comm, len);
	/* kd->argv may point into argbuf: drop it before re-reading. */
	free(kd->argv);
	kd->argv = NULL;
	free(kd->envv);
	kd->envv = NULL;
	if ((size = read_procargs(kd, ki->ki_pid)) == 0)
		return (buf);
	path = kd->argbuf + sizeof(int);
	if (strnlen(path, size - sizeof(int)) == size - sizeof(int) ||
	    *path == '\0')
		return (buf);
	base = strrchr(path, '/');
	strlcpy(buf, base != NULL ? base + 1 : path, len);
	return (buf);
}

int
jail_getid(const char *name)
{
	char *end;
	long jid;

	/* Only jail 0, the host, exists. */
	jid = strtol(name, &end, 10);
	if (*name != '\0' && *end == '\0' && jid == 0)
		return (0);
	errno = ENOENT;
	return (-1);
}
