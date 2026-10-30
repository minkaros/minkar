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
 * emond: the event monitor. Rules (property lists in the rules directory,
 * each an array of dictionaries) say which events they take (eventTypes),
 * what an event must hold (criterion: key, operator, value; all of them,
 * or any with allowPartialCriterionMatch) and what to do: Log a message,
 * RunCommand, SendEmail. Messages and arguments may name ${event.key}
 * and ${builtin:now}, ${builtin:hostname}.
 *
 * Events: "startup" when emond starts, and the property lists clients
 * leave in the event queue directory (a dictionary with eventType and
 * the event's keys), handled and removed. launchd starts emond when its
 * clients directory is not empty; emond stays while it is (looking every
 * few seconds), and rereads its rules on SIGHUP.
 */

#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#define RULES_DIR	"/etc/emond.d/rules"
#define CLIENTS_DIR	"/var/db/emondClients"
#define EVENTS_DIR	"/var/db/emond/events"
#define POLL_SECONDS	5

static CFMutableArrayRef rules;		/* the enabled rules */
static volatile sig_atomic_t reload, stop;
static bool foreground;
static const char *rules_dir = RULES_DIR;
static const char *events_dir = EVENTS_DIR;
static const char *clients_dir = CLIENTS_DIR;

static void
note(int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (foreground) {
		va_list aq;

		va_copy(aq, ap);
		vfprintf(stderr, fmt, aq);
		fputc('\n', stderr);
		va_end(aq);
	}
	vsyslog(pri, fmt, ap);
	va_end(ap);
}

static CFPropertyListRef
read_plist(const char *path)
{
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL,
	    (const UInt8 *)path, (CFIndex)strlen(path), false);
	CFReadStreamRef rs = CFReadStreamCreateWithFile(NULL, url);
	CFPropertyListRef pl = NULL;

	if (rs != NULL && CFReadStreamOpen(rs)) {
		pl = CFPropertyListCreateWithStream(NULL, rs, 0,
		    kCFPropertyListImmutable, NULL, NULL);
		CFReadStreamClose(rs);
	}
	if (rs != NULL)
		CFRelease(rs);
	CFRelease(url);
	return (pl);
}

static bool
cstr(CFTypeRef v, char *buf, size_t len)
{
	buf[0] = '\0';
	if (v == NULL)
		return (false);
	if (CFGetTypeID(v) == CFStringGetTypeID())
		return (CFStringGetCString(v, buf, (CFIndex)len,
		    kCFStringEncodingUTF8));
	if (CFGetTypeID(v) == CFNumberGetTypeID()) {
		double d = 0;

		CFNumberGetValue(v, kCFNumberDoubleType, &d);
		if (d == (long long)d)
			snprintf(buf, len, "%lld", (long long)d);
		else
			snprintf(buf, len, "%g", d);
		return (true);
	}
	if (CFGetTypeID(v) == CFBooleanGetTypeID()) {
		strlcpy(buf, CFBooleanGetValue(v) ? "true" : "false", len);
		return (true);
	}
	return (false);
}

static CFTypeRef
get(CFDictionaryRef d, const char *key)
{
	CFStringRef k = CFStringCreateWithCString(NULL, key,
	    kCFStringEncodingUTF8);
	CFTypeRef v = d != NULL && CFGetTypeID(d) == CFDictionaryGetTypeID() ?
	    CFDictionaryGetValue(d, k) : NULL;

	CFRelease(k);
	return (v);
}

/* "a.b.c" in nested dictionaries. */
static CFTypeRef
lookup(CFDictionaryRef d, const char *path)
{
	char buf[256], *p = buf, *part;
	CFTypeRef v = d;

	strlcpy(buf, path, sizeof(buf));
	while (v != NULL && (part = strsep(&p, ".")) != NULL)
		v = get(v, part);
	return (v);
}

