/*
 * SPDX-License-Identifier: BSD-2-Clause
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
 * loginwindow, minimal (WindowServer plan, section 10.3; WS4b): it owns the
 * session. At start it logs root in on the console and starts the session's
 * applications; it serves the requests of LoginWindowRequest.h: Log Out
 * asks every application to quit (LaunchServices' session), waits for them
 * and starts the session again; Restart and Shut Down do the same, then
 * call reboot(2). An application that does not quit cancels the request.
 * A session application that exits is started again, as launchd keeps the
 * reference platform's Dock alive (so "killall Dock" applies its
 * preferences); one that exits within RESPAWN_THROTTLE seconds of its start
 * waits that long first. Not while the session ends.
 *
 * The session's user (Atlas.md, M5): at the first start, autoLoginUser in
 * /Library/Preferences/com.apple.loginwindow, as on the reference
 * platform; otherwise, and after each Log Out, the login window
 * (LoginWindow.app, inside this bundle) asks, and says on its standard
 * output whom it verified ("user NAME"), or that it was asked to restart or
 * shut down. The session's applications run as that user, with its HOME,
 * USER, LOGNAME and SHELL, in its home; the console is the user's while the
 * session lasts, so the user's apps may ask for Log Out, Restart and Shut
 * Down. Its programs run in the user's launchd (its per-user context),
 * where its LaunchAgents are loaded at login and unloaded at Log Out; its
 * login items are opened by a copy of this program running as the user
 * (-loginitems).
 *
 * SHORTCUT(Phase 5): no confirmation of its own (the app asking shows
 * one), one session at a time, and the user's launchd's Background session
 * (LaunchAgents limited to Aqua are not loaded).
 *
 *	loginwindow [-n]	-n: try it out, without restarting or
 *				shutting down, and with the same user's session
 *				again after Log Out (for the tests)
 */

#include <bsm/libbsm.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <limits.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <pthread.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <mach-o/dyld.h>
#include <LaunchServices/LaunchServices.h>
#include <LaunchServices/LSSession.h>
#include <SkyLight/SkyLight.h>
#include "LoginWindowRequest.h"

/* liblaunch: the per-user launchd as the bootstrap (vproc_priv.h). */
void *_vprocmgr_move_subset_to_user(uid_t target_user, const char *session_type, uint64_t flags);

extern char **environ;

/* The applications of a session, one path per line (an app bundle or an
 * executable); those not installed are left out. */
#define SESSION_APPS	"/System/Library/CoreServices/loginwindow.app/Contents/Resources/SessionApplications"
/* The login window, and the preferences that name the user logged in at
 * the first start. */
#define LOGIN_WINDOW	"/System/Library/CoreServices/loginwindow.app/Contents/Resources/LoginWindow.app/Contents/MacOS/LoginWindow"
#define LOGIN_PREFS	"/Library/Preferences/com.apple.loginwindow.plist"
/* How long the applications have to quit. */
#define QUIT_TIMEOUT	30

static int dryRun;

/* The session applications started, to start again when they exit. */
#define MAX_SESSION_APPS	16
#define RESPAWN_THROTTLE	10
static struct {
	char exe[1200];
	pid_t pid;
	time_t started;
} sessionApps[MAX_SESSION_APPS];
static int sessionAppCount;
/* The session's user. */
static struct {
	char	name[64];
	uid_t	uid;
	gid_t	gid;
	char	home[PATH_MAX];
	char	shell[PATH_MAX];
} sessionUser;
static int firstSession = 1;
static int sessionEnding;
static pthread_mutex_t sessionLock = PTHREAD_MUTEX_INITIALIZER;

static const char *
requestName(uint32_t request)
{
	switch (request) {
	case kLoginWindowLogOut:	return "log out";
	case kLoginWindowRestart:	return "restart";
	case kLoginWindowShutDown:	return "shut down";
	}
	return "?";
}

/* An app bundle's executable: Contents/MacOS/ and its name without .app. */
static void
executableFor(const char *path, char *out, size_t size)
{
	size_t len = strlen(path);
	const char *base = strrchr(path, '/');

	base = base ? base + 1 : path;
	if (len > 4 && strcmp(path + len - 4, ".app") == 0)
		snprintf(out, size, "%s/Contents/MacOS/%.*s", path, (int)strlen(base) - 4, base);
	else
		snprintf(out, size, "%s", path);
}

