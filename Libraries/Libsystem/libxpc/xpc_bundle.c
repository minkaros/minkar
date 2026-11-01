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
 * XPC services inside an application's bundle (Contents/XPCServices, one
 * .xpc bundle each).
 *
 * xpc_connection_create(name) in an app looks for a service of that name
 * (the bundle's directory "<name>.xpc", or the one whose CFBundleIdentifier
 * is name) and starts it on demand as the app's child, with
 * XPC_SERVICE_NAME set. The child's xpc_main() makes an anonymous listener
 * and hands its port back through a port the app registered for it
 * (mach_ports_register: registered ports are inherited by a new task), so
 * neither launchd nor a privilege is needed. One instance runs per app;
 * when it exits the app's connections are interrupted and the next message
 * starts it again. The service exits when the app does.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dispatch/dispatch.h>
#include <xpc/xpc.h>
#include "xpc_internal.h"

extern char **environ;

/* The library's flags hide these declarations (as in xpc_connection.c). */
char *strdup(const char *);
char *strndup(const char *, size_t);
char *strstr(const char *, const char *);
char *strrchr(const char *, int);
int asprintf(char **, const char *, ...);

#define HANDOFF_ENV	"MINKAR_XPC_HANDOFF"
#define HANDOFF_ID	0x58504348	/* 'XPCH' */
#define HANDOFF_TIMEOUT	10000		/* ms the app waits for the service */

struct handoff_message {
	mach_msg_header_t		header;
	mach_msg_body_t			body;
	mach_msg_port_descriptor_t	port;
};

struct handoff_receive {
	struct handoff_message		msg;
	mach_msg_trailer_t		trailer;
};

struct bundled_service {
	struct bundled_service *next;
	char *name;
	pid_t pid;
	mach_port_t port;		/* a send right to its listener */
};

static pthread_mutex_t bundled_lock = PTHREAD_MUTEX_INITIALIZER;
static struct bundled_service *bundled_list;