static bool
truth(CFTypeRef v, bool dflt)
{
	if (v == NULL)
		return (dflt);
	if (CFGetTypeID(v) == CFBooleanGetTypeID())
		return (CFBooleanGetValue(v));
	if (CFGetTypeID(v) == CFNumberGetTypeID()) {
		int n = 0;

		CFNumberGetValue(v, kCFNumberIntType, &n);
		return (n != 0);
	}
	return (dflt);
}

/* ---- Rules ------------------------------------------------------------------- */

static void
load_rules(void)
{
	struct dirent **names;
	int n, i;

	if (rules != NULL)
		CFRelease(rules);
	rules = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	n = scandir(rules_dir, &names, NULL, alphasort);
	for (i = 0; i < n; i++) {
		size_t len = strlen(names[i]->d_name);
		char path[PATH_MAX];
		CFPropertyListRef pl;
		CFIndex j;

		if (len > 6 && strcmp(names[i]->d_name + len - 6, ".plist") == 0) {
			snprintf(path, sizeof(path), "%s/%s", rules_dir,
			    names[i]->d_name);
			pl = read_plist(path);
			if (pl == NULL || CFGetTypeID(pl) != CFArrayGetTypeID()) {
				note(LOG_WARNING, "%s: not an array of rules", path);
			} else {
				for (j = 0; j < CFArrayGetCount(pl); j++) {
					CFTypeRef r = CFArrayGetValueAtIndex(pl, j);

					if (CFGetTypeID(r) == CFDictionaryGetTypeID() &&
					    truth(get(r, "enabled"), true))
						CFArrayAppendValue(rules, r);
				}
			}
			if (pl != NULL)
				CFRelease(pl);
		}
		free(names[i]);
	}
	if (n >= 0)
		free(names);
	note(LOG_INFO, "%ld rules loaded", (long)CFArrayGetCount(rules));
}

static bool
takes_event(CFDictionaryRef rule, const char *type)
{
	CFTypeRef types = get(rule, "eventTypes");
	char buf[128];
	CFIndex i;

	if (types == NULL || CFGetTypeID(types) != CFArrayGetTypeID())
		return (false);
	for (i = 0; i < CFArrayGetCount(types); i++)
		if (cstr(CFArrayGetValueAtIndex(types, i), buf, sizeof(buf)) &&
		    strcmp(buf, type) == 0)
			return (true);
	return (false);
}

static bool
criterion_holds(CFDictionaryRef c, CFDictionaryRef event)
{
	char key[256], op[64], want[1024], have[1024];
	CFTypeRef v;
	char *end;
	double a, b;
	bool numeric;

	if (!cstr(get(c, "key"), key, sizeof(key)))
		return (false);
	if (!cstr(get(c, "operator"), op, sizeof(op)))
		strlcpy(op, "Equal", sizeof(op));
	v = lookup(event, key);
	if (strcmp(op, "Exists") == 0)
		return (v != NULL);
	if (strcmp(op, "NotExists") == 0)
		return (v == NULL);
	if (!cstr(v, have, sizeof(have)))
		return (strcmp(op, "NotEqual") == 0);
	cstr(get(c, "value"), want, sizeof(want));
	a = strtod(have, &end);
	numeric = *have != '\0' && *end == '\0';
	b = strtod(want, &end);
	numeric = numeric && *want != '\0' && *end == '\0';
	if (strcmp(op, "Equal") == 0)
		return (numeric ? a == b : strcmp(have, want) == 0);
	if (strcmp(op, "NotEqual") == 0)
		return (numeric ? a != b : strcmp(have, want) != 0);
	if (strcmp(op, "Contains") == 0)
		return (strstr(have, want) != NULL);
	if (strcmp(op, "BeginsWith") == 0)
		return (strncmp(have, want, strlen(want)) == 0);
	if (strcmp(op, "EndsWith") == 0)
		return (strlen(have) >= strlen(want) &&
		    strcmp(have + strlen(have) - strlen(want), want) == 0);
	if (strcmp(op, "GreaterThan") == 0)
		return (numeric ? a > b : strcmp(have, want) > 0);
	if (strcmp(op, "LessThan") == 0)
		return (numeric ? a < b : strcmp(have, want) < 0);
	if (strcmp(op, "GreaterThanOrEqual") == 0)
		return (numeric ? a >= b : strcmp(have, want) >= 0);
	if (strcmp(op, "LessThanOrEqual") == 0)
		return (numeric ? a <= b : strcmp(have, want) <= 0);
	note(LOG_WARNING, "unknown operator %s", op);
	return (false);
}