static void startApp(int i);

/* Command-Option-Escape (the window server's kSLSHotKeyForceQuit) opens
 * the Force Quit window, as loginwindow does on the reference platform.
 * The window server may start after loginwindow, or again: the claim is
 * made again whenever the connection is lost. */
static void *
forceQuitHotKey(void *unused)
{
	(void)unused;
	for (;;) {
		if (SLSMainConnectionID() == 0 || SLSClaimSymbolicHotKey(kSLSHotKeyForceQuit, 1) != kSLSErrorSuccess) {
			sleep(2);
			continue;
		}
		fprintf(stderr, "loginwindow: Command-Option-Escape claimed\n");
		for (;;) {
			SLSEvent events[kSLSMaxEvents];
			uint32_t n = 0;
			if (SLSReceiveEvents(events, kSLSMaxEvents, &n, 1) != kSLSErrorSuccess)
				break;
			for (uint32_t i = 0; i < n; i++) {
				if (events[i].type != kSLSEventSymbolicHotKey || events[i].buttonNumber != kSLSHotKeyForceQuit)
					continue;
				CFURLRef url = CFURLCreateWithFileSystemPath(NULL,
				    CFSTR("/System/Library/CoreServices/ForceQuit.app"), kCFURLPOSIXPathStyle, true);
				OSStatus err = LSOpenCFURLRef(url, NULL);
				CFRelease(url);
				fprintf(stderr, "loginwindow: Force Quit%s\n", err == 0 ? "" : " (cannot open it)");
			}
		}
		sleep(1);
	}
	return NULL;
}

/* The user's login items (Open at Login in the Dock, LSSharedFileList's
 * session list), opened once the session's applications are up: hidden
 * when the item asks. */
static void *
openLoginItems(void *unused)
{
	(void)unused;
	sleep(2);
	LSSharedFileListRef list = LSSharedFileListCreate(NULL, kLSSharedFileListSessionLoginItems, NULL);
	if (list == NULL)
		return NULL;
	CFArrayRef items = LSSharedFileListCopySnapshot(list, NULL);
	for (CFIndex i = 0; items != NULL && i < CFArrayGetCount(items); i++) {
		LSSharedFileListItemRef item = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(items, i);
		CFURLRef url = LSSharedFileListItemCopyResolvedURL(item, 0, NULL);
		if (url == NULL)
			continue;
		CFTypeRef hidden = LSSharedFileListItemCopyProperty(item, kLSSharedFileListLoginItemHidden);
		LSLaunchURLSpec spec = { 0 };
		spec.appURL = url;
		spec.launchFlags = kLSLaunchDefaults |
		    (hidden != NULL && CFEqual(hidden, kCFBooleanTrue) ? kLSLaunchAndHide : 0);
		char path[1024] = "";
		CFURLGetFileSystemRepresentation(url, true, (UInt8 *)path, sizeof(path));
		OSStatus err = LSOpenFromURLSpec(&spec, NULL);
		fprintf(stderr, "loginwindow: login item %s%s\n", path,
		    err == 0 ? "" : " (cannot open it)");
		if (hidden != NULL)
			CFRelease(hidden);
		CFRelease(url);
	}
	if (items != NULL)
		CFRelease(items);
	CFRelease(list);
	return NULL;
}

static char selfPath[PATH_MAX];

/* Starts exe as the session's user, in its home: its pid, or -1. A
 * user's program goes through a fresh copy of this one (-exec), which
 * takes the user's launchd as its bootstrap, then the user's IDs. */
static pid_t
spawnAsUser(const char *exe, char *const argv[])
{
	pid_t pid;
	int err;

	if (sessionUser.uid == 0) {
		err = posix_spawn(&pid, exe, NULL, NULL, argv, environ);
	} else {
		char *args[64];
		int n = 0;

		args[n++] = selfPath;
		args[n++] = "-exec";
		args[n++] = (char *)exe;
		for (int i = 1; argv[i] != NULL && n < 63; i++)
			args[n++] = argv[i];
		args[n] = NULL;
		err = posix_spawn(&pid, selfPath, NULL, NULL, args, environ);
	}
	if (err != 0) {
		errno = err;
		return -1;
	}
	return pid;
}

