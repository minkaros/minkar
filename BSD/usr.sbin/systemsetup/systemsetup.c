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
 * systemsetup: the machine's settings from the command line, for an
 * administrator. The date, time and time zone (settimeofday, tzlink), the
 * computer and local subnet names (SystemConfiguration's preferences),
 * remote login (sshd's launchd job), the network time server
 * (/etc/ntp.conf). What this system does not have yet (power management's
 * sleep and restart settings, choosing the startup disk, a network time
 * service, remote Apple events) is reported as not supported, its sleep
 * timers as never.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#define ZONEINFO	"/var/db/timezone/zoneinfo"
#define LOCALTIME	"/etc/localtime"
#define NTP_CONF	"/etc/ntp.conf"
#define SSHD_PLIST	"/System/Library/LaunchDaemons/com.openssh.sshd.plist"
#define SSHD_LABEL	"com.openssh.sshd"

static bool force;

/* ---- Helpers ------------------------------------------------------------- */

static int
run(const char *const argv[], bool quiet)
{
	posix_spawn_file_actions_t fa;
	pid_t pid;
	int status;

	posix_spawn_file_actions_init(&fa);
	if (quiet) {
		posix_spawn_file_actions_addopen(&fa, STDOUT_FILENO, "/dev/null",
		    O_WRONLY, 0);
		posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null",
		    O_WRONLY, 0);
	}
	if (posix_spawn(&pid, argv[0], &fa, NULL, (char *const *)argv,
	    environ) != 0) {
		posix_spawn_file_actions_destroy(&fa);
		return (-1);
	}
	posix_spawn_file_actions_destroy(&fa);
	while (waitpid(pid, &status, 0) < 0)
		if (errno != EINTR)
			return (-1);
	return (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

static bool
on_off(const char *arg, bool *value)
{
	if (strcasecmp(arg, "on") == 0 || strcasecmp(arg, "yes") == 0) {
		*value = true;
		return (true);
	}
	if (strcasecmp(arg, "off") == 0 || strcasecmp(arg, "no") == 0) {
		*value = false;
		return (true);
	}
	return (false);
}

static void
cfprint(const char *label, CFStringRef s)
{
	char buf[1024] = "";

	if (s != NULL)
		CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8);
	printf("%s: %s\n", label, buf);
}

static int
not_supported(const char *cmd)
{
	(void)cmd;
	printf("Not supported on this machine.\n");
	return (0);
}

/* ---- Date, time and time zone -------------------------------------------- */

static int
get_date(const char *cmd)
{
	char buf[32];
	time_t t = time(NULL);

	(void)cmd;
	strftime(buf, sizeof(buf), "%m/%d/%Y", localtime(&t));
	printf("Date: %s\n", buf);
	return (0);
}

static int
get_time(const char *cmd)
{
	char buf[32];
	time_t t = time(NULL);

	(void)cmd;
	strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&t));
	printf("Time: %s\n", buf);
	return (0);
}

static int
set_clock(struct tm *tm)
{
	struct timeval tv;
	time_t t;

	tm->tm_isdst = -1;
	if ((t = mktime(tm)) == (time_t)-1)
		return (-1);
	tv.tv_sec = t;
	tv.tv_usec = 0;
	return (settimeofday(&tv, NULL));
}

/* mm:dd:yy (or mm:dd:yyyy), the time of day kept. */
static int
set_date(const char *cmd, const char *arg)
{
	time_t now = time(NULL);
	struct tm tm = *localtime(&now);
	int m, d, y;

	if (arg == NULL || sscanf(arg, "%d:%d:%d", &m, &d, &y) != 3 ||
	    m < 1 || m > 12 || d < 1 || d > 31 || y < 0) {
		fprintf(stderr, "%s: invalid date, use mm:dd:yy.\n", cmd);
		return (1);
	}
	tm.tm_mon = m - 1;
	tm.tm_mday = d;
	tm.tm_year = (y < 100 ? y + 2000 : y) - 1900;
	if (set_clock(&tm) != 0) {
		warn("%s", cmd);
		return (1);
	}
	return (0);
}