static bool
rule_matches(CFDictionaryRef rule, CFDictionaryRef event)
{
	CFTypeRef crit = get(rule, "criterion");
	bool partial = truth(get(rule, "allowPartialCriterionMatch"), false);
	CFIndex i, n;
	int held = 0;

	if (crit == NULL || CFGetTypeID(crit) != CFArrayGetTypeID() ||
	    (n = CFArrayGetCount(crit)) == 0)
		return (true);
	for (i = 0; i < n; i++)
		if (criterion_holds(CFArrayGetValueAtIndex(crit, i), event))
			held++;
	return (partial ? held > 0 : held == n);
}

/* ---- Actions ------------------------------------------------------------------ */

/* ${event.key}, ${builtin:now}, ${builtin:hostname} */
static void
expand(const char *in, CFDictionaryRef event, char *out, size_t len)
{
	size_t o = 0;

	while (*in != '\0' && o + 1 < len) {
		const char *close;

		if (in[0] == '$' && in[1] == '{' &&
		    (close = strchr(in, '}')) != NULL) {
			char name[256], val[1024] = "";
			size_t n = (size_t)(close - in - 2);

			if (n >= sizeof(name))
				n = sizeof(name) - 1;
			memcpy(name, in + 2, n);
			name[n] = '\0';
			if (strcmp(name, "builtin:now") == 0) {
				time_t t = time(NULL);

				strftime(val, sizeof(val), "%Y-%m-%d %H:%M:%S",
				    localtime(&t));
			} else if (strcmp(name, "builtin:hostname") == 0) {
				gethostname(val, sizeof(val));
			} else if (strncmp(name, "event.", 6) == 0) {
				cstr(lookup(event, name + 6), val, sizeof(val));
			}
			o += strlcpy(out + o, val, len - o);
			if (o >= len)
				o = len - 1;
			in = close + 1;
		} else {
			out[o++] = *in++;
		}
	}
	out[o] = '\0';
}

static void
spawn_wait(const char **argv, const char *input)
{
	posix_spawn_file_actions_t fa;
	pid_t pid;
	int fds[2] = { -1, -1 }, status;

	posix_spawn_file_actions_init(&fa);
	if (input != NULL && pipe(fds) == 0) {
		posix_spawn_file_actions_adddup2(&fa, fds[0], STDIN_FILENO);
		posix_spawn_file_actions_addclose(&fa, fds[0]);
		posix_spawn_file_actions_addclose(&fa, fds[1]);
	}
	if (posix_spawn(&pid, argv[0], &fa, NULL, (char *const *)argv,
	    environ) != 0) {
		note(LOG_ERR, "%s: %s", argv[0], strerror(errno));
		pid = -1;
	}
	posix_spawn_file_actions_destroy(&fa);
	if (fds[0] >= 0) {
		close(fds[0]);
		if (pid > 0)
			write(fds[1], input, strlen(input));
		close(fds[1]);
	}
	if (pid > 0)
		while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
			;
}

static int
log_level(CFDictionaryRef action)
{
	char lvl[32];

	cstr(get(action, "logLevel"), lvl, sizeof(lvl));
	if (strcasecmp(lvl, "Emergency") == 0) return (LOG_EMERG);
	if (strcasecmp(lvl, "Alert") == 0) return (LOG_ALERT);
	if (strcasecmp(lvl, "Critical") == 0) return (LOG_CRIT);
	if (strcasecmp(lvl, "Error") == 0) return (LOG_ERR);
	if (strcasecmp(lvl, "Warning") == 0) return (LOG_WARNING);
	if (strcasecmp(lvl, "Info") == 0) return (LOG_INFO);
	if (strcasecmp(lvl, "Debug") == 0) return (LOG_DEBUG);
	return (LOG_NOTICE);
}

