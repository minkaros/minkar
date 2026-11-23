/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * directoryd: users and groups for libinfo's directory module
 * (ds_module.c), under the service name apps' sandbox profiles allow, so
 * a sandboxed process that may not read /etc still finds its user. It
 * runs as root and answers from the local files through libinfo itself,
 * with its own directory module off (XBS_DISABLE_LIBINFO): users,
 * groups and group lists. As with the files themselves, only a caller
 * running as root, and not sandboxed (it could not read them), gets
 * password hashes; others get "********" for an account with a password,
 * "*" for one without, as the reference's directory. Other routines
 * get an error, and libinfo asks the next module.
 *
 * libinfo keeps what this service answers in each process's cache until
 * told it is out of date: when the account files change (written in
 * place or replaced), directoryd says so (pwd_mkdb says so too, before
 * the tools that change them return).
 *
 * It is also OpenDirectory's service (odapi.c), as the reference's
 * opendirectoryd answers both: the framework's sessions, nodes, queries
 * and passwords.
 */

#include <dispatch/dispatch.h>
#include <errno.h>
#include <fcntl.h>
#include <notify.h>
#include <sys/stat.h>
#include <grp.h>
#include <pwd.h>
#include <sandbox.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xpc/xpc.h>
#include <xpc/launchd.h>
#include <bsm/audit.h>

#include "odapi.h"

#define SERVICE		"com.apple.system.opendirectoryd.libinfo"

/* libinfo's own cache, off in this process (as notifyd turns it off): the
   files' times are whole seconds, so a change in the same second as the
   last lookup would go unseen, and the answers are cached by the asking
   processes anyway. */
extern uint32_t gL1CacheEnabled;
#define OD_RPC_NAME	"rpc_name"
#define OD_RPC_RESULT	"result"
#define OD_RPC_ERROR	"error"

/* Whether the request's sender runs as root (euid 0), without a sandbox. */
static int caller_is_root;

/* The password field: the hash for root; for others, as the reference's
   directory gives it, "********" for an account with a password (authd
   takes "*" for a locked account) and "*" for one without (a disabled
   hash begins with "*"). */
static const char *secret(const char *hash) {
	if (caller_is_root && hash != NULL)
		return hash;
	if (hash != NULL && hash[0] != '\0' && hash[0] != '*')
		return "********";
	return "*";
}

static void set_user(xpc_object_t d, const struct passwd *pw) {
	xpc_dictionary_set_string(d, "pw_name", pw->pw_name);
	xpc_dictionary_set_string(d, "pw_passwd", secret(pw->pw_passwd));
	xpc_dictionary_set_int64(d, "pw_uid", pw->pw_uid);
	xpc_dictionary_set_int64(d, "pw_gid", pw->pw_gid);
	xpc_dictionary_set_int64(d, "pw_change", (int64_t)pw->pw_change);
	xpc_dictionary_set_int64(d, "pw_expire", (int64_t)pw->pw_expire);
	xpc_dictionary_set_string(d, "pw_class", pw->pw_class != NULL ? pw->pw_class : "");
	xpc_dictionary_set_string(d, "pw_gecos", pw->pw_gecos != NULL ? pw->pw_gecos : "");
	xpc_dictionary_set_string(d, "pw_dir", pw->pw_dir != NULL ? pw->pw_dir : "/var/empty");
	xpc_dictionary_set_string(d, "pw_shell", pw->pw_shell != NULL ? pw->pw_shell : "/usr/bin/false");
}

static void set_group(xpc_object_t d, const struct group *gr) {
	xpc_dictionary_set_string(d, "gr_name", gr->gr_name);
	xpc_dictionary_set_string(d, "gr_passwd", secret(gr->gr_passwd));
	xpc_dictionary_set_int64(d, "gr_gid", gr->gr_gid);
	xpc_object_t members = xpc_array_create(NULL, 0);
	for (char **m = gr->gr_mem; m != NULL && *m != NULL; m++)
		xpc_array_set_string(members, XPC_ARRAY_APPEND, *m);
	xpc_dictionary_set_value(d, "gr_mem", members);
	xpc_release(members);
}

