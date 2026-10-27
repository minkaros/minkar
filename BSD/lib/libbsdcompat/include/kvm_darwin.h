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
 * The part of FreeBSD's libkvm and libjail that pkill(1) and killall(1)
 * use, on Darwin.
 * Processes come from sysctl(KERN_PROC_ALL) and arguments from
 * KERN_PROCARGS2; each Darwin kinfo_proc becomes a FreeBSD-shaped record
 * with the ki_* fields pkill reads. Darwin has no jails (every process is
 * in jail 0) and one login class.
 */

#ifndef _MINKAR_KVM_DARWIN_H_
#define	_MINKAR_KVM_DARWIN_H_

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Each command uses part of this file. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

#define	KERN_PROC_PROC	KERN_PROC_ALL
#define	P_KPROC		P_SYSTEM

struct minkar_kinfo {
	pid_t		ki_pid;
	pid_t		ki_ppid;
	pid_t		ki_pgid;
	pid_t		ki_sid;
	uid_t		ki_uid;		/* effective */
	uid_t		ki_ruid;
	gid_t		ki_rgid;
	dev_t		ki_tdev;
	int		ki_flag;
	char		ki_stat;	/* SIDL, SRUN, ..., SZOMB */
	int		ki_jid;
	struct timeval	ki_start;
	char		ki_comm[MAXCOMLEN + 1];
	char		ki_loginclass[16];
};

typedef struct {
	struct minkar_kinfo	*procs;
	char			**argv;
	char			*argbuf;
	char			err[128];
} kvm_t;

static kvm_t *
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

static char *
kvm_geterr(kvm_t *kd)
{
	return (kd->err);
}

static void
minkar_kinfo_fill(struct minkar_kinfo *ki, const struct kinfo_proc *kp)
{
	memset(ki, 0, sizeof(*ki));
	ki->ki_pid = kp->kp_proc.p_pid;
	ki->ki_ppid = kp->kp_eproc.e_ppid;
	ki->ki_pgid = kp->kp_eproc.e_pgid;
	ki->ki_sid = getsid(ki->ki_pid);
	ki->ki_uid = kp->kp_eproc.e_ucred.cr_uid;
	ki->ki_ruid = kp->kp_eproc.e_pcred.p_ruid;
	ki->ki_rgid = kp->kp_eproc.e_pcred.p_rgid;
	ki->ki_tdev = kp->kp_eproc.e_tdev;
	ki->ki_flag = kp->kp_proc.p_flag;
	ki->ki_stat = kp->kp_proc.p_stat;
	/* The kernel (pid 0) is the process pgrep -S is about. */
	if (ki->ki_pid == 0)
		ki->ki_flag |= P_SYSTEM;
	ki->ki_start = kp->kp_proc.p_starttime;
	strlcpy(ki->ki_comm, kp->kp_proc.p_comm, sizeof(ki->ki_comm));
	strlcpy(ki->ki_loginclass, "default", sizeof(ki->ki_loginclass));
}

static struct minkar_kinfo *
kvm_getprocs(kvm_t *kd, int op, int arg, int *cnt)
{
	int mib[4] = { CTL_KERN, KERN_PROC, op, arg };
	struct kinfo_proc *kp = NULL;
	size_t len = 0;
	int i, n, tries;

	*cnt = 0;
	free(kd->procs);
	kd->procs = NULL;
	/* The table can grow between the size query and the read. */
	for (tries = 0; tries < 8; tries++) {
		if (sysctl(mib, op == KERN_PROC_ALL ? 3 : 4, NULL, &len,
		    NULL, 0) == -1)
			goto fail;
		len += len / 8 + sizeof(*kp);
		free(kp);
		if ((kp = malloc(len)) == NULL)
			goto fail;
		if (sysctl(mib, op == KERN_PROC_ALL ? 3 : 4, kp, &len,
		    NULL, 0) == 0)
			break;
		if (errno != ENOMEM)
			goto fail;
	}
	if (tries == 8)
		goto fail;
	n = (int)(len / sizeof(*kp));
	if ((kd->procs = calloc(n > 0 ? n : 1, sizeof(*kd->procs))) == NULL)
		goto fail;
	for (i = 0; i < n; i++)
		minkar_kinfo_fill(&kd->procs[i], &kp[i]);
	free(kp);
	*cnt = n;
	return (kd->procs);
fail:
	strlcpy(kd->err, strerror(errno), sizeof(kd->err));
	free(kp);
	return (NULL);
}

/*
 * KERN_PROCARGS2 gives argc, the executable path, NUL padding, then argc
 * NUL-terminated arguments (and the environment, which is not wanted).
 */
static char **
kvm_getargv(kvm_t *kd, const struct minkar_kinfo *ki, int nchr)
{
	int mib[3] = { CTL_KERN, KERN_PROCARGS2, ki->ki_pid };
	size_t len;
	int argmax, argc, i;
	char *p, *end;

	(void)nchr;
	free(kd->argv);
	kd->argv = NULL;
	len = sizeof(argmax);
	if (sysctlbyname("kern.argmax", &argmax, &len, NULL, 0) == -1)
		return (NULL);
	free(kd->argbuf);
	if ((kd->argbuf = malloc(argmax)) == NULL)
		return (NULL);
	len = argmax;
	if (sysctl(mib, 3, kd->argbuf, &len, NULL, 0) == -1 ||
	    len < sizeof(int))
		return (NULL);
	memcpy(&argc, kd->argbuf, sizeof(argc));
	p = kd->argbuf + sizeof(int);
	end = kd->argbuf + len;
	while (p < end && *p != '\0')		/* executable path */
		p++;
	while (p < end && *p == '\0')		/* padding */
		p++;
	if (argc <= 0 || (kd->argv = calloc(argc + 1, sizeof(char *))) == NULL)
		return (NULL);
	for (i = 0; i < argc && p < end; i++) {
		kd->argv[i] = p;
		while (p < end && *p != '\0')
			p++;
		p++;
	}
	if (i == 0 || p > end) {
		free(kd->argv);
		kd->argv = NULL;
	}
	return (kd->argv);
}

/*
 * The name a process was started under: the last component of its
 * executable's path (KERN_PROCARGS2). p_comm keeps only MAXCOMLEN (16)
 * characters, and macOS's killall matches whole names. When the
 * arguments cannot be read (another user's process), ki_comm.
 */
static const char *
kvm_getcomm(kvm_t *kd, const struct minkar_kinfo *ki, char *buf, size_t len)
{
	int mib[3] = { CTL_KERN, KERN_PROCARGS2, ki->ki_pid };
	size_t size;
	int argmax;
	char *path, *base;

	strlcpy(buf, ki->ki_comm, len);
	size = sizeof(argmax);
	if (sysctlbyname("kern.argmax", &argmax, &size, NULL, 0) == -1)
		return (buf);
	free(kd->argbuf);
	if ((kd->argbuf = malloc(argmax)) == NULL)
		return (buf);
	size = argmax;
	if (sysctl(mib, 3, kd->argbuf, &size, NULL, 0) == -1 ||
	    size <= sizeof(int))
		return (buf);
	path = kd->argbuf + sizeof(int);
	if (strnlen(path, size - sizeof(int)) == size - sizeof(int) ||
	    *path == '\0')
		return (buf);
	base = strrchr(path, '/');
	strlcpy(buf, base != NULL ? base + 1 : path, len);
	return (buf);
}

static int
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

#pragma clang diagnostic pop

#define	kinfo_proc	minkar_kinfo

#endif /* !_MINKAR_KVM_DARWIN_H_ */
