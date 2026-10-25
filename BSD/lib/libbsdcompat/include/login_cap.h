/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
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
 * FreeBSD's login classes, reduced to what Minkar has: one class,
 * "default", with no capabilities database (macOS has none either).
 * setusercontext() sets the groups, login name, PATH, umask and user;
 * resource limits, priority and MAC labels are left as they are.
 */

#ifndef _RAVYNOS_LOGIN_CAP_H
#define _RAVYNOS_LOGIN_CAP_H

#include <sys/cdefs.h>
#include <sys/types.h>

#define	LOGIN_DEFCLASS		"default"
#define	LOGIN_DEFROOTCLASS	"root"
#define	LOGIN_MECLASS		"me"

#define	LOGIN_SETGROUP		0x0001	/* set group ids and supplementary groups */
#define	LOGIN_SETLOGIN		0x0002	/* set login name (setlogin) */
#define	LOGIN_SETRESOURCES	0x0004	/* resource limits (not on Minkar) */
#define	LOGIN_SETPRIORITY	0x0008	/* priority (not on Minkar) */
#define	LOGIN_SETUMASK		0x0010	/* umask 022 */
#define	LOGIN_SETUSER		0x0020	/* set user id */
#define	LOGIN_SETENV		0x0040	/* environment (nothing on Minkar) */
#define	LOGIN_SETPATH		0x0080	/* PATH */
#define	LOGIN_SETMAC		0x0100	/* MAC label (not on Minkar) */
#define	LOGIN_SETCPUMASK	0x0200	/* CPU mask (not on Minkar) */
#define	LOGIN_SETLOGINCLASS	0x0400	/* login class (not on Minkar) */
#define	LOGIN_SETALL		0x07ff

typedef struct login_cap {
	char	*lc_class;
} login_cap_t;

struct passwd;

__BEGIN_DECLS
login_cap_t	*login_getclass(const char *name);
login_cap_t	*login_getpwclass(const struct passwd *pwd);
void		 login_close(login_cap_t *lc);
int		 setusercontext(login_cap_t *lc, const struct passwd *pwd,
		    uid_t uid, unsigned int flags);
__END_DECLS

#endif /* !_RAVYNOS_LOGIN_CAP_H */
