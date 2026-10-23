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
 * login: log into the system.
 *
 *	login [-fp] [-h hostname] [user]
 *
 * As on macOS, authentication and accounts are PAM's (the "login" service,
 * /etc/pam.d/login). login stays in the parent while the shell runs, to
 * close the PAM session and the utmpx record when the shell exits.
 *
 *	-f	the user is already authenticated (only when root runs login)
 *	-h	the remote host the user comes from (for sshd, telnetd)
 *	-p	keep the environment
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <libgen.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sysexits.h>
#include <unistd.h>
#include <utmpx.h>

#include <security/pam_appl.h>
#include <security/openpam.h>

#define PAM_SERVICE	"login"
#define MAX_TRIES	3
#define LOGIN_TIMEOUT	300	/* seconds to log in */
#define FAIL_DELAY	2	/* seconds after a failed attempt */
#define PATH_MOTD	"/etc/motd"
#define USER_PATH	"/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin"

extern char **environ;

static pam_handle_t *pamh;
static const struct pam_conv conv = { openpam_ttyconv, NULL };

static void __dead2
usage(void)
{
	fprintf(stderr, "usage: login [-fp] [-h hostname] [user]\n");
	exit(EX_USAGE);
}

static void
timed_out(int sig)
{
	(void)sig;
	fprintf(stderr, "\nLogin timed out after %d seconds\n", LOGIN_TIMEOUT);
	_exit(EX_TEMPFAIL);
}

/* Log a PAM failure, end the transaction and give up. */
static void __dead2
pam_fail(const char *what, int rc)
{
	syslog(LOG_ERR, "%s: %s", what, pam_strerror(pamh, rc));
	pam_end(pamh, rc);
	exit(EX_NOPERM);
}

/* Record the login (USER_PROCESS) or the logout (DEAD_PROCESS) in utmpx. */
static void
record(short type, const char *user, const char *tty, const char *host, pid_t pid)
{
	struct utmpx ut;
	const char *line = tty;

	if (line == NULL)
		return;
	if (strncmp(line, _PATH_DEV, sizeof(_PATH_DEV) - 1) == 0)
		line += sizeof(_PATH_DEV) - 1;

	memset(&ut, 0, sizeof(ut));
	ut.ut_type = type;
	ut.ut_pid = pid;
	gettimeofday(&ut.ut_tv, NULL);
	strncpy(ut.ut_line, line, sizeof(ut.ut_line));
	/* The id is the line's last characters, as login(1) does on macOS. */
	size_t len = strlen(line);
	strncpy(ut.ut_id, len > sizeof(ut.ut_id) ? line + len - sizeof(ut.ut_id) : line,
	    sizeof(ut.ut_id));
	if (type == USER_PROCESS) {
		strncpy(ut.ut_user, user, sizeof(ut.ut_user));
		if (host != NULL)
			strncpy(ut.ut_host, host, sizeof(ut.ut_host));
	}
	setutxent();
	if (pututxline(&ut) == NULL)
		syslog(LOG_WARNING, "could not record the login in utmpx: %m");
	endutxent();
}

/* /etc/motd, unless the user has a ~/.hushlogin. */
static void
show_motd(const struct passwd *pw)
{
	char path[MAXPATHLEN], line[256];
	struct stat sb;
	FILE *f;

	snprintf(path, sizeof(path), "%s/.hushlogin", pw->pw_dir);
	if (stat(path, &sb) == 0 || (f = fopen(PATH_MOTD, "r")) == NULL)
		return;
	while (fgets(line, sizeof(line), f) != NULL)
		fputs(line, stdout);
	fclose(f);
}

/*
 * The shell's environment: the PAM modules' variables over a fresh one
 * (or over ours with -p).
 */
static void
setup_environment(const struct passwd *pw, const char *shell, bool preserve)
{
	char *term = getenv("TERM");
	char **pamenv;

	if (!preserve) {
		static char *empty[] = { NULL };
		if (term != NULL && (term = strdup(term)) == NULL)
			err(EX_OSERR, NULL);
		environ = empty;
	}
	setenv("HOME", pw->pw_dir, 1);
	setenv("SHELL", shell, 1);
	setenv("USER", pw->pw_name, 1);
	setenv("LOGNAME", pw->pw_name, 1);
	if (!preserve || getenv("PATH") == NULL)
		setenv("PATH", pw->pw_uid == 0 ? USER_PATH ":/usr/local/sbin" : USER_PATH, 1);
	if (!preserve && term != NULL)
		setenv("TERM", term, 1);

	if ((pamenv = pam_getenvlist(pamh)) != NULL) {
		for (char **e = pamenv; *e != NULL; e++) {
			putenv(*e);	/* the strings become the environment's */
		}
		free(pamenv);
	}
}

/* Become the user and run the login shell; does not return. */
static void __dead2
run_shell(const struct passwd *pw, const char *shell)
{
	char argv0[MAXPATHLEN];
	char *base;

	if (setlogin(pw->pw_name) != 0)
		syslog(LOG_ERR, "setlogin(%s): %m", pw->pw_name);
	if (setgid(pw->pw_gid) != 0 || initgroups(pw->pw_name, (int)pw->pw_gid) != 0 ||
	    setuid(pw->pw_uid) != 0) {
		syslog(LOG_ERR, "could not become %s: %m", pw->pw_name);
		err(EX_NOPERM, "could not become %s", pw->pw_name);
	}
	if (chdir(pw->pw_dir) != 0) {
		printf("No home directory %s!\n", pw->pw_dir);
		if (chdir("/") != 0)
			err(EX_OSERR, "/");
		printf("Logging in with home = \"/\".\n");
		setenv("HOME", "/", 1);
	}

	/* A leading '-' in argv[0] makes it a login shell. */
	if ((base = strrchr(shell, '/')) != NULL)
		base++;
	else
		base = __DECONST(char *, shell);
	snprintf(argv0, sizeof(argv0), "-%s", base);
	execl(shell, argv0, (char *)NULL);
	err(EX_OSFILE, "%s", shell);
}