static void
run_action(CFDictionaryRef action, CFDictionaryRef event, const char *rule)
{
	char type[64], text[4096], buf[1024];

	cstr(get(action, "type"), type, sizeof(type));
	if (strcmp(type, "Log") == 0) {
		cstr(get(action, "message"), buf, sizeof(buf));
		expand(buf, event, text, sizeof(text));
		syslog(log_level(action), "%s", text);
		if (foreground)
			fprintf(stderr, "%s\n", text);
	} else if (strcmp(type, "RunCommand") == 0) {
		CFTypeRef args = get(action, "arguments");
		const char *argv[66];
		char user[64], *copies[66];
		int argc = 0, i;

		if (!cstr(get(action, "command"), buf, sizeof(buf)) ||
		    buf[0] != '/') {
			note(LOG_ERR, "%s: RunCommand needs an absolute command",
			    rule);
			return;
		}
		if (cstr(get(action, "user"), user, sizeof(user)) &&
		    user[0] != '\0' && strcmp(user, "root") != 0) {
			/* Another user's command runs through su. */
			if (getpwnam(user) == NULL) {
				note(LOG_ERR, "%s: no user %s", rule, user);
				return;
			}
			argv[argc++] = "/usr/bin/su";
			argv[argc++] = "-m";
			argv[argc++] = user;
			argv[argc++] = "-c";
			/* The command and its arguments, quoted, for su's shell. */
			snprintf(text, sizeof(text), "'%s'", buf);
			for (i = 0; args != NULL && CFGetTypeID(args) ==
			    CFArrayGetTypeID() && i < CFArrayGetCount(args); i++) {
				char a[1024], e[1024];

				cstr(CFArrayGetValueAtIndex(args, i), a, sizeof(a));
				expand(a, event, e, sizeof(e));
				if (strchr(e, '\'') != NULL)
					continue;
				strlcat(text, " '", sizeof(text));
				strlcat(text, e, sizeof(text));
				strlcat(text, "'", sizeof(text));
			}
			argv[argc++] = text;
			argv[argc] = NULL;
			spawn_wait(argv, NULL);
			return;
		}
		argv[argc++] = buf;
		for (i = 0; args != NULL && CFGetTypeID(args) == CFArrayGetTypeID() &&
		    i < CFArrayGetCount(args) && argc < 64; i++) {
			char a[1024];

			copies[argc] = malloc(1024);
			cstr(CFArrayGetValueAtIndex(args, i), a, sizeof(a));
			expand(a, event, copies[argc], 1024);
			argv[argc] = copies[argc];
			argc++;
		}
		argv[argc] = NULL;
		spawn_wait(argv, NULL);
		for (i = 1; i < argc; i++)
			free(copies[i]);
	} else if (strcmp(type, "SendEmail") == 0) {
		char to[512], subject[512], s2[512];
		const char *argv[] = { "/usr/sbin/sendmail", "-t", NULL };

		CFTypeRef r = get(action, "recipientAddresses");

		/* One address, or a list of them. */
		if (r != NULL && CFGetTypeID(r) == CFArrayGetTypeID()) {
			CFIndex k;

			to[0] = '\0';
			for (k = 0; k < CFArrayGetCount(r); k++) {
				char a[256];

				if (!cstr(CFArrayGetValueAtIndex(r, k), a, sizeof(a)))
					continue;
				if (to[0] != '\0')
					strlcat(to, ", ", sizeof(to));
				strlcat(to, a, sizeof(to));
			}
		} else {
			cstr(r, to, sizeof(to));
		}
		if (to[0] == '\0') {
			note(LOG_ERR, "%s: SendEmail without recipientAddresses",
			    rule);
			return;
		}
		cstr(get(action, "subject"), subject, sizeof(subject));
		expand(subject, event, s2, sizeof(s2));
		cstr(get(action, "message"), buf, sizeof(buf));
		snprintf(text, sizeof(text), "To: %s\nSubject: %s\n\n", to, s2);
		expand(buf, event, text + strlen(text), sizeof(text) -
		    strlen(text));
		strlcat(text, "\n", sizeof(text));
		if (access(argv[0], X_OK) != 0) {
			note(LOG_ERR, "%s: no %s to send mail", rule, argv[0]);
			return;
		}
		spawn_wait(argv, text);
	} else {
		note(LOG_WARNING, "%s: action type %s not supported", rule,
		    type[0] ? type : "(none)");
	}
}

