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
 * sysadminctl: user accounts for an administrator. Accounts live in
 * /etc/master.passwd (edited through libpw, the database rebuilt with
 * pwd_mkdb) and their groups in /etc/group: a new user gets the first free
 * uid from 501, the staff group, a home in /Users with the usual folders,
 * zsh, and a SHA-512 crypt(3) password; -admin adds it to the admin group.
 * Deleting a user ends its processes, takes it out of its groups and
 * removes its home under /Users. Like macOS, it reports on stderr, each
 * line stamped with the time and the process.
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <pwd.h>
#include <readpassphrase.h>
#include <signal.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#define GROUP_FILE	"/etc/group"
#define USERS_DIR	"/Users"
#define FIRST_UID	501
#define STAFF_GID	20
#define DEFAULT_SHELL	"/bin/zsh"

/* ---- Reporting ------------------------------------------------------------- */

static void
report(const char *fmt, ...)
{
	struct timeval tv;
	struct tm tm;
	char stamp[32];
	uint64_t tid = 0;
	va_list ap;

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm);
	strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm);
	pthread_threadid_np(NULL, &tid);
	fprintf(stderr, "%s.%03d sysadminctl[%d:%llx] ", stamp,
	    (int)(tv.tv_usec / 1000), (int)getpid(), (unsigned long long)tid);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

/* ---- Passwords --------------------------------------------------------------- */

static char *
hash_password(const char *password)
{
	static const char chars[] =
	    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	char salt[3 + 16 + 2], *hash;
	int i;

	memcpy(salt, "$6$", 3);
	for (i = 0; i < 16; i++)
		salt[3 + i] = chars[arc4random_uniform(sizeof(chars) - 1)];
	salt[3 + 16] = '$';
	salt[3 + 16 + 1] = '\0';
	hash = crypt(password, salt);
	if (hash == NULL || strncmp(hash, "$6$", 3) != 0)
		return (NULL);
	return (strdup(hash));
}

/* "-" asks for the password on the terminal. */
static const char *
password_arg(const char *arg, const char *user)
{
	static char buf[256];
	char prompt[300];

	if (arg == NULL || strcmp(arg, "-") != 0)
		return (arg);
	snprintf(prompt, sizeof(prompt), "Enter password for %s : ", user);
	if (readpassphrase(prompt, buf, sizeof(buf), RPP_REQUIRE_TTY) == NULL)
		return (NULL);
	return (buf);
}

/* ---- The account database ---------------------------------------------------- */

/* Replaces old with new (adding new when old is NULL, deleting old when new
 * is NULL) and rebuilds the databases. */
static bool
update_passwd(struct passwd *new, struct passwd *old)
{
	int pfd, tfd;

	if (pw_init(NULL, NULL) != 0) {
		report("Can't open the account database: %s", strerror(errno));
		return (false);
	}
	if ((pfd = pw_lock()) == -1) {
		report("The account database is in use: %s", strerror(errno));
		pw_fini();
		return (false);
	}
	if ((tfd = pw_tmp(-1)) == -1 || pw_copy(pfd, tfd, new, old) == -1 ||
	    pw_mkdb(new != NULL ? new->pw_name : NULL) == -1) {
		report("Can't update the account database: %s",
		    strerror(errno));
		pw_fini();
		return (false);
	}
	pw_fini();
	return (true);
}

/* Adds name to, or takes it out of, the member list of one group (or of
 * every group when group is NULL). */