int
main(int argc, char *argv[])
{
	bool preauth = false, preserve = false;
	const char *host = NULL, *tty, *shell;
	const void *item;
	char *user;
	struct passwd *pw;
	int ch, rc, status, tries;
	pid_t pid;

	openlog("login", LOG_ODELAY, LOG_AUTH);
	while ((ch = getopt(argc, argv, "fh:p")) != -1) {
		switch (ch) {
		case 'f':
			preauth = true;
			break;
		case 'h':
			if (getuid() != 0)
				errx(EX_NOPERM, "-h is for the superuser");
			host = optarg;
			break;
		case 'p':
			preserve = true;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 1)
		usage();
	user = argc == 1 ? argv[0] : NULL;

	/* Only root may vouch for a user it did not authenticate here. */
	if (preauth && getuid() != 0)
		errx(EX_NOPERM, "-f is for the superuser");

	(void)signal(SIGALRM, timed_out);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGHUP, SIG_DFL);
	alarm(LOGIN_TIMEOUT);

	tty = ttyname(STDIN_FILENO);

	for (tries = 0;; tries++) {
		if ((rc = pam_start(PAM_SERVICE, user, &conv, &pamh)) != PAM_SUCCESS)
			errx(EX_SOFTWARE, "pam_start: %s", pam_strerror(NULL, rc));
		if (tty != NULL)
			(void)pam_set_item(pamh, PAM_TTY, tty);
		if (host != NULL)
			(void)pam_set_item(pamh, PAM_RHOST, host);

		rc = preauth && user != NULL ? PAM_SUCCESS : pam_authenticate(pamh, 0);
		if (rc == PAM_SUCCESS)
			break;
		if (rc != PAM_AUTH_ERR && rc != PAM_USER_UNKNOWN && rc != PAM_MAXTRIES)
			pam_fail("pam_authenticate", rc);

		syslog(LOG_NOTICE, "login failure on %s%s%s", tty != NULL ? tty : "?",
		    host != NULL ? " from " : "", host != NULL ? host : "");
		pam_end(pamh, rc);
		pamh = NULL;
		preauth = false;
		user = NULL;
		sleep(FAIL_DELAY);
		printf("Login incorrect\n");
		if (tries + 1 >= MAX_TRIES)
			exit(EX_NOPERM);
	}

	rc = pam_acct_mgmt(pamh, 0);
	if (rc == PAM_NEW_AUTHTOK_REQD)
		rc = pam_chauthtok(pamh, PAM_CHANGE_EXPIRED_AUTHTOK);
	if (rc != PAM_SUCCESS)
		pam_fail("pam_acct_mgmt", rc);

	/* The user PAM settled on (a module may map the name). */
	if ((rc = pam_get_item(pamh, PAM_USER, &item)) != PAM_SUCCESS || item == NULL)
		pam_fail("pam_get_item(PAM_USER)", rc);
	if ((pw = getpwnam(item)) == NULL)
		pam_fail("getpwnam", PAM_USER_UNKNOWN);
	alarm(0);

	if (pw->pw_uid != 0 && access(_PATH_NOLOGIN, F_OK) == 0) {
		FILE *f = fopen(_PATH_NOLOGIN, "r");
		char line[256];
		while (f != NULL && fgets(line, sizeof(line), f) != NULL)
			fputs(line, stdout);
		if (f != NULL)
			fclose(f);
		pam_end(pamh, PAM_SUCCESS);
		exit(EX_TEMPFAIL);
	}

	if ((rc = pam_setcred(pamh, PAM_ESTABLISH_CRED)) != PAM_SUCCESS)
		pam_fail("pam_setcred", rc);
	if ((rc = pam_open_session(pamh, 0)) != PAM_SUCCESS) {
		pam_setcred(pamh, PAM_DELETE_CRED);
		pam_fail("pam_open_session", rc);
	}

	shell = pw->pw_shell != NULL && *pw->pw_shell != '\0' ? pw->pw_shell : _PATH_BSHELL;
	setup_environment(pw, shell, preserve);
	syslog(LOG_INFO, "login on %s as %s%s%s", tty != NULL ? tty : "?", pw->pw_name,
	    host != NULL ? " from " : "", host != NULL ? host : "");

	switch (pid = fork()) {
	case -1:
		syslog(LOG_ERR, "fork: %m");
		pam_close_session(pamh, 0);
		pam_setcred(pamh, PAM_DELETE_CRED);
		pam_end(pamh, PAM_SUCCESS);
		err(EX_OSERR, "fork");
	case 0:
		(void)signal(SIGQUIT, SIG_DFL);
		(void)signal(SIGINT, SIG_DFL);
		show_motd(pw);
		run_shell(pw, shell);
	}

	record(USER_PROCESS, pw->pw_name, tty, host, pid);
	/* The shell gets the terminal's signals; we only wait for it. */
	(void)signal(SIGHUP, SIG_IGN);
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
		;
	record(DEAD_PROCESS, NULL, tty, NULL, pid);

	pam_close_session(pamh, 0);
	pam_setcred(pamh, PAM_DELETE_CRED);
	pam_end(pamh, PAM_SUCCESS);
	return WIFEXITED(status) ? WEXITSTATUS(status) : EX_SOFTWARE;
}