/* hh:mm:ss, the date kept. */
static int
set_time(const char *cmd, const char *arg)
{
	time_t now = time(NULL);
	struct tm tm = *localtime(&now);
	int h, m, s = 0;

	if (arg == NULL || sscanf(arg, "%d:%d:%d", &h, &m, &s) < 2 ||
	    h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 60) {
		fprintf(stderr, "%s: invalid time, use hh:mm:ss.\n", cmd);
		return (1);
	}
	tm.tm_hour = h;
	tm.tm_min = m;
	tm.tm_sec = s;
	if (set_clock(&tm) != 0) {
		warn("%s", cmd);
		return (1);
	}
	return (0);
}

static int
get_timezone(const char *cmd)
{
	char link[PATH_MAX];
	ssize_t n;
	const char *zone;

	(void)cmd;
	n = readlink(LOCALTIME, link, sizeof(link) - 1);
	if (n < 0) {
		printf("Time Zone: GMT\n");
		return (0);
	}
	link[n] = '\0';
	zone = strstr(link, "zoneinfo/");
	printf("Time Zone: %s\n", zone != NULL ? zone + 9 : link);
	return (0);
}

/* The zones: the regular files under the zone directory, by path. */
static int
cmp_str(const void *a, const void *b)
{
	return (strcmp(*(char *const *)a, *(char *const *)b));
}

static void
collect_zones(const char *dir, const char *prefix, char ***zones, size_t *n)
{
	DIR *d = opendir(dir);
	struct dirent *de;

	if (d == NULL)
		return;
	while ((de = readdir(d)) != NULL) {
		char path[PATH_MAX], name[PATH_MAX];
		struct stat st;

		/* "+VERSION", the tables and the legacy aliases stay out. */
		if (de->d_name[0] == '.' || de->d_name[0] == '+' ||
		    strchr(de->d_name, '.') != NULL ||
		    (prefix[0] == '\0' && !(de->d_name[0] >= 'A' &&
		    de->d_name[0] <= 'Z')))
			continue;
		snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
		snprintf(name, sizeof(name), "%s%s", prefix, de->d_name);
		if (stat(path, &st) != 0)
			continue;
		if (S_ISDIR(st.st_mode)) {
			char sub[PATH_MAX];

			if (strcmp(de->d_name, "Etc") == 0 ||
			    strcmp(de->d_name, "SystemV") == 0 ||
			    strcmp(de->d_name, "US") == 0)
				continue;
			snprintf(sub, sizeof(sub), "%s/", name);
			collect_zones(path, sub, zones, n);
		} else if (S_ISREG(st.st_mode) && strchr(name, '/') != NULL) {
			*zones = realloc(*zones, sizeof(**zones) * (*n + 1));
			if (*zones == NULL)
				err(1, NULL);
			(*zones)[(*n)++] = strdup(name);
		}
	}
	closedir(d);
}

static char **
zones(size_t *n)
{
	char **z = NULL;

	*n = 0;
	collect_zones(ZONEINFO, "", &z, n);
	qsort(z, *n, sizeof(*z), cmp_str);
	return (z);
}

static int
list_timezones(const char *cmd)
{
	size_t n, i;
	char **z = zones(&n);

	(void)cmd;
	printf("Time Zones:\n");
	for (i = 0; i < n; i++)
		printf("    %s\n", z[i]);
	return (0);
}

static int
set_timezone(const char *cmd, const char *arg)
{
	size_t n, i;
	char **z = zones(&n);
	const char *argv[] = { "/usr/sbin/tzlink", arg, NULL };
	bool valid = false;

	for (i = 0; arg != NULL && i < n && !valid; i++)
		valid = strcmp(z[i], arg) == 0;
	/* A zone outside the list (UTC, GMT) is fine when its file exists. */
	if (arg != NULL && !valid && strstr(arg, "..") == NULL &&
	    arg[0] != '/') {
		char path[PATH_MAX];
		struct stat st;

		snprintf(path, sizeof(path), "%s/%s", ZONEINFO, arg);
		valid = stat(path, &st) == 0 && S_ISREG(st.st_mode);
	}
	if (!valid) {
		fprintf(stderr, "%s: invalid time zone, see -listtimezones for "
		    "the valid ones.\n", cmd);
		return (1);
	}
	if (run(argv, false) != 0) {
		fprintf(stderr, "%s: could not set the time zone.\n", cmd);
		return (1);
	}
	return (0);
}