/* -exec EXE ARGS...: as root, in the environment of the session (USER,
 * HOME): the user's launchd (its per-user context, created when first
 * asked) as the bootstrap, so the user's programs and LaunchAgents share
 * it; then the user's groups and IDs, its home, and EXE. */
static int
execAsUser(char *argv[])
{
	const char *user = getenv("USER");
	struct passwd *pw = user != NULL ? getpwnam(user) : NULL;

	if (pw == NULL || pw->pw_uid == 0)
		return 126;
	if (_vprocmgr_move_subset_to_user(pw->pw_uid, "Background", 0) != NULL)
		fprintf(stderr, "loginwindow: no launchd of %s's: %s keeps the system's\n",
		    pw->pw_name, argv[0]);
	if (initgroups(pw->pw_name, (int)pw->pw_gid) != 0 || setgid(pw->pw_gid) != 0 ||
	    setuid(pw->pw_uid) != 0)
		return 126;
	if (chdir(pw->pw_dir) != 0)
		(void)chdir("/");
	execv(argv[0], argv);
	fprintf(stderr, "loginwindow: cannot start %s: %s\n", argv[0], strerror(errno));
	return 127;
}

/* The user's LaunchAgents loaded (or unloaded) in its launchd: the
 * system's, the computer's and its own. SHORTCUT(Phase 5): the session is
 * the user's launchd's Background one; agents limited to Aqua are left. */
static void
userAgents(const char *verb)
{
	char own[PATH_MAX + 32];
	pid_t pid;
	int status;

	if (sessionUser.uid == 0)
		return;
	snprintf(own, sizeof(own), "%s/Library/LaunchAgents", sessionUser.home);
	char *argv[] = { "/bin/launchctl", (char *)verb, "/System/Library/LaunchAgents",
	    "/Library/LaunchAgents", own, NULL };
	if ((pid = spawnAsUser(argv[0], argv)) > 0)
		(void)waitpid(pid, &status, 0);
	fprintf(stderr, "loginwindow: %s %s's LaunchAgents\n", verb, sessionUser.name);
}

static int
setSessionUser(const char *name)
{
	struct passwd *pw = name != NULL && name[0] != '\0' ? getpwnam(name) : NULL;

	if (pw == NULL)
		return 0;
	snprintf(sessionUser.name, sizeof(sessionUser.name), "%s", pw->pw_name);
	sessionUser.uid = pw->pw_uid;
	sessionUser.gid = pw->pw_gid;
	snprintf(sessionUser.home, sizeof(sessionUser.home), "%s", pw->pw_dir);
	snprintf(sessionUser.shell, sizeof(sessionUser.shell), "%s",
	    pw->pw_shell != NULL && pw->pw_shell[0] != '\0' ? pw->pw_shell : "/bin/sh");
	return 1;
}

/* autoLoginUser, from the login window's preferences. */
static int
autoLoginUser(char *out, size_t size)
{
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)LOGIN_PREFS,
	    (CFIndex)strlen(LOGIN_PREFS), false);
	CFReadStreamRef stream = url != NULL ? CFReadStreamCreateWithFile(NULL, url) : NULL;
	CFPropertyListRef plist = NULL;
	int found = 0;

	if (stream != NULL && CFReadStreamOpen(stream)) {
		plist = CFPropertyListCreateWithStream(NULL, stream, 0, kCFPropertyListImmutable, NULL, NULL);
		CFReadStreamClose(stream);
	}
	if (plist != NULL && CFGetTypeID(plist) == CFDictionaryGetTypeID()) {
		CFStringRef user = CFDictionaryGetValue(plist, CFSTR("autoLoginUser"));
		if (user != NULL && CFGetTypeID(user) == CFStringGetTypeID())
			found = CFStringGetCString(user, out, (CFIndex)size, kCFStringEncodingUTF8);
	}
	if (plist != NULL)
		CFRelease(plist);
	if (stream != NULL)
		CFRelease(stream);
	if (url != NULL)
		CFRelease(url);
	return found;
}

static void powerOff(uint32_t request);

/* The user of the next session: autoLoginUser the first time, else the
 * login window's answer (restarting or shutting down when it says so). */