static int64_t number(xpc_object_t request, const char *key, int *found) {
	xpc_object_t v = xpc_dictionary_get_value(request, key);
	*found = 0;
	if (v == NULL)
		return 0;
	*found = 1;
	if (xpc_get_type(v) == XPC_TYPE_INT64)
		return xpc_int64_get_value(v);
	if (xpc_get_type(v) == XPC_TYPE_UINT64)
		return (int64_t)xpc_uint64_get_value(v);
	if (xpc_get_type(v) == XPC_TYPE_STRING)
		return strtoll(xpc_string_get_string_ptr(v), NULL, 10);
	*found = 0;
	return 0;
}

/* Fills the reply; false: this routine is not answered here. */
static int answer(xpc_object_t request, xpc_object_t reply) {
	const char *rpc = xpc_dictionary_get_string(request, OD_RPC_NAME);
	const char *name = xpc_dictionary_get_string(request, "name");
	int found;

	if (rpc == NULL)
		return 0;
	if (strcmp(rpc, "getpwnam") == 0 || strcmp(rpc, "getpwuid") == 0) {
		struct passwd *pw = NULL;
		if (rpc[5] == 'n' && name != NULL)
			pw = getpwnam(name);
		else if (rpc[5] == 'u') {
			int64_t uid = number(request, "uid", &found);
			if (found)
				pw = getpwuid((uid_t)uid);
		}
		if (pw == NULL)
			xpc_dictionary_set_int64(reply, OD_RPC_ERROR, ENOENT);
		else
			set_user(reply, pw);
		return 1;
	}
	if (strcmp(rpc, "getgrnam") == 0 || strcmp(rpc, "getgrgid") == 0) {
		struct group *gr = NULL;
		if (rpc[5] == 'n' && name != NULL)
			gr = getgrnam(name);
		else if (rpc[5] == 'g') {
			int64_t gid = number(request, "gid", &found);
			if (found)
				gr = getgrgid((gid_t)gid);
		}
		if (gr == NULL)
			xpc_dictionary_set_int64(reply, OD_RPC_ERROR, ENOENT);
		else
			set_group(reply, gr);
		return 1;
	}
	if (strcmp(rpc, "getpwent") == 0 || strcmp(rpc, "getgrent") == 0) {
		xpc_object_t list = xpc_array_create(NULL, 0);
		if (rpc[3] == 'p') {
			struct passwd *pw;
			setpwent();
			while ((pw = getpwent()) != NULL) {
				xpc_object_t d = xpc_dictionary_create(NULL, NULL, 0);
				set_user(d, pw);
				xpc_array_append_value(list, d);
				xpc_release(d);
			}
			endpwent();
		} else {
			struct group *gr;
			setgrent();
			while ((gr = getgrent()) != NULL) {
				xpc_object_t d = xpc_dictionary_create(NULL, NULL, 0);
				set_group(d, gr);
				xpc_array_append_value(list, d);
				xpc_release(d);
			}
			endgrent();
		}
		xpc_dictionary_set_value(reply, OD_RPC_RESULT, list);
		xpc_release(list);
		return 1;
	}
	if (strcmp(rpc, "getgrouplist") == 0 && name != NULL) {
		struct passwd *pw = getpwnam(name);
		int ngroups = (int)number(request, "ngroups", &found);
		if (!found || ngroups <= 0 || ngroups > 1024)
			ngroups = 1024;
		gid_t *groups = calloc((size_t)ngroups, sizeof(gid_t));
		int count = ngroups;
		if (groups == NULL || pw == NULL)
			count = 0;
		else
			/* Too many groups: the first ngroups are given. */
			(void)getgrouplist(name, (int)pw->pw_gid, (int *)groups, &count);
		if (count < 0)
			count = 0;
		if (count > ngroups)
			count = ngroups;
		xpc_dictionary_set_int64(reply, "count", count);
		xpc_dictionary_set_data(reply, "groups", groups != NULL ? groups : (gid_t *)"", (size_t)count * sizeof(gid_t));
		free(groups);
		return 1;
	}
	return 0;
}

