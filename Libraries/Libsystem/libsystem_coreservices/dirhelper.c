/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * _dirhelper: each user's own folders under /var/folders, which
 * confstr(_CS_DARWIN_USER_DIR, _CS_DARWIN_USER_TEMP_DIR,
 * _CS_DARWIN_USER_CACHE_DIR) gives: .../0/, .../T/ and .../C/.
 *
 * /var/folders/<xx>/ belongs to root; a user's folder in it belongs to the
 * user, and 0, T and C are theirs only (0700). A folder that exists but is
 * not the user's, or that others may enter, is refused. Root creates them
 * itself; other users ask the dirhelper service (root), which creates the
 * caller's only.
 */

#include <errno.h>
#include <mach/mach.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xpc/xpc.h>
#include <xpc/launchd.h>

#define FOLDERS		"/var/folders"
#define SERVICE		"com.apple.bsd.dirhelper"

enum { DIRHELPER_USER_LOCAL = 0, DIRHELPER_USER_LOCAL_TEMP, DIRHELPER_USER_LOCAL_CACHE };

/* The user's folder, ending in '/'. */
static int user_folder(uid_t uid, char *path, size_t size) {
	int n = snprintf(path, size, FOLDERS "/%02x/u%u/", (unsigned)(uid & 0xff), (unsigned)uid);
	if (n < 0 || (size_t)n >= size) {
		errno = ENAMETOOLONG;
		return -1;
	}
	return 0;
}

/* A directory owned by uid, which others may not change (and, when
   private, not enter). */
static int is_ours(const char *path, uid_t uid, int private) {
	struct stat st;
	if (lstat(path, &st) != 0)
		return 0;
	if (!S_ISDIR(st.st_mode) || st.st_uid != uid)
		return 0;
	return (st.st_mode & (private ? 077 : 022)) == 0;
}

static int make_dir(const char *path, mode_t mode, uid_t uid, gid_t gid, int private) {
	if (mkdir(path, mode) == 0) {
		if (chown(path, uid, gid) != 0 || chmod(path, mode) != 0)
			return -1;
	} else if (errno != EEXIST)
		return -1;
	if (!is_ours(path, uid, private)) {
		errno = EACCES;
		return -1;
	}
	return 0;
}

/* Root only: creates (or checks) the user's folders. Used by the
   dirhelper service, and by root's own processes. 0 or -1 (errno). */
int __dirhelper_create_user(uid_t uid, gid_t gid) {
	char path[256], sub[260];

	if (geteuid() != 0) {
		errno = EPERM;
		return -1;
	}
	if (make_dir(FOLDERS, 0755, 0, 0, 0) != 0)
		return -1;
	snprintf(path, sizeof(path), FOLDERS "/%02x", (unsigned)(uid & 0xff));
	if (make_dir(path, 0755, 0, 0, 0) != 0)
		return -1;
	if (user_folder(uid, path, sizeof(path)) != 0)
		return -1;
	if (make_dir(path, 0755, uid, gid, 0) != 0)
		return -1;
	static const char *const subs[] = { "0", "T", "C" };
	for (size_t i = 0; i < sizeof(subs) / sizeof(subs[0]); i++) {
		snprintf(sub, sizeof(sub), "%s%s", path, subs[i]);
		if (make_dir(sub, 0700, uid, gid, 1) != 0)
			return -1;
	}
	return 0;
}

static int ask_service(void) {
	xpc_pipe_t pipe = xpc_pipe_create(SERVICE, XPC_PIPE_FLAG_PRIVILEGED);
	if (pipe == NULL) {
		errno = EIO;
		return -1;
	}
	xpc_object_t request = xpc_dictionary_create(NULL, NULL, 0);
	xpc_object_t reply = NULL;
	int r = xpc_pipe_routine(pipe, request, &reply);
	xpc_release(request);
	xpc_release(pipe);
	if (r != 0) {
		errno = EIO;
		return -1;
	}
	int64_t error = xpc_dictionary_get_int64(reply, "error");
	xpc_release(reply);
	if (error != 0) {
		errno = (int)error;
		return -1;
	}
	return 0;
}

char *_dirhelper(int which, char *path, size_t pathlen) {
	static const char *const subs[] = { "0/", "T/", "C/" };
	char folder[256];
	uid_t uid = geteuid();

	if (which < DIRHELPER_USER_LOCAL || which > DIRHELPER_USER_LOCAL_CACHE || path == NULL) {
		errno = EINVAL;
		return NULL;
	}
	if (user_folder(uid, folder, sizeof(folder)) != 0)
		return NULL;
	if (snprintf(path, pathlen, "%s%s", folder, subs[which]) >= (int)pathlen) {
		errno = ENAMETOOLONG;
		return NULL;
	}
	if (is_ours(folder, uid, 0) && is_ours(path, uid, 1))
		return path;
	int r = uid == 0 ? __dirhelper_create_user(0, 0) : ask_service();
	if (r != 0 || !is_ours(folder, uid, 0) || !is_ours(path, uid, 1)) {
		if (r == 0)
			errno = EACCES;
		return NULL;
	}
	return path;
}

/* libSystem calls it in a forked child: nothing is kept across fork. */
void _libcoreservices_fork_child(void) {
}