/* The value of key in an XML property list (a <string>), or NULL. */
static char *
plist_string(const char *path, const char *key)
{
	char pattern[256], *buf, *p, *end, *result = NULL;
	struct stat st;
	int fd;

	if ((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0)
		return (NULL);
	if (fstat(fd, &st) != 0 || st.st_size <= 0 || st.st_size > 1024 * 1024 ||
	    (buf = malloc((size_t)st.st_size + 1)) == NULL) {
		close(fd);
		return (NULL);
	}
	if (read(fd, buf, (size_t)st.st_size) != st.st_size) {
		free(buf);
		close(fd);
		return (NULL);
	}
	close(fd);
	buf[st.st_size] = '\0';
	snprintf(pattern, sizeof(pattern), "<key>%s</key>", key);
	if ((p = strstr(buf, pattern)) != NULL) {
		p += strlen(pattern);
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
			p++;
		if (strncmp(p, "<string>", 8) == 0 && (end = strstr(p + 8, "</string>")) != NULL)
			result = strndup(p + 8, (size_t)(end - p - 8));
	}
	free(buf);
	return (result);
}

/* The app's Contents directory: its executable is Contents/MacOS/<name>. */
static bool
app_contents(char *out, size_t size)
{
	char exe[PATH_MAX], real[PATH_MAX], *slash;
	uint32_t len = sizeof(exe);

	if (_NSGetExecutablePath(exe, &len) != 0 || realpath(exe, real) == NULL)
		return (false);
	if ((slash = strrchr(real, '/')) == NULL)
		return (false);
	*slash = '\0';				/* .../Contents/MacOS */
	if ((slash = strrchr(real, '/')) == NULL || strcmp(slash, "/MacOS") != 0)
		return (false);
	*slash = '\0';				/* .../Contents */
	return (strlcpy(out, real, size) < size);
}

/* The executable of the bundle at dir (an .xpc). */
static bool
service_executable(const char *dir, char *out, size_t size)
{
	char path[PATH_MAX], *exe, *base, *dot;
	struct stat st;

	snprintf(path, sizeof(path), "%s/Contents/Info.plist", dir);
	exe = plist_string(path, "CFBundleExecutable");
	if (exe == NULL) {
		/* Without one, the bundle's own name. */
		base = strrchr(dir, '/');
		exe = strdup(base != NULL ? base + 1 : dir);
		if (exe == NULL)
			return (false);
		if ((dot = strrchr(exe, '.')) != NULL)
			*dot = '\0';
	}
	snprintf(out, size, "%s/Contents/MacOS/%s", dir, exe);
	free(exe);
	return (stat(out, &st) == 0 && S_ISREG(st.st_mode) && access(out, X_OK) == 0);
}

/* The executable of the app's service called name. */
static bool
find_service(const char *name, char *exe, size_t size)
{
	char contents[PATH_MAX], dir[PATH_MAX], plist[PATH_MAX];
	struct dirent *ent;
	bool found = false;
	DIR *d;

	if (name == NULL || strchr(name, '/') != NULL || !app_contents(contents, sizeof(contents)))
		return (false);
	snprintf(dir, sizeof(dir), "%s/XPCServices/%s.xpc", contents, name);
	if (service_executable(dir, exe, size))
		return (true);
	snprintf(dir, sizeof(dir), "%s/XPCServices", contents);
	if ((d = opendir(dir)) == NULL)
		return (false);
	while (!found && (ent = readdir(d)) != NULL) {
		size_t n = strlen(ent->d_name);
		char *ident;

		if (n < 5 || strcmp(ent->d_name + n - 4, ".xpc") != 0)
			continue;
		snprintf(plist, sizeof(plist), "%s/%s/Contents/Info.plist", dir, ent->d_name);
		if ((ident = plist_string(plist, "CFBundleIdentifier")) == NULL)
			continue;
		if (strcmp(ident, name) == 0) {
			char sdir[PATH_MAX];

			snprintf(sdir, sizeof(sdir), "%s/%s", dir, ent->d_name);
			found = service_executable(sdir, exe, size);
		}
		free(ident);
	}
	closedir(d);
	return (found);
}

bool
_xpc_bundle_has_service(const char *name)
{
	char exe[PATH_MAX];

	return (find_service(name, exe, sizeof(exe)));
}

/* The environment with XPC_SERVICE_NAME and the handoff flag set. */
static char **
service_environment(const char *name)
{
	size_t n = 0, i, j = 0;
	char **env;

	while (environ[n] != NULL)
		n++;
	if ((env = calloc(n + 3, sizeof(char *))) == NULL)
		return (NULL);
	for (i = 0; i < n; i++)
		if (strncmp(environ[i], "XPC_SERVICE_NAME=", 17) != 0 &&
		    strncmp(environ[i], HANDOFF_ENV "=", sizeof(HANDOFF_ENV)) != 0)
			env[j++] = environ[i];
	asprintf(&env[j++], "XPC_SERVICE_NAME=%s", name);
	env[j++] = strdup(HANDOFF_ENV "=1");
	env[j] = NULL;
	return (env);
}

static void
free_environment(char **env)
{
	size_t n = 0;

	while (env[n] != NULL)
		n++;
	/* The last two are ours. */
	if (n >= 2) {
		free(env[n - 1]);
		free(env[n - 2]);
	}
	free(env);
}

/* A service ended: reap it and forget its port. */
static void
watch_service(pid_t pid)
{
	dispatch_source_t ds = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC,
	    (uintptr_t)pid, DISPATCH_PROC_EXIT,
	    dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));

	if (ds == NULL)
		return;
	dispatch_source_set_event_handler(ds, ^{
		struct bundled_service **pp, *s;

		(void)waitpid(pid, NULL, WNOHANG);
		pthread_mutex_lock(&bundled_lock);
		for (pp = &bundled_list; (s = *pp) != NULL; pp = &s->next)
			if (s->pid == pid) {
				*pp = s->next;
				mach_port_deallocate(mach_task_self(), s->port);
				free(s->name);
				free(s);
				break;
			}
		pthread_mutex_unlock(&bundled_lock);
		dispatch_source_cancel(ds);
		dispatch_release(ds);
	});
	dispatch_resume(ds);
}

/* Starts the service and waits for its listener's port. */
static mach_port_t
start_service(const char *name, const char *exe, pid_t *pidp)
{
	mach_port_t handoff = MACH_PORT_NULL, result = MACH_PORT_NULL;
	mach_port_array_t old = NULL;
	mach_msg_type_number_t oldcount = 0;
	struct handoff_receive msg;
	char *argv[] = { NULL, NULL };
	char **env;
	pid_t pid;
	int error;

	if ((env = service_environment(name)) == NULL)
		return (MACH_PORT_NULL);
	if ((argv[0] = strdup(exe)) == NULL) {
		free_environment(env);
		return (MACH_PORT_NULL);
	}
	if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &handoff) != KERN_SUCCESS) {
		free_environment(env);
		free(argv[0]);
		return (MACH_PORT_NULL);
	}
	mach_port_insert_right(mach_task_self(), handoff, handoff, MACH_MSG_TYPE_MAKE_SEND);

	/* The child inherits the registered ports; ours come back after. */
	(void)mach_ports_lookup(mach_task_self(), &old, &oldcount);
	mach_ports_register(mach_task_self(), &handoff, 1);
	error = posix_spawn(&pid, exe, NULL, NULL, argv, env);
	mach_ports_register(mach_task_self(), old, oldcount);
	for (mach_msg_type_number_t i = 0; i < oldcount; i++)
		if (MACH_PORT_VALID(old[i]))
			mach_port_deallocate(mach_task_self(), old[i]);
	if (old != NULL)
		vm_deallocate(mach_task_self(), (vm_address_t)old, oldcount * sizeof(old[0]));
	free_environment(env);
	free(argv[0]);
	mach_port_deallocate(mach_task_self(), handoff);	/* our send right */

	if (error != 0) {
		debugf("cannot start %s: %s", exe, strerror(error));
		mach_port_mod_refs(mach_task_self(), handoff, MACH_PORT_RIGHT_RECEIVE, -1);
		return (MACH_PORT_NULL);
	}
	watch_service(pid);

	memset(&msg, 0, sizeof(msg));
	if (mach_msg(&msg.msg.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg), handoff,
	    HANDOFF_TIMEOUT, MACH_PORT_NULL) == MACH_MSG_SUCCESS) {
		if (msg.msg.header.msgh_id == HANDOFF_ID &&
		    (msg.msg.header.msgh_bits & MACH_MSGH_BITS_COMPLEX) &&
		    msg.msg.body.msgh_descriptor_count == 1)
			result = msg.msg.port.name;
		else
			mach_msg_destroy(&msg.msg.header);
	} else {
		debugf("%s did not answer", name);
	}
	mach_port_mod_refs(mach_task_self(), handoff, MACH_PORT_RIGHT_RECEIVE, -1);
	if (!MACH_PORT_VALID(result))
		kill(pid, SIGKILL);
	*pidp = pid;
	return (result);
}