static bool
edit_groups(const char *name, const char *group, bool add)
{
	char tmp[] = GROUP_FILE ".XXXXXX", line[4096];
	FILE *in, *out;
	struct stat st;
	int fd, lfd;
	bool ok = true;

	if ((lfd = open(GROUP_FILE, O_RDONLY)) < 0 || flock(lfd, LOCK_EX) != 0 ||
	    (in = fopen(GROUP_FILE, "r")) == NULL) {
		report("Can't open %s: %s", GROUP_FILE, strerror(errno));
		if (lfd >= 0)
			close(lfd);
		return (false);
	}
	fstat(fileno(in), &st);
	if ((fd = mkstemp(tmp)) < 0 || (out = fdopen(fd, "w")) == NULL) {
		report("Can't write %s: %s", GROUP_FILE, strerror(errno));
		fclose(in);
		close(lfd);
		return (false);
	}
	while (fgets(line, sizeof(line), in) != NULL) {
		char copy[4096], *fields[4], *p = copy, *m, *last;
		char members[4096] = "";
		bool present = false;
		int n = 0;

		strlcpy(copy, line, sizeof(copy));
		copy[strcspn(copy, "\n")] = '\0';
		while (n < 4 && (fields[n] = strsep(&p, ":")) != NULL)
			n++;
		if (n != 4 || copy[0] == '#' ||
		    (group != NULL && strcmp(fields[0], group) != 0)) {
			fputs(line, out);
			continue;
		}
		for (m = strtok_r(fields[3], ",", &last); m != NULL;
		    m = strtok_r(NULL, ",", &last)) {
			if (strcmp(m, name) == 0) {
				present = true;
				if (!add)
					continue;
			}
			if (members[0] != '\0')
				strlcat(members, ",", sizeof(members));
			strlcat(members, m, sizeof(members));
		}
		if (add && !present) {
			if (members[0] != '\0')
				strlcat(members, ",", sizeof(members));
			strlcat(members, name, sizeof(members));
		}
		if (!add && present)
			report("Removing %s from group %s", name, fields[0]);
		fprintf(out, "%s:%s:%s:%s\n", fields[0], fields[1], fields[2],
		    members);
	}
	fclose(in);
	fchmod(fd, st.st_mode & 07777);
	fchown(fd, st.st_uid, st.st_gid);
	if (fclose(out) != 0 || rename(tmp, GROUP_FILE) != 0) {
		report("Can't write %s: %s", GROUP_FILE, strerror(errno));
		unlink(tmp);
		ok = false;
	}
	close(lfd);
	return (ok);
}

static bool
valid_name(const char *name)
{
	const char *p;

	if (name == NULL || name[0] == '\0' || strlen(name) > 31 ||
	    !(islower((unsigned char)name[0]) || name[0] == '_'))
		return (false);
	for (p = name; *p != '\0'; p++)
		if (!islower((unsigned char)*p) && !isdigit((unsigned char)*p) &&
		    *p != '_' && *p != '.' && *p != '-')
			return (false);
	return (true);
}

/* The folders macOS gives a new home, and who may enter them. */
static void
make_home(const char *home, uid_t uid, gid_t gid)
{
	static const struct {
		const char	*name;
		mode_t		 mode;
	} folders[] = {
		{ "Desktop", 0700 }, { "Documents", 0700 }, { "Downloads", 0700 },
		{ "Library", 0700 }, { "Movies", 0700 }, { "Music", 0700 },
		{ "Pictures", 0700 }, { "Public", 0755 },
		{ "Public/Drop Box", 0733 },
	};
	char path[PATH_MAX];
	size_t i;

	if (mkdir(home, 0755) != 0 && errno != EEXIST) {
		report("Can't create home directory %s: %s", home,
		    strerror(errno));
		return;
	}
	chown(home, uid, gid);
	chmod(home, 0755);
	for (i = 0; i < sizeof(folders) / sizeof(folders[0]); i++) {
		snprintf(path, sizeof(path), "%s/%s", home, folders[i].name);
		if (mkdir(path, folders[i].mode) == 0 || errno == EEXIST) {
			chown(path, uid, gid);
			chmod(path, folders[i].mode);
		}
	}
}

/* ---- Commands ---------------------------------------------------------------- */

struct options {
	const char	*full_name;
	const char	*password;
	const char	*home;
	const char	*shell;
	long		 uid;
	bool		 admin;
	bool		 keep_home;
};

