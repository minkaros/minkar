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
 * groups and group lists. Password hashes are never given: pw_passwd and
 * gr_passwd are "*". Other routines get an error, and libinfo asks the
 * next module.
 */

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xpc/xpc.h>
#include <xpc/launchd.h>

#define SERVICE		"com.apple.system.opendirectoryd.libinfo"
#define OD_RPC_NAME	"rpc_name"
#define OD_RPC_RESULT	"result"
#define OD_RPC_ERROR	"error"

static void set_user(xpc_object_t d, const struct passwd *pw) {
	xpc_dictionary_set_string(d, "pw_name", pw->pw_name);
	xpc_dictionary_set_string(d, "pw_passwd", "*");
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
	xpc_dictionary_set_string(d, "gr_passwd", "*");
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

int main(void) {
	/* libinfo asks the files, not this service, for this process. */
	setenv("XBS_DISABLE_LIBINFO", "YES", 1);

	mach_port_t port = MACH_PORT_NULL;
	kern_return_t kr = bootstrap_check_in(bootstrap_port, SERVICE, &port);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "directoryd: no %s service (0x%x)\n", SERVICE, kr);
		return 1;
	}
	for (;;) {
		xpc_object_t request = NULL;
		mach_port_t reply_port = MACH_PORT_NULL;
		uint64_t id = 0;
		if (xpc_pipe_receive_request(port, &request, &reply_port, &id) != 0)
			continue;
		xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
		if (!answer(request, reply))
			xpc_dictionary_set_int64(reply, OD_RPC_ERROR, ENOTSUP);
		xpc_pipe_send_reply(reply, reply_port, id);
		xpc_release(reply);
		xpc_release(request);
	}
}