/*
 * A send right to the listener of the app's service name, started if it
 * is not running; MACH_PORT_NULL if the app has no such service or it did
 * not start.
 */
mach_port_t
_xpc_bundle_lookup(const char *name)
{
	char exe[PATH_MAX];
	struct bundled_service *s;
	mach_port_t port = MACH_PORT_NULL;
	pid_t pid = 0;

	pthread_mutex_lock(&bundled_lock);
	for (s = bundled_list; s != NULL; s = s->next)
		if (strcmp(s->name, name) == 0)
			break;
	if (s != NULL && kill(s->pid, 0) == 0 &&
	    mach_port_mod_refs(mach_task_self(), s->port, MACH_PORT_RIGHT_SEND, 1) == KERN_SUCCESS) {
		port = s->port;
	} else if (find_service(name, exe, sizeof(exe)) &&
	    MACH_PORT_VALID(port = start_service(name, exe, &pid))) {
		if ((s = calloc(1, sizeof(*s))) != NULL && (s->name = strdup(name)) != NULL) {
			s->pid = pid;
			s->port = port;
			mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, 1);
			s->next = bundled_list;
			bundled_list = s;
		} else {
			free(s);
		}
	}
	pthread_mutex_unlock(&bundled_lock);
	return (port);
}

/*
 * In a service started by its app: send the listener's port to the app,
 * and exit when the app does. false when this process was not started so.
 */
bool
_xpc_bundle_handoff(mach_port_t listener)
{
	mach_port_array_t ports = NULL;
	mach_msg_type_number_t count = 0;
	struct handoff_message msg;
	mach_port_t app;
	pid_t parent;
	kern_return_t kr;

	if (getenv(HANDOFF_ENV) == NULL)
		return (false);
	unsetenv(HANDOFF_ENV);
	if (mach_ports_lookup(mach_task_self(), &ports, &count) != KERN_SUCCESS || count == 0)
		return (false);
	app = ports[0];
	for (mach_msg_type_number_t i = 1; i < count; i++)
		if (MACH_PORT_VALID(ports[i]))
			mach_port_deallocate(mach_task_self(), ports[i]);
	vm_deallocate(mach_task_self(), (vm_address_t)ports, count * sizeof(ports[0]));
	/* Not to be inherited further. */
	mach_ports_register(mach_task_self(), NULL, 0);
	if (!MACH_PORT_VALID(app))
		return (false);

	memset(&msg, 0, sizeof(msg));
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
	msg.header.msgh_size = sizeof(msg);
	msg.header.msgh_remote_port = app;
	msg.header.msgh_id = HANDOFF_ID;
	msg.body.msgh_descriptor_count = 1;
	msg.port.name = listener;
	msg.port.disposition = MACH_MSG_TYPE_MAKE_SEND;
	msg.port.type = MACH_MSG_PORT_DESCRIPTOR;
	kr = mach_msg(&msg.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(msg), 0,
	    MACH_PORT_NULL, HANDOFF_TIMEOUT, MACH_PORT_NULL);
	if (kr != MACH_MSG_SUCCESS) {
		mach_port_deallocate(mach_task_self(), app);
		return (false);
	}

	/* The service belongs to its app. */
	parent = getppid();
	if (parent <= 1)
		exit(EXIT_SUCCESS);
	dispatch_source_t ds = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, (uintptr_t)parent,
	    DISPATCH_PROC_EXIT, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
	if (ds != NULL) {
		dispatch_source_set_event_handler(ds, ^{ exit(EXIT_SUCCESS); });
		dispatch_resume(ds);
	}
	if (getppid() != parent)
		exit(EXIT_SUCCESS);
	return (true);
}