/* ---- Network time --------------------------------------------------------- */

static int
get_networktime(const char *cmd)
{
	(void)cmd;
	printf("Network Time: Off\n");	/* no network time service yet */
	return (0);
}

static int
get_networktimeserver(const char *cmd)
{
	FILE *f = fopen(NTP_CONF, "r");
	char line[512], server[256] = "";

	(void)cmd;
	while (f != NULL && fgets(line, sizeof(line), f) != NULL)
		if (sscanf(line, "server %255s", server) == 1)
			break;
	if (f != NULL)
		fclose(f);
	printf("Network Time Server: %s\n", server);
	return (0);
}

/* /etc/ntp.conf names the one server, as on macOS. */
static int
set_networktimeserver(const char *cmd, const char *arg)
{
	char tmp[] = NTP_CONF ".XXXXXX";
	FILE *f;
	int fd;

	if (arg == NULL || arg[0] == '\0' || strpbrk(arg, " \t\n") != NULL) {
		fprintf(stderr, "%s: a server name is needed.\n", cmd);
		return (1);
	}
	if ((fd = mkstemp(tmp)) < 0 || (f = fdopen(fd, "w")) == NULL) {
		warn("%s", NTP_CONF);
		return (1);
	}
	fprintf(f, "server %s\n", arg);
	fchmod(fd, 0644);
	if (fclose(f) != 0 || rename(tmp, NTP_CONF) != 0) {
		warn("%s", NTP_CONF);
		unlink(tmp);
		return (1);
	}
	return (0);
}

/* ---- Names ---------------------------------------------------------------- */

static int
get_computername(const char *cmd)
{
	CFStringRef name = SCDynamicStoreCopyComputerName(NULL, NULL);

	(void)cmd;
	cfprint("Computer Name", name);
	if (name != NULL)
		CFRelease(name);
	return (0);
}

static int
get_localsubnetname(const char *cmd)
{
	CFStringRef name = SCDynamicStoreCopyLocalHostName(NULL);

	(void)cmd;
	cfprint("Local Subnet Name", name);
	if (name != NULL)
		CFRelease(name);
	return (0);
}

static int
set_name(const char *cmd, const char *arg, bool local)
{
	SCPreferencesRef prefs;
	CFStringRef name;
	Boolean ok;

	if (arg == NULL || arg[0] == '\0') {
		fprintf(stderr, "%s: a name is needed.\n", cmd);
		return (1);
	}
	prefs = SCPreferencesCreate(NULL, CFSTR("systemsetup"), NULL);
	name = CFStringCreateWithCString(NULL, arg, kCFStringEncodingUTF8);
	if (prefs == NULL || name == NULL) {
		fprintf(stderr, "%s: the system preferences are not "
		    "available.\n", cmd);
		return (1);
	}
	if (local)
		ok = SCPreferencesSetLocalHostName(prefs, name);
	else
		ok = SCPreferencesSetComputerName(prefs, name,
		    kCFStringEncodingUTF8);
	ok = ok && SCPreferencesCommitChanges(prefs) &&
	    SCPreferencesApplyChanges(prefs);
	CFRelease(name);
	CFRelease(prefs);
	if (!ok) {
		fprintf(stderr, "%s: %s.\n", cmd, SCErrorString(SCError()));
		return (1);
	}
	return (0);
}

static int
set_computername(const char *cmd, const char *arg)
{
	return (set_name(cmd, arg, false));
}

static int
set_localsubnetname(const char *cmd, const char *arg)
{
	return (set_name(cmd, arg, true));
}

/* ---- Remote login ---------------------------------------------------------- */