static int
add_user(const char *name, const struct options *o)
{
	struct passwd pw;
	char home[PATH_MAX], *hash = NULL;
	const char *password;
	uid_t uid;

	if (!valid_name(name)) {
		report("Invalid user name: %s", name ? name : "");
		return (1);
	}
	if (getpwnam(name) != NULL) {
		report("User %s already exists", name);
		return (1);
	}
	if (o->uid >= 0) {
		uid = (uid_t)o->uid;
		if (getpwuid(uid) != NULL) {
			report("UID %u is already in use", uid);
			return (1);
		}
	} else {
		for (uid = FIRST_UID; getpwuid(uid) != NULL; uid++)
			;
	}
	if (o->home != NULL)
		strlcpy(home, o->home, sizeof(home));
	else
		snprintf(home, sizeof(home), "%s/%s", USERS_DIR, name);
	password = password_arg(o->password, name);
	if (password != NULL && password[0] != '\0' &&
	    (hash = hash_password(password)) == NULL) {
		report("Can't hash the password");
		return (1);
	}

	report("Creating user record…");
	memset(&pw, 0, sizeof(pw));
	pw.pw_name = (char *)name;
	/* Without a password the account is created locked. */
	pw.pw_passwd = hash != NULL ? hash : "*";
	pw.pw_uid = uid;
	pw.pw_gid = STAFF_GID;
	pw.pw_class = "";
	pw.pw_gecos = (char *)(o->full_name != NULL ? o->full_name : name);
	pw.pw_dir = home;
	pw.pw_shell = (char *)(o->shell != NULL ? o->shell : DEFAULT_SHELL);
	if (!update_passwd(&pw, NULL)) {
		free(hash);
		return (1);
	}
	free(hash);
	report("Assigning UID: %u homeDir: %s", uid, home);
	make_home(home, uid, STAFF_GID);
	if (o->admin && edit_groups(name, "admin", true))
		report("Adding %s to group admin", name);
	return (0);
}

static void
kill_processes(uid_t uid)
{
	int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_UID, (int)uid };
	struct kinfo_proc *kp;
	size_t len = 0, i;

	if (sysctl(mib, 4, NULL, &len, NULL, 0) != 0 || len == 0)
		return;
	len += len / 4;
	if ((kp = malloc(len)) == NULL)
		return;
	if (sysctl(mib, 4, kp, &len, NULL, 0) == 0)
		for (i = 0; i < len / sizeof(*kp); i++)
			if (kp[i].kp_proc.p_pid > 1)
				kill(kp[i].kp_proc.p_pid, SIGKILL);
	free(kp);
}

/* Only a home in /Users, one level down, is removed. */
static void
remove_home(const char *home)
{
	const char *argv[] = { "/bin/rm", "-rf", home, NULL };
	const char *rest;
	pid_t pid;
	int status;

	if (strncmp(home, USERS_DIR "/", sizeof(USERS_DIR)) != 0 ||
	    (rest = home + sizeof(USERS_DIR))[0] == '\0' ||
	    strchr(rest, '/') != NULL || strcmp(rest, "..") == 0 ||
	    strcmp(rest, ".") == 0 || strcmp(rest, "Shared") == 0) {
		report("Keeping home directory %s", home);
		return;
	}
	report("Deleting home directory %s", home);
	if (posix_spawn(&pid, argv[0], NULL, NULL, (char *const *)argv,
	    environ) == 0)
		waitpid(pid, &status, 0);
}

static int
delete_user(const char *name, const struct options *o)
{
	struct passwd *pw;
	char *home;

	if (name == NULL || (pw = getpwnam(name)) == NULL) {
		report("User %s not found", name ? name : "");
		return (1);
	}
	if (pw->pw_uid < FIRST_UID) {
		report("%s is a system account; it is not deleted", name);
		return (1);
	}
	if ((pw = pw_dup(pw)) == NULL || (home = strdup(pw->pw_dir)) == NULL)
		err(1, NULL);
	report("Killing all processes for UID %u", pw->pw_uid);
	kill_processes(pw->pw_uid);
	report("Deleting record for %s", name);
	if (!update_passwd(NULL, pw))
		return (1);
	edit_groups(name, NULL, false);
	if (!o->keep_home)
		remove_home(home);
	free(home);
	return (0);
}

static int
reset_password(const char *name, const struct options *o)
{
	struct passwd *pw, *old;
	const char *password;
	char *hash;

	if (name == NULL || (pw = getpwnam(name)) == NULL) {
		report("User %s not found", name ? name : "");
		return (1);
	}
	password = password_arg(o->password, name);
	if (password == NULL || password[0] == '\0') {
		report("-newPassword is needed");
		return (1);
	}
	if ((hash = hash_password(password)) == NULL) {
		report("Can't hash the password");
		return (1);
	}
	if ((old = pw_dup(pw)) == NULL || (pw = pw_dup(pw)) == NULL)
		err(1, NULL);
	pw->pw_passwd = hash;
	if (!update_passwd(pw, old))
		return (1);
	report("resetPasswordFor %s: the password was changed", name);
	return (0);
}