static void
handle_event(CFDictionaryRef event)
{
	char type[128], name[256];
	CFIndex i, j;

	if (!cstr(get(event, "eventType"), type, sizeof(type)))
		return;
	for (i = 0; i < CFArrayGetCount(rules); i++) {
		CFDictionaryRef rule = CFArrayGetValueAtIndex(rules, i);
		CFTypeRef actions;

		if (!takes_event(rule, type) || !rule_matches(rule, event))
			continue;
		if (!cstr(get(rule, "name"), name, sizeof(name)))
			strlcpy(name, "(unnamed rule)", sizeof(name));
		actions = get(rule, "actions");
		for (j = 0; actions != NULL &&
		    CFGetTypeID(actions) == CFArrayGetTypeID() &&
		    j < CFArrayGetCount(actions); j++)
			run_action(CFArrayGetValueAtIndex(actions, j), event, name);
	}
}

/* The events clients left: each a dictionary, handled then removed. */
static void
queued_events(void)
{
	struct dirent **names;
	int n, i;

	n = scandir(events_dir, &names, NULL, alphasort);
	for (i = 0; i < n; i++) {
		char path[PATH_MAX];
		CFPropertyListRef ev;

		if (names[i]->d_name[0] != '.') {
			snprintf(path, sizeof(path), "%s/%s", events_dir,
			    names[i]->d_name);
			ev = read_plist(path);
			if (ev != NULL && CFGetTypeID(ev) == CFDictionaryGetTypeID())
				handle_event(ev);
			else
				note(LOG_WARNING, "%s: not an event", path);
			if (ev != NULL)
				CFRelease(ev);
			unlink(path);
		}
		free(names[i]);
	}
	if (n >= 0)
		free(names);
}

static bool
has_clients(void)
{
	DIR *d = opendir(clients_dir);
	struct dirent *de;
	bool any = false;

	while (d != NULL && !any && (de = readdir(d)) != NULL)
		any = de->d_name[0] != '.';
	if (d != NULL)
		closedir(d);
	return (any);
}

static void
on_hup(int sig)
{
	(void)sig;
	reload = 1;
}

static void
on_term(int sig)
{
	(void)sig;
	stop = 1;
}

static void
usage(void)
{
	fprintf(stderr, "usage: emond [-d] [-r rulesdir] [-e eventsdir] "
	    "[-c clientsdir]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	CFMutableDictionaryRef startup;
	int ch;

	while ((ch = getopt(argc, argv, "dr:e:c:")) != -1) {
		switch (ch) {
		case 'd':
			foreground = true;
			break;
		case 'r':
			rules_dir = optarg;
			break;
		case 'e':
			events_dir = optarg;
			break;
		case 'c':
			clients_dir = optarg;
			break;
		default:
			usage();
		}
	}
	openlog("emond", LOG_PID, LOG_DAEMON);
	signal(SIGHUP, on_hup);
	signal(SIGTERM, on_term);
	signal(SIGINT, on_term);
	signal(SIGCHLD, SIG_DFL);
	mkdir(events_dir, 0755);
	load_rules();

	startup = CFDictionaryCreateMutable(NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(startup, CFSTR("eventType"), CFSTR("startup"));
	handle_event(startup);
	CFRelease(startup);

	/* Stay while there are clients; launchd starts us again for them. */
	while (!stop) {
		if (reload) {
			reload = 0;
			load_rules();
		}
		queued_events();
		if (!has_clients())
			break;
		sleep(POLL_SECONDS);
	}
	queued_events();
	note(LOG_INFO, "exiting");
	return (0);
}
