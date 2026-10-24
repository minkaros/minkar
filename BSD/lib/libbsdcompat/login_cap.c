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

/* login classes for ravynOS: see include/login_cap.h. */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <paths.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "login_cap.h"

/* The class "default" (or none) is the only one there is. */
login_cap_t *
login_getclass(const char *name)
{
	login_cap_t *lc;

	if ((lc = calloc(1, sizeof(*lc))) == NULL)
		return (NULL);
	if (name == NULL || *name == '\0' || strcmp(name, LOGIN_DEFCLASS) == 0) {
		if ((lc->lc_class = strdup(LOGIN_DEFCLASS)) == NULL) {
			free(lc);
			return (NULL);
		}
	}
	return (lc);
}

login_cap_t *
login_getpwclass(const struct passwd *pwd)
{

	(void)pwd;
	return (login_getclass(LOGIN_DEFCLASS));
}

void
login_close(login_cap_t *lc)
{

	if (lc != NULL) {
		free(lc->lc_class);
		free(lc);
	}
}

int
setusercontext(login_cap_t *lc, const struct passwd *pwd, uid_t uid,
    unsigned int flags)
{

	(void)lc;
	if (pwd == NULL && (flags & (LOGIN_SETGROUP | LOGIN_SETLOGIN))) {
		errno = EINVAL;
		return (-1);
	}
	if (flags & LOGIN_SETGROUP) {
		if (setgid(pwd->pw_gid) == -1)
			return (-1);
		if (initgroups(pwd->pw_name, (int)pwd->pw_gid) == -1)
			return (-1);
	}
	if ((flags & LOGIN_SETLOGIN) && setlogin(pwd->pw_name) == -1)
		return (-1);
	if (flags & LOGIN_SETPATH) {
		if (setenv("PATH", uid == 0 ? _PATH_STDPATH : _PATH_DEFPATH,
		    1) == -1)
			return (-1);
	}
	if (flags & LOGIN_SETUMASK)
		(void)umask(022);
	/* Last: after it there may be no privilege left for the rest. */
	if ((flags & LOGIN_SETUSER) && setuid(uid) == -1)
		return (-1);
	return (0);
}
