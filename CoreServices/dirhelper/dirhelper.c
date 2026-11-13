/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * dirhelper: creates a user's own folders under /var/folders (see
 * libsystem_coreservices' dirhelper.c), which only root may do. Each
 * request is for its sender: the user and group come from the message's
 * audit token, never from the request. The reply's "error" is 0 or an
 * errno.
 *
 * "dirhelper -machineBoot" (launchctl, at every boot, before the
 * services) empties each user's temporary folder (T) and exits.
 */

#include <bsm/audit.h>
#include <dirent.h>
#include <errno.h>
#include <fts.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <xpc/xpc.h>
#include <xpc/launchd.h>

#define SERVICE "com.apple.bsd.dirhelper"

extern int __dirhelper_create_user(uid_t uid, gid_t gid);

#define FOLDERS "/private/var/folders"

/* Removes what is inside dir, not dir itself: links are removed, not
 * followed, and other volumes are not entered. */
static void empty(const char *dir) {
	char *const paths[] = { (char *)dir, NULL };
	FTS *fts = fts_open(paths, FTS_PHYSICAL | FTS_XDEV | FTS_NOCHDIR, NULL);
	FTSENT *e;

	if (fts == NULL)
		return;
	while ((e = fts_read(fts)) != NULL) {
		if (e->fts_level == FTS_ROOTLEVEL)
			continue;
		switch (e->fts_info) {
		case FTS_DP:
			(void)rmdir(e->fts_accpath);
			break;
		case FTS_D:
			break;
		default:
			(void)unlink(e->fts_accpath);
			break;
		}
	}
	fts_close(fts);
}

/* At boot: every user's T emptied (/var/folders/<xx>/<user>/T). */
static int machineBoot(void) {
	DIR *top = opendir(FOLDERS);
	struct dirent *a, *b;

	if (top == NULL)
		return 0;
	while ((a = readdir(top)) != NULL) {
		if (a->d_name[0] == '.')
			continue;
		char level[512];
		snprintf(level, sizeof(level), "%s/%s", FOLDERS, a->d_name);
		DIR *users = opendir(level);
		if (users == NULL)
			continue;
		while ((b = readdir(users)) != NULL) {
			if (b->d_name[0] == '.')
				continue;
			char temp[768];
			struct stat st;
			snprintf(temp, sizeof(temp), "%s/%s/T", level, b->d_name);
			if (lstat(temp, &st) == 0 && S_ISDIR(st.st_mode))
				empty(temp);
		}
		closedir(users);
	}
	closedir(top);
	return 0;
}

int main(int argc, char *argv[]) {
	if (argc > 1 && strcmp(argv[1], "-machineBoot") == 0)
		return machineBoot();
	if (argc > 1) {
		fprintf(stderr, "usage: dirhelper [-machineBoot]\n");
		return 2;
	}

	mach_port_t port = MACH_PORT_NULL;
	kern_return_t kr = bootstrap_check_in(bootstrap_port, SERVICE, &port);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "dirhelper: no %s service (0x%x)\n", SERVICE, kr);
		return 1;
	}
	for (;;) {
		xpc_object_t request = NULL;
		mach_port_t reply_port = MACH_PORT_NULL;
		uint64_t id = 0;
		if (xpc_pipe_receive_request(port, &request, &reply_port, &id) != 0)
			continue;
		audit_token_t token;
		memset(&token, 0xff, sizeof(token));
		xpc_dictionary_get_audit_token(request, &token);
		/* The token's fields: auid, euid, egid, ruid, rgid, pid, asid. */
		uid_t uid = (uid_t)token.val[1];
		gid_t gid = (gid_t)token.val[2];
		int error = 0;
		if (uid == (uid_t)-1)
			error = EPERM;
		else if (__dirhelper_create_user(uid, gid) != 0)
			error = errno;
		xpc_object_t reply = xpc_dictionary_create(NULL, NULL, 0);
		xpc_dictionary_set_int64(reply, "error", error);
		xpc_pipe_send_reply(reply, reply_port, id);
		xpc_release(reply);
		xpc_release(request);
	}
}
