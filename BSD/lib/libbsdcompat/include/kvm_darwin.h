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
 * The part of FreeBSD's libkvm and libjail that pkill(1), killall(1),
 * w(1) and ps(1) use, on Darwin (kvm_darwin.c, in libbsdcompat).
 * Processes come from sysctl(KERN_PROC_*), arguments and environment
 * from KERN_PROCARGS2, and sizes, times, threads and run states from
 * libproc; each becomes a FreeBSD-shaped record with the ki_* fields
 * those commands read. What Darwin does not keep is zero. Darwin has no
 * jails (every process is in jail 0) and one login class.
 */

#ifndef _MINKAR_KVM_DARWIN_H_
#define	_MINKAR_KVM_DARWIN_H_

#include <sys/types.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/time.h>

#define	KERN_PROC_PROC		KERN_PROC_ALL
/* Asks kvm_getprocs() for one record per thread (ps -M). */
#define	KERN_PROC_INC_THREAD	0x10
#define	P_KPROC			P_SYSTEM
#define	TDF_SINTR		0x00000008
#define	NOCPU			(-1)
#define	KI_NGROUPS		16

/* ki_kiflag */
#define	KI_CTTY			0x00000001	/* has a controlling terminal */
#define	KI_SLEADER		0x00000002	/* session leader */
#define	KI_LOCKBLOCK		0x00000004	/* never: Darwin does not say */

struct minkar_priority {
	u_char		pri_class;
	u_char		pri_level;	/* Darwin's priority, 0-127 */
	u_char		pri_native;
	u_char		pri_user;
};

struct minkar_kinfo {
	pid_t		ki_pid;
	pid_t		ki_ppid;
	pid_t		ki_pgid;
	pid_t		ki_tpgid;	/* the terminal's foreground group */
	pid_t		ki_sid;
	pid_t		ki_tsid;	/* the terminal's session */
	short		ki_jobc;
	dev_t		ki_tdev;
	uid_t		ki_uid;		/* effective */
	uid_t		ki_ruid;
	uid_t		ki_svuid;
	gid_t		ki_rgid;
	gid_t		ki_svgid;
	short		ki_ngroups;
	gid_t		ki_groups[KI_NGROUPS];	/* [0] is the effective gid */
	vm_size_t	ki_size;	/* virtual size, bytes */
	/* long, as FreeBSD's 64-bit segsz_t: ps reads them so. */
	long		ki_rssize;	/* resident pages */
	long		ki_tsize;	/* text pages */
	u_short		ki_xstat;
	u_short		ki_acflag;
	fixpt_t		ki_pctcpu;	/* FSCALE units */
	u_int		ki_estcpu;
	u_int		ki_slptime;	/* seconds since a thread last ran */
	u_int		ki_swtime;
	uint64_t	ki_runtime;	/* user + system, microseconds */
	struct timeval	ki_start;
	struct timeval	ki_childtime;
	struct timeval	ki_childutime;
	struct timeval	ki_childstime;
	long		ki_flag;	/* Darwin's P_* */
	long		ki_kiflag;	/* KI_* */
	int		ki_traceflag;
	char		ki_stat;	/* SIDL, SRUN, ..., SZOMB */
	char		ki_runstate;	/* from the threads: R U S I T, or 0 */
	signed char	ki_nice;
	char		ki_lock;
	int		ki_oncpu;
	int		ki_lastcpu;
	int		ki_tdflags;
	struct minkar_priority ki_pri;
	struct rusage	ki_rusage;
	int		ki_numthreads;
	int		ki_tid;		/* the thread's index with -M, else 0 */
	int		ki_jid;
	sigset_t	ki_siglist;
	sigset_t	ki_sigmask;
	sigset_t	ki_sigignore;
	sigset_t	ki_sigcatch;
	void		*ki_wchan;
	void		*ki_paddr;
	void		*ki_tracep;
	void		*ki_ru;
	void		*ki_sess;
	void		*ki_tsess;
	/* Workqueue threads (macOS ps's wq, wqb, wqr, wql). */
	u_int		ki_wq_threads;
	u_int		ki_wq_running;
	u_int		ki_wq_blocked;
	u_int		ki_wq_state;
	char		ki_comm[MAXCOMLEN + 1];
	char		ki_tdname[MAXCOMLEN + 1];
	char		ki_moretdname[1];
	char		ki_wmesg[8];
	char		ki_login[MAXLOGNAME];
	char		ki_loginclass[16];
	void		*ki_udata;	/* for the command's own use */
};

typedef struct minkar_kvm kvm_t;

__BEGIN_DECLS
kvm_t	*kvm_openfiles(const char *, const char *, const char *, int, char *);
int	 kvm_close(kvm_t *);
char	*kvm_geterr(kvm_t *);
struct minkar_kinfo *kvm_getprocs(kvm_t *, int, int, int *);
char	**kvm_getargv(kvm_t *, const struct minkar_kinfo *, int);
char	**kvm_getenvv(kvm_t *, const struct minkar_kinfo *, int);
/*
 * The name a process was started under: the last component of its
 * executable's path. p_comm keeps only MAXCOMLEN (16) characters; when
 * the arguments cannot be read (another user's process), ki_comm.
 */
const char *kvm_getcomm(kvm_t *, const struct minkar_kinfo *, char *, size_t);
int	 jail_getid(const char *);
__END_DECLS

#ifndef _MINKAR_KVM_IMPL
#define	kinfo_proc	minkar_kinfo
#endif

#endif /* !_MINKAR_KVM_DARWIN_H_ */