static int
secure_token_status(const char *name)
{
	if (name == NULL || getpwnam(name) == NULL) {
		report("User %s not found", name ? name : "");
		return (1);
	}
	/* No volume encryption yet, so no user holds a secure token. */
	report("Secure token is DISABLED for user %s", name);
	return (0);
}

static int
guest_account(const char *arg)
{
	if (arg == NULL || strcmp(arg, "status") != 0) {
		report("-guestAccount: only status is supported");
		return (1);
	}
	report("Guest account %s.", getpwnam("Guest") != NULL ? "enabled" :
	    "disabled");
	return (0);
}

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: sysadminctl\n"
	    "\t-deleteUser <user name> [-secure || -keepHome]\n"
	    "\t-resetPasswordFor <local user name> -newPassword <new password> "
	    "[-passwordHint <password hint>]\n"
	    "\t-addUser <user name> [-fullName <full name>] [-UID <user ID>] "
	    "[-shell <path to shell>] [-password <user password>] "
	    "[-hint <user hint>] [-home <full path to home>] [-admin] "
	    "[-picture <full path to user image>]\n"
	    "\t-secureTokenStatus <user name>\n"
	    "\t-guestAccount status\n\n"
	    "Pass '-' instead of password in commands above to request "
	    "prompt.\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct options o = { .uid = -1 };
	const char *cmd = NULL, *target = NULL;
	int i;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];
		const char *v = i + 1 < argc ? argv[i + 1] : NULL;

#define TAKES(field)	do { if (v == NULL) usage(); field = v; i++; } while (0)
		if (strcmp(a, "-addUser") == 0 || strcmp(a, "-deleteUser") == 0 ||
		    strcmp(a, "-resetPasswordFor") == 0 ||
		    strcmp(a, "-secureTokenStatus") == 0 ||
		    strcmp(a, "-guestAccount") == 0) {
			if (cmd != NULL)
				usage();
			cmd = a;
			TAKES(target);
		} else if (strcmp(a, "-fullName") == 0) {
			TAKES(o.full_name);
		} else if (strcmp(a, "-password") == 0 ||
		    strcmp(a, "-newPassword") == 0) {
			TAKES(o.password);
		} else if (strcmp(a, "-home") == 0) {
			TAKES(o.home);
		} else if (strcmp(a, "-shell") == 0) {
			TAKES(o.shell);
		} else if (strcmp(a, "-UID") == 0) {
			const char *u;
			char *end;

			TAKES(u);
			o.uid = strtol(u, &end, 10);
			if (*end != '\0' || o.uid < 0)
				usage();
		} else if (strcmp(a, "-admin") == 0) {
			o.admin = true;
		} else if (strcmp(a, "-keepHome") == 0) {
			o.keep_home = true;
		} else if (strcmp(a, "-secure") == 0) {
			continue;	/* removing the home is all there is */
		} else if (strcmp(a, "-hint") == 0 ||
		    strcmp(a, "-passwordHint") == 0 ||
		    strcmp(a, "-picture") == 0 ||
		    strcmp(a, "-adminUser") == 0 ||
		    strcmp(a, "-adminPassword") == 0) {
			const char *ignored;

			TAKES(ignored);	/* no hints, pictures or tokens yet */
			(void)ignored;
		} else {
			usage();
		}
#undef TAKES
	}
	if (cmd == NULL)
		usage();
	if (geteuid() != 0) {
		report("sysadminctl needs administrator access: run it as root");
		return (1);
	}
	if (strcmp(cmd, "-addUser") == 0)
		return (add_user(target, &o));
	if (strcmp(cmd, "-deleteUser") == 0)
		return (delete_user(target, &o));
	if (strcmp(cmd, "-resetPasswordFor") == 0)
		return (reset_password(target, &o));
	if (strcmp(cmd, "-secureTokenStatus") == 0)
		return (secure_token_status(target));
	return (guest_account(target));
}