static bool
remote_login(void)
{
	const char *argv[] = { "/bin/launchctl", "list", SSHD_LABEL, NULL };

	return (run(argv, true) == 0);
}

static int
get_remotelogin(const char *cmd)
{
	(void)cmd;
	printf("Remote Login: %s\n", remote_login() ? "On" : "Off");
	return (0);
}

static int
set_remotelogin(const char *cmd, const char *arg)
{
	const char *argv[] = { "/bin/launchctl", NULL, "-w", SSHD_PLIST, NULL };
	bool on;

	if (arg == NULL || !on_off(arg, &on)) {
		fprintf(stderr, "%s: use on or off.\n", cmd);
		return (1);
	}
	if (on == remote_login())
		return (0);
	if (!on && !force) {
		char answer[16] = "";

		printf("Do you really want to turn remote login off? If you do, "
		    "you will lose this connection and can only turn it back "
		    "on locally at the server (yes/no)? ");
		fflush(stdout);
		if (fgets(answer, sizeof(answer), stdin) == NULL ||
		    strncasecmp(answer, "yes", 3) != 0)
			return (0);
	}
	argv[1] = on ? "load" : "unload";
	if (run(argv, true) != 0 || remote_login() != on) {
		fprintf(stderr, "%s: could not turn remote login %s.\n", cmd,
		    on ? "on" : "off");
		return (1);
	}
	return (0);
}

/* ---- What this machine does not have yet ---------------------------------- */

static int
get_never(const char *cmd)
{
	static const struct {
		const char	*cmd;
		const char	*label;
	} sleeps[] = {
		{ "-getcomputersleep",	"Computer Sleep" },
		{ "-getdisplaysleep",	"Display Sleep" },
		{ "-getharddisksleep",	"Hard Disk Sleep" },
	};
	size_t i;

	if (strcmp(cmd, "-getsleep") == 0) {
		printf("Computer Sleep: Never\nDisplay Sleep: Never\n"
		    "Hard Disk Sleep: Never\n");
		return (0);
	}
	for (i = 0; i < sizeof(sleeps) / sizeof(sleeps[0]); i++)
		if (strcmp(cmd, sleeps[i].cmd) == 0)
			printf("%s: Never\n", sleeps[i].label);
	return (0);
}

static int
get_remoteappleevents(const char *cmd)
{
	(void)cmd;
	printf("Remote Apple Events: Off\n");
	return (0);
}

static int
set_not_supported(const char *cmd, const char *arg)
{
	(void)arg;
	return (not_supported(cmd));
}

/* ---- Commands -------------------------------------------------------------- */