static void
chooseUser(void)
{
	char name[256];

	if (firstSession) {
		firstSession = 0;
		if (autoLoginUser(name, sizeof(name)) && setSessionUser(name)) {
			fprintf(stderr, "loginwindow: automatic login of %s\n", name);
			return;
		}
	}
	for (;;) {
		int fds[2];
		posix_spawn_file_actions_t actions;
		char *argv[] = { LOGIN_WINDOW, NULL };
		char line[300] = "";
		pid_t pid;
		FILE *in;

		if (pipe(fds) != 0) {
			sleep(1);
			continue;
		}
		posix_spawn_file_actions_init(&actions);
		posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
		posix_spawn_file_actions_addclose(&actions, fds[0]);
		setenv("HOME", "/var/root", 1);
		setenv("USER", "root", 1);
		setenv("LOGNAME", "root", 1);
		int err = posix_spawn(&pid, LOGIN_WINDOW, &actions, NULL, argv, environ);
		posix_spawn_file_actions_destroy(&actions);
		close(fds[1]);
		if (err != 0) {
			fprintf(stderr, "loginwindow: cannot start the login window: %s; root logs in\n",
			    strerror(err));
			close(fds[0]);
			setSessionUser("root");
			return;
		}
		fprintf(stderr, "loginwindow: login window (pid %d)\n", pid);
		in = fdopen(fds[0], "r");
		if (in == NULL || fgets(line, sizeof(line), in) == NULL)
			line[0] = '\0';
		if (in != NULL)
			fclose(in);
		else
			close(fds[0]);
		line[strcspn(line, "\r\n")] = '\0';
		if (strncmp(line, "user ", 5) == 0 && setSessionUser(line + 5))
			return;
		if (strcmp(line, "restart") == 0)
			powerOff(kLoginWindowRestart);
		else if (strcmp(line, "shutdown") == 0)
			powerOff(kLoginWindowShutDown);
		else {
			fprintf(stderr, "loginwindow: the login window ended (\"%s\"): again\n", line);
			sleep(1);
		}
	}
}

/* Starts the session's applications; called with sessionLock held. */
static void
startSession(void)
{
	FILE *list = fopen(SESSION_APPS, "r");
	char line[1024], exe[1200];

	fprintf(stderr, "loginwindow: session of %s on the console\n", sessionUser.name);
	setenv("HOME", sessionUser.home, 1);
	setenv("USER", sessionUser.name, 1);
	setenv("LOGNAME", sessionUser.name, 1);
	setenv("SHELL", sessionUser.shell, 1);
	if (chown("/dev/console", sessionUser.uid, sessionUser.gid) != 0)
		fprintf(stderr, "loginwindow: console: %s\n", strerror(errno));
	sessionAppCount = 0;
	sessionEnding = 0;
	if (list == NULL)
		return;
	while (fgets(line, sizeof(line), list) != NULL) {
		line[strcspn(line, "\r\n")] = '\0';
		if (line[0] == '\0' || line[0] == '#')
			continue;
		executableFor(line, exe, sizeof(exe));
		if (access(exe, X_OK) != 0 || sessionAppCount == MAX_SESSION_APPS)
			continue;
		snprintf(sessionApps[sessionAppCount].exe, sizeof(sessionApps[0].exe), "%s", exe);
		startApp(sessionAppCount++);
	}
	fclose(list);
	if (sessionUser.uid != 0) {
		userAgents("load");
		/* The user's own list, opened as the user. */
		char *argv[] = { selfPath, "-loginitems", NULL };
		spawnAsUser(selfPath, argv);
		return;
	}
	pthread_t opener;
	if (pthread_create(&opener, NULL, openLoginItems, NULL) == 0)
		pthread_detach(opener);
}

/* Starts a session application; called with sessionLock held. */
static void
startApp(int i)
{
	char *argv[] = { sessionApps[i].exe, NULL };
	pid_t pid = spawnAsUser(sessionApps[i].exe, argv);
	int err = pid > 0 ? 0 : errno;

	sessionApps[i].pid = err == 0 ? pid : 0;
	sessionApps[i].started = time(NULL);
	if (err != 0)
		fprintf(stderr, "loginwindow: cannot start %s: %s\n", sessionApps[i].exe, strerror(err));
	else
		fprintf(stderr, "loginwindow: started %s (pid %d)\n", sessionApps[i].exe, pid);
}

