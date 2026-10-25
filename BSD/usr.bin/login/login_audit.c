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
 * login's part of the audit system, as on macOS: the session gets the
 * user's audit identity (auid), preselection mask (audit_user and
 * audit_control), terminal and a new session ID, and login, failed login
 * and logout are recorded (AUE_login, AUE_logout).
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <bsm/audit.h>
#include <bsm/audit_uevents.h>
#include <bsm/libbsm.h>
#include <netinet/in.h>

#include <errno.h>
#include <netdb.h>
#include <pwd.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "login_audit.h"

static void
terminal_id(au_tid_addr_t *tid, const char *tty, const char *host)
{
	struct addrinfo hints, *res;
	struct stat st;

	memset(tid, 0, sizeof(*tid));
	tid->at_type = AU_IPv4;
	if (host != NULL) {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_flags = AI_NUMERICHOST;
		if (getaddrinfo(host, NULL, &hints, &res) == 0) {
			if (res->ai_family == AF_INET6) {
				tid->at_type = AU_IPv6;
				memcpy(tid->at_addr,
				    &((struct sockaddr_in6 *)(void *)res->ai_addr)->sin6_addr, 16);
			} else if (res->ai_family == AF_INET) {
				memcpy(tid->at_addr,
				    &((struct sockaddr_in *)(void *)res->ai_addr)->sin_addr, 4);
			}
			freeaddrinfo(res);
		}
	} else if (tty != NULL && stat(tty, &st) == 0) {
		tid->at_port = st.st_rdev;	/* local: the terminal device */
	}
}

void
login_audit_success(const struct passwd *pw, const char *tty, const char *host)
{
	auditinfo_addr_t aia;
	au_mask_t mask;

	memset(&aia, 0, sizeof(aia));
	if (au_user_mask(pw->pw_name, &mask) != 0) {
		syslog(LOG_ERR, "au_user_mask(%s) failed", pw->pw_name);
		mask.am_success = mask.am_failure = 0;
	}
	aia.ai_auid = pw->pw_uid;
	aia.ai_mask = mask;
	aia.ai_asid = AU_ASSIGN_ASID;
	terminal_id(&aia.ai_termid, tty, host);
	if (setaudit_addr(&aia, sizeof(aia)) != 0 && errno != ENOSYS)
		syslog(LOG_ERR, "setaudit_addr: %m");

	if (audit_submit(AUE_login, pw->pw_uid, 0, 0, "successful login %s",
	    pw->pw_name) != 0)
		syslog(LOG_ERR, "could not record login of %s", pw->pw_name);
}

void
login_audit_failure(const char *user, const char *tty, const char *host)
{
	struct passwd *pw = user != NULL ? getpwnam(user) : NULL;
	au_id_t auid = pw != NULL ? pw->pw_uid : AU_DEFAUDITID;
	au_tid_addr_t tid;
	token_t *tok;
	int cond, fd;

	/* The record names the terminal; login's own audit state, still
	 * launchd's session, is left alone. */
	if (audit_get_cond(&cond) != 0 || cond == AUC_NOAUDIT)
		return;
	terminal_id(&tid, tty, host);
	if ((fd = au_open()) < 0)
		return;
	if ((tok = au_to_subject32_ex(auid, geteuid(), getegid(), getuid(),
	    getgid(), getpid(), getpid(), &tid)) != NULL)
		au_write(fd, tok);
	if ((tok = au_to_text(user != NULL ? user : "")) != NULL)
		au_write(fd, tok);
	if ((tok = au_to_text("bad login")) != NULL)
		au_write(fd, tok);
	if ((tok = au_to_return32(1, EPERM)) != NULL)
		au_write(fd, tok);
	if (au_close(fd, 1, AUE_login) != 0)
		syslog(LOG_ERR, "could not record failed login");
}

void
login_audit_logout(const struct passwd *pw)
{
	if (audit_submit(AUE_logout, pw->pw_uid, 0, 0, "logout %s",
	    pw->pw_name) != 0)
		syslog(LOG_ERR, "could not record logout of %s", pw->pw_name);
}
