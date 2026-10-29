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
 * What FreeBSD's password tools (vipw, pwd_mkdb, chpass) find in its
 * <pwd.h> and <libutil.h> and Darwin's lack: the versioned keys of the
 * pwd.db format, the _PWF_ bits saying which fields of a master.passwd
 * line were filled in, and the pw_util(3) and flopen(3) functions of
 * libpw.
 *
 * Darwin's struct passwd has no pw_fields member. PW_FIELDS(pw) gives
 * the bits kept for that struct beside it, in libpw; a struct libpw has
 * not seen (one from getpwnam) counts as read from the files with every
 * field filled in.
 */

#ifndef _MINKAR_PW_COMPAT_H_
#define	_MINKAR_PW_COMPAT_H_

#include <sys/types.h>
#include <limits.h>
#include <stdint.h>
#include <pwd.h>
#include <stdio.h>

#ifndef _MASTERPASSWD
#define	_MASTERPASSWD		"master.passwd"
#endif
#ifndef _PASSWD
#define	_PASSWD			"passwd"
#endif
#ifndef _MP_DB
#define	_MP_DB			"pwd.db"
#endif
#ifndef _SMP_DB
#define	_SMP_DB			"spwd.db"
#endif

/* pwd.db keys: a tag octet whose high bits carry the format version. */
#define	_PW_VERSION_MASK	'\xF0'
#define	_PW_VERSIONED(x, v)	((unsigned char)(((x) & 0xCF) | ((v) << 4)))
#ifndef _PW_KEYYPENABLED
#define	_PW_KEYYPENABLED	'\x34'
#endif
#ifndef _PW_KEYYPBYNUM
#define	_PW_KEYYPBYNUM		'\x35'
#endif
#define	_PWD_VERSION_KEY	"\xFF" "VERSION"
#define	_PWD_CURRENT_VERSION	'\x04'

/* Which fields of a passwd line were filled in, and where it came from. */
#define	_PWF(x)		(1 << (x))
#define	_PWF_NAME	_PWF(0)
#define	_PWF_PASSWD	_PWF(1)
#define	_PWF_UID	_PWF(2)
#define	_PWF_GID	_PWF(3)
#define	_PWF_CHANGE	_PWF(4)
#define	_PWF_CLASS	_PWF(5)
#define	_PWF_GECOS	_PWF(6)
#define	_PWF_DIR	_PWF(7)
#define	_PWF_SHELL	_PWF(8)
#define	_PWF_EXPIRE	_PWF(9)
#define	_PWF_SOURCE	0x3000
#define	_PWF_FILES	0x1000
#define	_PWF_NIS	0x2000
#define	_PWF_HESIOD	0x3000

#ifndef _PATH_ETC
#define	_PATH_ETC		"/etc"
#endif

int	flopen(const char *, int, ...);

/* A user or group id as Darwin writes it: above INT_MAX, negative. */
#define	PW_ID(id)	((intmax_t)(int32_t)(id))

int	*__pw_fields(const struct passwd *);
#define	PW_FIELDS(pw)	(*__pw_fields(pw))

/* pw_util(3) */
#define	PWSCAN_MASTER	0x01
#define	PWSCAN_WARN	0x02

int	pw_copy(int, int, const struct passwd *, struct passwd *);
struct passwd *pw_dup(const struct passwd *);
int	pw_edit(int);
int	pw_equal(const struct passwd *, const struct passwd *);
void	pw_fini(void);
int	pw_init(const char *, const char *);
void	pw_initpwd(struct passwd *);
char	*pw_make(const struct passwd *);
char	*pw_make_v7(const struct passwd *);
int	pw_mkdb(const char *);
int	pw_lock(void);
struct passwd *pw_scan(const char *, int);
const char *pw_tempname(void);
int	pw_tmp(int);

#endif /* !_MINKAR_PW_COMPAT_H_ */