/* Reaps the children; a session application that exits is started again. */
static void *
respawn(void *unused)
{
	(void)unused;
	for (;;) {
		int status;
		pid_t pid = waitpid(-1, &status, 0);

		if (pid < 0) {
			sleep(1);
			continue;
		}
		pthread_mutex_lock(&sessionLock);
		for (int i = 0; i < sessionAppCount; i++) {
			if (sessionApps[i].pid != pid)
				continue;
			sessionApps[i].pid = 0;
			if (sessionEnding)
				break;
			fprintf(stderr, "loginwindow: %s (pid %d) exited (%d): starting it again\n",
			    sessionApps[i].exe, pid, WIFEXITED(status) ? WEXITSTATUS(status) :
			    128 + WTERMSIG(status));
			if (time(NULL) - sessionApps[i].started < RESPAWN_THROTTLE) {
				pthread_mutex_unlock(&sessionLock);
				sleep(RESPAWN_THROTTLE);
				pthread_mutex_lock(&sessionLock);
				if (sessionEnding || sessionApps[i].pid != 0)
					break;
			}
			startApp(i);
			break;
		}
		pthread_mutex_unlock(&sessionLock);
	}
	return NULL;
}

/* The applications still running, but this process. */
static uint32_t
runningApps(LSSessionApp **apps)
{
	uint32_t count = 0;

	if (LSSessionCopyApps(apps, &count) != kLSSessionSuccess) {
		*apps = NULL;
		return 0;
	}
	for (uint32_t i = 0; i < count; i++)
		if ((*apps)[i].pid == getpid()) {
			(*apps)[i] = (*apps)[--count];
			break;
		}
	return count;
}

/* Asks every application to quit; YES once all have gone. */
static int
quitApplications(const char *what)
{
	LSSessionApp *apps;
	uint32_t count = runningApps(&apps);

	for (uint32_t i = 0; i < count; i++) {
		fprintf(stderr, "loginwindow: asking pid %d (%s) to quit\n", apps[i].pid,
		    apps[i].bundleIdentifier[0] ? apps[i].bundleIdentifier : apps[i].name);
		LSSessionQuit(apps[i].pid);
	}
	free(apps);
	for (int tick = 0; tick < QUIT_TIMEOUT * 4; tick++) {
		count = runningApps(&apps);
		if (count == 0)
			return 1;
		if (tick == QUIT_TIMEOUT * 4 - 1)
			for (uint32_t i = 0; i < count; i++)
				fprintf(stderr, "loginwindow: pid %d (%s) did not quit: %s cancelled\n",
				    apps[i].pid, apps[i].bundleIdentifier[0] ?
				    apps[i].bundleIdentifier : apps[i].name, what);
		free(apps);
		usleep(250000);
	}
	return 0;
}

static void endSession(void);

static void
serve(uint32_t request)
{
	const char *what = requestName(request);

	fprintf(stderr, "loginwindow: %s requested\n", what);
	pthread_mutex_lock(&sessionLock);
	sessionEnding = 1;
	pthread_mutex_unlock(&sessionLock);
	if (!quitApplications(what)) {
		/* Cancelled: the session goes on, and its apps are kept alive. */
		pthread_mutex_lock(&sessionLock);
		sessionEnding = 0;
		for (int i = 0; i < sessionAppCount; i++)
			if (sessionApps[i].pid == 0)
				startApp(i);
		pthread_mutex_unlock(&sessionLock);
		return;
	}
	switch (request) {
	case kLoginWindowLogOut:
		fprintf(stderr, "loginwindow: session ended (%s)\n", sessionUser.name);
		endSession();
		/* Trying out (-n): the same user's session again. */
		if (!dryRun)
			chooseUser();
		pthread_mutex_lock(&sessionLock);
		startSession();
		pthread_mutex_unlock(&sessionLock);
		break;
	case kLoginWindowRestart:
	case kLoginWindowShutDown:
		endSession();
		powerOff(request);
		break;
	}
}

/* The console back to root once the session's apps are gone. */
static void
endSession(void)
{
	userAgents("unload");
	if (chown("/dev/console", 0, 0) != 0)
		fprintf(stderr, "loginwindow: console: %s\n", strerror(errno));
}