/* ---- The account files watched ------------------------------------------ */

static const char *const watched[] = { "/etc/master.passwd", "/etc/passwd", "/etc/group" };
static struct timespec seen[3];
static dispatch_source_t fileSources[3];

static void invalidate(void) {
	notify_post("com.apple.system.DirectoryService.InvalidateCache");
	notify_post("com.apple.system.DirectoryService.InvalidateCache.user");
	notify_post("com.apple.system.DirectoryService.InvalidateCache.group");
}

static void watchFile(int i);

/* Any file whose time changed: the caches are out of date. A file
   replaced (renamed over) is watched anew. */
static void check(void) {
	int changed = 0;
	for (int i = 0; i < 3; i++) {
		struct stat st;
		struct timespec t = { 0, 0 };
		if (stat(watched[i], &st) == 0)
			t = st.st_mtimespec;
		if (t.tv_sec != seen[i].tv_sec || t.tv_nsec != seen[i].tv_nsec)
			changed = 1;
		seen[i] = t;
	}
	if (changed)
		invalidate();
}

static void watchFile(int i) {
	if (fileSources[i] != NULL) {
		dispatch_source_cancel(fileSources[i]);
		fileSources[i] = NULL;
	}
	int fd = open(watched[i], O_EVTONLY);
	if (fd < 0)
		return;
	dispatch_source_t s = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, (uintptr_t)fd,
	    DISPATCH_VNODE_WRITE | DISPATCH_VNODE_EXTEND | DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME,
	    dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
	dispatch_source_set_event_handler(s, ^{
		unsigned long what = dispatch_source_get_data(s);
		check();
		if (what & (DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME))
			watchFile(i);
	});
	dispatch_source_set_cancel_handler(s, ^{ close(fd); });
	fileSources[i] = s;
	dispatch_resume(s);
}

/* /etc itself: a file replaced by a rename shows there. */
static void watchAccounts(void) {
	dispatch_queue_t q = dispatch_queue_create("org.minkar.directoryd.watch", DISPATCH_QUEUE_SERIAL);
	dispatch_async(q, ^{
		check();
		for (int i = 0; i < 3; i++)
			watchFile(i);
	});
	int dir = open("/etc/", O_EVTONLY);
	if (dir < 0)
		return;
	dispatch_source_t s = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, (uintptr_t)dir,
	    DISPATCH_VNODE_WRITE, q);
	dispatch_source_set_event_handler(s, ^{
		check();
		for (int i = 0; i < 3; i++)
			watchFile(i);
	});
	dispatch_resume(s);
}

int main(void) {
	/* libinfo asks the files, not this service, for this process, and
	   keeps nothing: each answer is read from the files. */
	setenv("XBS_DISABLE_LIBINFO", "YES", 1);
	gL1CacheEnabled = 0;

	mach_port_t port = MACH_PORT_NULL;
	kern_return_t kr = bootstrap_check_in(bootstrap_port, SERVICE, &port);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "directoryd: no %s service (0x%x)\n", SERVICE, kr);
		return 1;
	}
	watchAccounts();
	/* OpenDirectory's API, answered on its own queue. */
	odapi_start();
	for (;;) {
		xpc_object_t request = NULL;
		mach_port_t reply_port = MACH_PORT_NULL;
		uint64_t id = 0;
		if (xpc_pipe_receive_request(port, &request, &reply_port, &id) != 0)
			continue;
		audit_token_t token;
		memset(&token, 0xff, sizeof(token));
		xpc_dictionary_get_audit_token(request, &token);
		caller_is_root = token.val[1] == 0 &&
		    sandbox_check((pid_t)token.val[5], NULL, SANDBOX_FILTER_NONE) == 0;
		xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
		if (!answer(request, reply))
			xpc_dictionary_set_int64(reply, OD_RPC_ERROR, ENOTSUP);
		xpc_pipe_send_reply(reply, reply_port, id);
		xpc_release(reply);
		xpc_release(request);
	}
}