static const struct command {
	const char	*name;
	const char	*args;		/* NULL: takes no argument */
	int		(*get)(const char *);
	int		(*set)(const char *, const char *);
} commands[] = {
	{ "-getdate",			NULL,		get_date, NULL },
	{ "-setdate",			"<mm:dd:yy>",	NULL, set_date },
	{ "-gettime",			NULL,		get_time, NULL },
	{ "-settime",			"<hh:mm:ss>",	NULL, set_time },
	{ "-gettimezone",		NULL,		get_timezone, NULL },
	{ "-listtimezones",		NULL,		list_timezones, NULL },
	{ "-settimezone",		"<timezone>",	NULL, set_timezone },
	{ "-getusingnetworktime",	NULL,		get_networktime, NULL },
	{ "-setusingnetworktime",	"<on off>",	NULL, set_not_supported },
	{ "-getnetworktimeserver",	NULL,		get_networktimeserver, NULL },
	{ "-setnetworktimeserver",	"<timeserver>",	NULL, set_networktimeserver },
	{ "-getsleep",			NULL,		get_never, NULL },
	{ "-setsleep",			"<minutes>",	NULL, set_not_supported },
	{ "-getcomputersleep",		NULL,		get_never, NULL },
	{ "-setcomputersleep",		"<minutes>",	NULL, set_not_supported },
	{ "-getdisplaysleep",		NULL,		get_never, NULL },
	{ "-setdisplaysleep",		"<minutes>",	NULL, set_not_supported },
	{ "-getharddisksleep",		NULL,		get_never, NULL },
	{ "-setharddisksleep",		"<minutes>",	NULL, set_not_supported },
	{ "-getwakeonmodem",		NULL,		not_supported, NULL },
	{ "-setwakeonmodem",		"<on off>",	NULL, set_not_supported },
	{ "-getwakeonnetworkaccess",	NULL,		not_supported, NULL },
	{ "-setwakeonnetworkaccess",	"<on off>",	NULL, set_not_supported },
	{ "-getrestartpowerfailure",	NULL,		not_supported, NULL },
	{ "-setrestartpowerfailure",	"<on off>",	NULL, set_not_supported },
	{ "-getrestartfreeze",		NULL,		not_supported, NULL },
	{ "-setrestartfreeze",		"<on off>",	NULL, set_not_supported },
	{ "-getallowpowerbuttontosleepcomputer", NULL,	not_supported, NULL },
	{ "-setallowpowerbuttontosleepcomputer", "<on off>", NULL,
	    set_not_supported },
	{ "-getremotelogin",		NULL,		get_remotelogin, NULL },
	{ "-setremotelogin",		"<on off>",	NULL, set_remotelogin },
	{ "-getremoteappleevents",	NULL,		get_remoteappleevents, NULL },
	{ "-setremoteappleevents",	"<on off>",	NULL, set_not_supported },
	{ "-getcomputername",		NULL,		get_computername, NULL },
	{ "-setcomputername",		"<computername>", NULL, set_computername },
	{ "-getlocalsubnetname",	NULL,		get_localsubnetname, NULL },
	{ "-setlocalsubnetname",	"<name>",	NULL, set_localsubnetname },
	{ "-getstartupdisk",		NULL,		not_supported, NULL },
	{ "-liststartupdisks",		NULL,		not_supported, NULL },
	{ "-setstartupdisk",		"<disk>",	NULL, set_not_supported },
	{ "-getwaitforstartupafterpowerfailure", NULL,	not_supported, NULL },
	{ "-setwaitforstartupafterpowerfailure", "<seconds>", NULL,
	    set_not_supported },
	{ "-getdisablekeyboardwhenenclosurelockisengaged", NULL,
	    not_supported, NULL },
	{ "-setdisablekeyboardwhenenclosurelockisengaged", "<yes no>", NULL,
	    set_not_supported },
};
#define NCOMMANDS	(sizeof(commands) / sizeof(commands[0]))

static void
print_commands(void)
{
	size_t i;

	for (i = 0; i < NCOMMANDS; i++)
		printf("systemsetup %s%s%s\n", commands[i].name,
		    commands[i].args ? " " : "",
		    commands[i].args ? commands[i].args : "");
}

static void
usage(void)
{
	fprintf(stderr, "usage: systemsetup [-f] -command [argument]\n"
	    "       systemsetup -help | -printCommands\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int i, rc = 0, ran = 0;

	if (argc < 2)
		usage();
	for (i = 1; i < argc; i++) {
		const char *a = argv[i];
		size_t c;

		if (strcmp(a, "-f") == 0) {
			force = true;
			continue;
		}
		if (strcmp(a, "-help") == 0 || strcmp(a, "-printCommands") == 0) {
			print_commands();
			return (0);
		}
		for (c = 0; c < NCOMMANDS; c++)
			if (strcasecmp(a, commands[c].name) == 0)
				break;
		if (c == NCOMMANDS) {
			fprintf(stderr, "%s: unknown command; see "
			    "systemsetup -printCommands.\n", a);
			return (1);
		}
		if (getuid() != 0 && geteuid() != 0) {
			printf("You need administrator access to run this "
			    "tool... exiting!\n");
			return (1);
		}
		if (commands[c].set != NULL) {
			const char *arg = i + 1 < argc ? argv[++i] : NULL;

			rc |= commands[c].set(commands[c].name, arg);
		} else {
			rc |= commands[c].get(commands[c].name);
		}
		ran++;
	}
	if (ran == 0)
		usage();
	return (rc);
}
