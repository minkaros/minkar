/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * /bin/sh: runs the shell that /private/var/select/sh names (bash by
 * default; dash and zsh are the other choices), with the same arguments
 * and the name sh, so that bash works as a POSIX shell. Scripts written for
 * the reference system's sh, bash in that mode, use what it adds ([[ ]],
 * $'...', local, pipefail).
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SELECT_PATH "/private/var/select/sh"
#define DEFAULT_SHELL "/bin/bash"

/* The shells /bin/sh may be: a link to anything else is not followed. */
static const char *const allowed[] = { "/bin/bash", "/bin/dash", "/bin/zsh", NULL };

static const char *selected(char *buffer, size_t size) {
	ssize_t length = readlink(SELECT_PATH, buffer, size - 1);
	int i;

	if (length <= 0)
		return DEFAULT_SHELL;
	buffer[length] = '\0';
	for (i = 0; allowed[i] != NULL; i++)
		if (strcmp(buffer, allowed[i]) == 0)
			return buffer;
	return DEFAULT_SHELL;
}

int main(int argc, char *argv[]) {
	char buffer[PATH_MAX];
	const char *shell = selected(buffer, sizeof(buffer));

	(void)argc;
	execv(shell, argv);
	if (strcmp(shell, DEFAULT_SHELL) != 0)
		execv(DEFAULT_SHELL, argv);
	fprintf(stderr, "sh: cannot run %s: %s\n", shell, strerror(errno));
	return 126;
}
