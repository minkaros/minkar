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
 * FreeBSD library functions that the FreeBSD commands built on ravynOS use
 * and Darwin lacks (libbsdcompat.a). Included first by those commands
 * (-include bsdcompat.h).
 */

#ifndef _RAVYNOS_BSDCOMPAT_H
#define _RAVYNOS_BSDCOMPAT_H

#include <sys/cdefs.h>
#include <stdint.h>

/*
 * Names FreeBSD has and Darwin calls otherwise, or lacks:
 * - struct stat's timespecs are st_atimespec etc. on Darwin;
 * - O_RESOLVE_BENEATH, AT_RESOLVE_BENEATH: no such lookup restriction on
 *   Darwin, so cp -R and friends open without it (a symbolic link in the
 *   destination can lead outside it, as with older FreeBSD);
 * - UF_ARCHIVE: no such flag (Darwin's is the superuser's SF_ARCHIVED).
 */
#define	st_atim		st_atimespec
#define	st_mtim		st_mtimespec
#define	st_ctim		st_ctimespec
#define	st_birthtim	st_birthtimespec
#ifndef O_RESOLVE_BENEATH
#define	O_RESOLVE_BENEATH	0
#endif
#ifndef AT_RESOLVE_BENEATH
#define	AT_RESOLVE_BENEATH	0
#endif
#ifndef UF_ARCHIVE
#define	UF_ARCHIVE	0
#endif
#ifndef O_SEARCH
#define	O_SEARCH	O_RDONLY	/* a directory opened to look things up in */
#endif
/*
 * ACLs: Darwin's are NFSv4-style "extended" ACLs; it has no POSIX.1e
 * access ACLs (so _PC_ACL_EXTENDED is a name pathconf rejects).
 */
#define	_PC_ACL_NFS4		_PC_EXTENDED_SECURITY_NP
#define	_PC_ACL_EXTENDED	(-1)
#define	ACL_TYPE_NFS4		ACL_TYPE_EXTENDED
/* <paths.h> */
#define	_PATH_CP	"/bin/cp"
#define	_PATH_RM	"/bin/rm"

__BEGIN_DECLS

/* <stdlib.h>: a number in [minval, maxval], or 0 and *errstr set. */
long long	strtonum(const char *numstr, long long minval, long long maxval,
		    const char **errstr);

/* <libutil.h>: "10k", "4M", ... (powers of 1024) to a number. */
int		expand_number(const char *buf, uint64_t *num);

/* <unistd.h>: copies len bytes; with an offset pointer, at that offset. */
#include <sys/types.h>
ssize_t		copy_file_range(int infd, off_t *inoffp, int outfd, off_t *outoffp,
		    size_t len, unsigned int flags);

/* <sys/acl.h>: an ACL without entries is trivial (the mode says it all). */
struct _acl;
int		acl_is_trivial_np(struct _acl *acl, int *trivial);

/* <unistd.h>: file flags, relative to a directory descriptor. */
int		chflagsat(int fd, const char *path, unsigned long flags, int atflag);

/* <sys/stat.h>: relative to a directory descriptor. */
int		mkfifoat(int fd, const char *path, mode_t mode);
int		mknodat(int fd, const char *path, mode_t mode, dev_t dev);

__END_DECLS

#endif /* !_RAVYNOS_BSDCOMPAT_H */
