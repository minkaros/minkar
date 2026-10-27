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
 * FreeBSD's pwcache(3) name-to-id lookups, which Darwin's libc lacks
 * (it has only user_from_uid and group_from_gid). pwcache_userdb and
 * pwcache_groupdb switch the lookups to other functions, as install -N
 * does with a passwd and group file of its own.
 */

#include <sys/types.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stddef.h>

#include "bsdcompat.h"

static struct passwd *(*pw_byname)(const char *) = getpwnam;
static struct group *(*gr_byname)(const char *) = getgrnam;

int
uid_from_user(const char *name, uid_t *uid)
{
	struct passwd *pw;

	if (name == NULL || uid == NULL)
		return (-1);
	if ((pw = pw_byname(name)) == NULL)
		return (-1);
	*uid = pw->pw_uid;
	return (0);
}

int
gid_from_group(const char *name, gid_t *gid)
{
	struct group *gr;

	if (name == NULL || gid == NULL)
		return (-1);
	if ((gr = gr_byname(name)) == NULL)
		return (-1);
	*gid = gr->gr_gid;
	return (0);
}

int
pwcache_userdb(int (*setpassent)(int), void (*endpwent)(void),
    struct passwd *(*getpwnam)(const char *), struct passwd *(*getpwuid)(uid_t))
{
	(void)setpassent;
	(void)endpwent;
	(void)getpwuid;
	if (getpwnam == NULL) {
		errno = EINVAL;
		return (-1);
	}
	pw_byname = getpwnam;
	return (0);
}

int
pwcache_groupdb(int (*setgroupent)(int), void (*endgrent)(void),
    struct group *(*getgrnam)(const char *), struct group *(*getgrgid)(gid_t))
{
	(void)setgroupent;
	(void)endgrent;
	(void)getgrgid;
	if (getgrnam == NULL) {
		errno = EINVAL;
		return (-1);
	}
	gr_byname = getgrnam;
	return (0);
}
