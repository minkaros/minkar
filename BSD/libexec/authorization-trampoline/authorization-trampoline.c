/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * authorization-trampoline: runs a tool as root for
 * AuthorizationExecuteWithPrivileges (Security.framework). Installed setuid
 * root. Its caller gives an authorization's external form on descriptor 3
 * (a socket) and the tool and its arguments as its own; the trampoline asks authd,
 * without any interaction, whether that authorization holds the
 * administrator's right (system.privilege.admin, which the caller obtained
 * first, asking the user), and only then becomes root wholly and runs the
 * tool. It answers on descriptor 3 with the result (an OSStatus) before
 * running the tool, so the caller may free its authorization. Anyone else
 * is refused.
 *
 *   authorization-trampoline tool [argument ...]   (the form on fd 3)
 */

#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

/* The check's result, to the caller (who waits for it), on descriptor 3. */
static void
answer(OSStatus status)
{
	(void)write(3, &status, sizeof(status));
	close(3);
}

int
main(int argc, char *argv[])
{
	AuthorizationExternalForm external;
	AuthorizationRef authorization = NULL;
	AuthorizationItem item;
	AuthorizationRights rights = { 1, &item };
	ssize_t got;
	OSStatus status;

	if (argc < 2) {
		fprintf(stderr, "usage: authorization-trampoline tool [argument ...]\n");
		return 2;
	}
	got = read(3, &external, sizeof(external));
	if (got != (ssize_t)sizeof(external)) {
		fprintf(stderr, "authorization-trampoline: no authorization given\n");
		return 1;
	}
	if ((status = AuthorizationCreateFromExternalForm(&external, &authorization)) != errAuthorizationSuccess) {
		answer(status);
		fprintf(stderr, "authorization-trampoline: the authorization is not valid (%d)\n", (int)status);
		return 1;
	}
	item.name = kAuthorizationRightExecute;
	item.valueLength = strlen(argv[1]);
	item.value = argv[1];
	item.flags = 0;
	/* The right already obtained, never asked for again here. */
	status = AuthorizationCopyRights(authorization, &rights, NULL, kAuthorizationFlagExtendRights, NULL);
	AuthorizationFree(authorization, kAuthorizationFlagDefaults);
	answer(status);
	if (status != errAuthorizationSuccess) {
		syslog(LOG_NOTICE, "authorization-trampoline: refused %s for uid %d (%d)", argv[1], (int)getuid(), (int)status);
		fprintf(stderr, "authorization-trampoline: not authorized (%d)\n", (int)status);
		return 1;
	}
	if (setgid(0) != 0 || setuid(0) != 0) {
		fprintf(stderr, "authorization-trampoline: cannot become root: %s\n", strerror(errno));
		return 1;
	}
	syslog(LOG_INFO, "authorization-trampoline: running %s for an administrator", argv[1]);
	execv(argv[1], argv + 1);
	fprintf(stderr, "authorization-trampoline: %s: %s\n", argv[1], strerror(errno));
	return 1;
}