/* Restarts or shuts down; trying out (-n), a new session instead. */
static void
powerOff(uint32_t request)
{
	fprintf(stderr, "loginwindow: session ended, %s%s\n",
	    request == kLoginWindowRestart ? "restarting" : "shutting down",
	    dryRun ? " (not: -n)" : "");
	if (dryRun) {
		pthread_mutex_lock(&sessionLock);
		startSession();
		pthread_mutex_unlock(&sessionLock);
		return;
	}
	sync();
	reboot(request == kLoginWindowRestart ? RB_AUTOBOOT : RB_HALT);
	fprintf(stderr, "loginwindow: reboot: %s\n", strerror(errno));
}

/* The -textconsole boot argument: no window server, so no session. */
static int
textConsole(void)
{
	char args[1024] = "";
	size_t len = sizeof(args);

	if (sysctlbyname("kern.bootargs", args, &len, NULL, 0) != 0)
		return 0;
	for (char *word = strtok(args, " "); word != NULL; word = strtok(NULL, " "))
		if (strcmp(word, "-textconsole") == 0)
			return 1;
	return 0;
}

/* Root and the console's owner may ask. */
static int
allowed(const audit_token_t *token)
{
	uid_t uid = audit_token_to_euid(*token);
	struct stat st;

	return uid == 0 || (stat("/dev/console", &st) == 0 && st.st_uid == uid);
}

int
main(int argc, char *argv[])
{
	mach_port_t service = MACH_PORT_NULL;
	kern_return_t kr;

	setvbuf(stderr, NULL, _IOLBF, 0);
	if (argc > 2 && strcmp(argv[1], "-exec") == 0)
		return execAsUser(argv + 2);
	if (argc > 1 && strcmp(argv[1], "-loginitems") == 0) {
		/* As the session's user: its login items. */
		openLoginItems(NULL);
		return 0;
	}
	if (argc > 1 && strcmp(argv[1], "-n") == 0)
		dryRun = 1;
	uint32_t size = sizeof(selfPath);
	if (_NSGetExecutablePath(selfPath, &size) != 0)
		snprintf(selfPath, sizeof(selfPath), "%s", argv[0]);
	if (textConsole()) {
		fprintf(stderr, "loginwindow: -textconsole: no graphical session\n");
		return 0;
	}
	kr = bootstrap_check_in(bootstrap_port, kLoginWindowServiceName, &service);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "loginwindow: cannot check in %s: 0x%x\n", kLoginWindowServiceName, kr);
		return 1;
	}
	fprintf(stderr, "loginwindow: serving %s%s\n", kLoginWindowServiceName,
	    dryRun ? " (trying out: no restart or shut down)" : "");
	chooseUser();
	pthread_mutex_lock(&sessionLock);
	startSession();
	pthread_mutex_unlock(&sessionLock);
	pthread_t reaper, hotKey;
	pthread_create(&reaper, NULL, respawn, NULL);
	pthread_create(&hotKey, NULL, forceQuitHotKey, NULL);

	for (;;) {
		union {
			LoginWindowRequest req;
			uint8_t bytes[sizeof(LoginWindowRequest) + MAX_TRAILER_SIZE];
		} msg;

		kr = mach_msg(&msg.req.header, MACH_RCV_MSG |
		    MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
		    MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT), 0, sizeof(msg), service,
		    MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
		if (kr != MACH_MSG_SUCCESS) {
			fprintf(stderr, "loginwindow: receive: 0x%x\n", kr);
			continue;
		}
		mach_msg_audit_trailer_t *trailer = (mach_msg_audit_trailer_t *)
		    ((uint8_t *)&msg + round_msg(msg.req.header.msgh_size));
		if (msg.req.header.msgh_id != kLoginWindowRequestID ||
		    msg.req.header.msgh_size < sizeof(LoginWindowRequest)) {
			mach_msg_destroy(&msg.req.header);
			continue;
		}
		if (!allowed(&trailer->msgh_audit)) {
			fprintf(stderr, "loginwindow: %s refused to uid %d\n",
			    requestName(msg.req.request), audit_token_to_euid(trailer->msgh_audit));
			mach_msg_destroy(&msg.req.header);
			continue;
		}
		uint32_t request = msg.req.request;
		mach_msg_destroy(&msg.req.header);
		serve(request);
	}
}
