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
 */

#include <bsm/audit.h>
#include <errno.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <xpc/xpc.h>
#include <xpc/launchd.h>

#define SERVICE "com.apple.bsd.dirhelper"

extern int __dirhelper_create_user(uid_t uid, gid_t gid);

int main(void) {
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
