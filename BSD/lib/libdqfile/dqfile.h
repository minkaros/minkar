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
 * Darwin's quota files, as <sys/quota.h> describes them: at the root of
 * a volume, .quota.user and .quota.group hold a header and a hash table
 * of struct dqblk, one per user or group, found by double hashing on the
 * id; .quota.ops.user and .quota.ops.group say quotas are wanted there.
 * Limits and usage are in bytes and files. libdqfile reads and writes
 * those files and talks to quotactl(2) for the quota tools.
 */

#ifndef _MINKAR_DQFILE_H_
#define	_MINKAR_DQFILE_H_

#include <sys/types.h>
#include <sys/quota.h>
#include <stdbool.h>
#include <stdint.h>

struct dqfile {
	int		fd;
	int		type;		/* USRQUOTA or GRPQUOTA */
	struct dqfilehdr hdr;	/* in host byte order */
	char		path[1024];
};

/* "user" or "group". */
const char *dq_typename(int type);

/* Path of the quota file / ops file of a mount point. */
void	dq_filepath(const char *mnt, int type, char *buf, size_t len);
void	dq_opspath(const char *mnt, int type, char *buf, size_t len);
/* Whether quotas of that type are wanted on the volume (the ops file). */
bool	dq_wanted(const char *mnt, int type);
/* Whether the kernel has them on (Q_QUOTASTAT). */
bool	dq_enabled(const char *mnt, int type);

/*
 * Open a quota file. With create, a missing (or empty) file is made with
 * room for maxentries (0: sized from the volume, as the kernel would).
 * -1 and errno on error.
 */
int	dq_open(struct dqfile *, const char *mnt, int type, bool create,
	    uint32_t maxentries);
void	dq_close(struct dqfile *);
/* The entry for id (zeroed, with dqb_id set, when it has none). */
int	dq_get(struct dqfile *, uint32_t id, struct dqblk *);
/* Store the entry for id, adding it if needed. */
int	dq_put(struct dqfile *, uint32_t id, const struct dqblk *);
/* Write the header (grace times, check time, count). */
int	dq_puthdr(struct dqfile *);
/* Call fn for every entry in use. */
int	dq_foreach(struct dqfile *, int (*fn)(const struct dqblk *, void *),
	    void *arg);

/* Limits and usage from the kernel (when on) or else from the file. */
int	dq_getquota(const char *mnt, int type, uint32_t id, struct dqblk *);
/* Store limits: in the kernel when on (Q_SETQUOTA), and in the file. */
int	dq_setquota(const char *mnt, int type, uint32_t id,
	    const struct dqblk *);

/* Each mounted local volume: fn(mount point, device). */
int	dq_mounts(int (*fn)(const char *mnt, const char *dev, void *),
	    void *arg);

/* Byte counts as 1K blocks, rounded up, as the tools show them. */
#define	DQ_KB(bytes)	(((bytes) + 1023) / 1024)

#endif /* !_MINKAR_DQFILE_H_ */
