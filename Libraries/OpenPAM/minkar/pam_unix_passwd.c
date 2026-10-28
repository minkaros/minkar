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
 * pam_unix's password changing, which OpenPAM's pam_unix leaves out: it is
 * built into the same module (Makefile renames OpenPAM's empty
 * pam_sm_chauthtok). Minkar keeps its accounts in /etc/master.passwd,
 * which libinfo reads directly (no database to rebuild): the new password
 * is hashed with SHA-512 ("$6$", crypt(3)) and the user's line rewritten
 * through a temporary file renamed over the old one, under an exclusive
 * lock on it.
 *
 * Only root (the real user ID) may change another user's password or skip
 * the old one; the caller (passwd, setuid root) needs the effective one.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <security/pam_modules.h>
#include <security/pam_appl.h>
#include <security/openpam.h>

#define MASTER_PASSWD	"/etc/master.passwd"
#define SALT_LEN	16

static const char salt_chars[] =
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

/* The old password: asked for in the first pass, taken from PAM's item in
 * the second (asking again would prompt twice). */
static int
check_old(pam_handle_t *pamh, struct passwd *pwd, int flags, int ask)
{
	const char *old, *hash;
	int r;

	if (getuid() == 0)
		return (PAM_SUCCESS);
	if (pwd->pw_passwd[0] == '\0' && !(flags & PAM_DISALLOW_NULL_AUTHTOK))
		return (PAM_SUCCESS);
	if (ask)
		r = pam_get_authtok(pamh, PAM_OLDAUTHTOK, &old, NULL);
	else
		r = pam_get_item(pamh, PAM_OLDAUTHTOK, (const void **)&old);
	if (r != PAM_SUCCESS)
		return (r);
	if (old == NULL)
		return (PAM_PERM_DENIED);
	hash = crypt(old, pwd->pw_passwd);
	if (hash == NULL || strcmp(hash, pwd->pw_passwd) != 0)
		return (PAM_PERM_DENIED);
	return (PAM_SUCCESS);
}

/* The new password; root is not asked for the old one, so it is asked to
 * type the new one twice here (pam_get_authtok does that only after an
 * old one). */
static int
get_new(pam_handle_t *pamh, const char **newp)
{
	const void *old;
	char *again;
	int r;

	r = pam_get_authtok(pamh, PAM_AUTHTOK, newp, "New Password:");
	if (r != PAM_SUCCESS)
		return (r);
	if (pam_get_item(pamh, PAM_OLDAUTHTOK, &old) == PAM_SUCCESS &&
	    old != NULL)
		return (PAM_SUCCESS);
	r = pam_prompt(pamh, PAM_PROMPT_ECHO_OFF, &again,
	    "Retype New Password:");
	if (r != PAM_SUCCESS)
		return (r);
	r = (again != NULL && strcmp(again, *newp) == 0) ?
	    PAM_SUCCESS : PAM_AUTHTOK_ERR;
	if (again != NULL) {
		memset(again, 0, strlen(again));
		free(again);
	}
	if (r != PAM_SUCCESS)
		pam_error(pamh, "Mismatch; try again, EOF to quit.");
	return (r);
}

static char *
make_hash(const char *password)
{
	char salt[3 + SALT_LEN + 2];
	const char *hash;
	int i;

	memcpy(salt, "$6$", 3);
	for (i = 0; i < SALT_LEN; i++)
		salt[3 + i] = salt_chars[arc4random_uniform(
		    sizeof(salt_chars) - 1)];
	salt[3 + SALT_LEN] = '$';
	salt[3 + SALT_LEN + 1] = '\0';
	hash = crypt(password, salt);
	if (hash == NULL || strncmp(hash, "$6$", 3) != 0)
		return (NULL);
	return (strdup(hash));
}

/* Copies MASTER_PASSWD with the user's second field replaced. */
static int
rewrite(const char *user, const char *hash)
{
	char tmp[] = "/etc/master.passwd.XXXXXX";
	char *line = NULL;
	size_t cap = 0, ulen = strlen(user);
	ssize_t len;
	FILE *in = NULL, *out = NULL;
	int ifd, ofd = -1, found = 0, r = PAM_SYSTEM_ERR;
	char *rest;

	if ((ifd = open(MASTER_PASSWD, O_RDONLY | O_EXLOCK | O_CLOEXEC)) < 0)
		return (PAM_SYSTEM_ERR);
	if ((in = fdopen(ifd, "r")) == NULL) {
		close(ifd);
		return (PAM_SYSTEM_ERR);
	}
	if ((ofd = mkstemp(tmp)) < 0)
		goto done;
	if (fchown(ofd, 0, 0) != 0 || fchmod(ofd, 0600) != 0 ||
	    (out = fdopen(ofd, "w")) == NULL)
		goto done;
	while ((len = getline(&line, &cap, in)) > 0) {
		if (!found && strncmp(line, user, ulen) == 0 &&
		    line[ulen] == ':' &&
		    (rest = strchr(line + ulen + 1, ':')) != NULL) {
			fprintf(out, "%s:%s%s", user, hash, rest);
			found = 1;
		} else
			fwrite(line, 1, (size_t)len, out);
	}
	if (ferror(in) || !found)
		goto done;
	if (fflush(out) != 0 || fsync(ofd) != 0)
		goto done;
	if (rename(tmp, MASTER_PASSWD) != 0)
		goto done;
	r = PAM_SUCCESS;
done:
	if (out != NULL)
		fclose(out);
	else if (ofd >= 0)
		close(ofd);
	if (r != PAM_SUCCESS && ofd >= 0)
		unlink(tmp);
	fclose(in);
	free(line);
	return (r);
}

PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t *pamh, int flags,
	int argc, const char *argv[])
{
	struct passwd *pwd;
	const char *user, *new;
	char *hash;
	int r;

	(void)argc;
	(void)argv;

	if ((r = pam_get_user(pamh, &user, NULL)) != PAM_SUCCESS)
		return (r);
	if ((pwd = getpwnam(user)) == NULL)
		return (PAM_USER_UNKNOWN);
	if (getuid() != 0 && getuid() != pwd->pw_uid)
		return (PAM_PERM_DENIED);
	if (geteuid() != 0)
		return (PAM_PERM_DENIED);

	if (flags & PAM_PRELIM_CHECK)
		return (check_old(pamh, pwd, flags, 1));
	if (!(flags & PAM_UPDATE_AUTHTOK))
		return (PAM_SERVICE_ERR);

	if ((r = check_old(pamh, pwd, flags, 0)) != PAM_SUCCESS)
		return (r);
	if ((r = get_new(pamh, &new)) != PAM_SUCCESS)
		return (r);
	if (new[0] == '\0' && (flags & PAM_DISALLOW_NULL_AUTHTOK))
		return (PAM_AUTHTOK_ERR);
	if ((hash = make_hash(new)) == NULL)
		return (PAM_SYSTEM_ERR);
	r = rewrite(pwd->pw_name, hash);
	memset(hash, 0, strlen(hash));
	free(hash);
	return (r);
}
