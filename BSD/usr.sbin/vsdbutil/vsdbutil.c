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
 * vsdbutil: turn the use of owners and permissions on a volume on (-a,
 * adopt) or off (-d, disown), or show it (-c). The volume is the one the
 * path is on; it is updated in place with mount -u -o owners or noowners.
 * macOS also keeps the choice in a database DiskArbitration applies at
 * the next mount; Minkar has no DiskArbitration yet, so -i, which fills
 * that database, has nothing to do.
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/wait.h>

#include <err.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

static void
usage(void)
{
	fprintf(stderr, "usage: vsdbutil -a path | -c path | -d path | -i\n");
	exit(1);
}

static int
remount(const struct statfs *sf, int owners)
{
	char *argv[] = { "mount", "-u", "-o", owners ? "owners" : "noowners",
	    (char *)sf->f_mntonname, NULL };
	pid_t pid;
	int status;

	if (posix_spawn(&pid, "/sbin/mount", NULL, NULL, argv, environ) != 0)
		err(1, "/sbin/mount");
	if (waitpid(pid, &status, 0) == -1)
		err(1, "waitpid");
	return (WIFEXITED(status) ? WEXITSTATUS(status) : 1);
}

int
main(int argc, char *argv[])
{
	struct statfs sf;
	int ch, mode = 0;
	const char *path = NULL;

	while ((ch = getopt(argc, argv, "a:c:d:i")) != -1) {
		if (mode != 0)
			usage();
		mode = ch;
		if (ch != 'i')
			path = optarg;
	}
	if (mode == 0 || optind != argc)
		usage();
	if (mode == 'i')
		return (0);	/* no volume database without DiskArbitration */

	if (statfs(path, &sf) == -1)
		err(1, "%s", path);
	switch (mode) {
	case 'c':
		printf("Permissions on '%s' are %s.\n", sf.f_mntonname,
		    (sf.f_flags & MNT_IGNORE_OWNERSHIP) ? "disabled" : "enabled");
		return (0);
	case 'a':
		return (remount(&sf, 1));
	case 'd':
		return (remount(&sf, 0));
	}
	usage();
	return (1);
}
