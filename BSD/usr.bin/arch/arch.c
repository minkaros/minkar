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
 * arch: with no arguments, the machine's architecture family as macOS
 * prints it ("i386" on Intel); with a command, run it preferring the
 * architectures given (-x86_64, -arch x86_64...) in a universal binary,
 * and with environment variables set (-e) or removed (-d).
 */

#include <sys/types.h>
#include <mach/machine.h>

#include <err.h>
#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

#define	MAXPREF	8

static const struct {
	const char	*name;
	cpu_type_t	type;
} archs[] = {
	{ "i386", CPU_TYPE_I386 },
	{ "x86_64", CPU_TYPE_X86_64 },
	{ "x86_64h", CPU_TYPE_X86_64 },
	{ "arm64", CPU_TYPE_ARM64 },
	{ "arm64e", CPU_TYPE_ARM64 },
};

static void
usage(void)
{

	fprintf(stderr,
	    "usage: arch\n"
	    "       arch {-arch_name | -arch arch_name} ... [-c] "
	    "[-d envname] ... [-e envname=value] ... [-h] prog [args ...]\n");
	exit(1);
}

static cpu_type_t
lookup(const char *name)
{
	size_t i;

	for (i = 0; i < sizeof(archs) / sizeof(archs[0]); i++)
		if (strcmp(archs[i].name, name) == 0)
			return (archs[i].type);
	errx(1, "Unknown architecture: %s", name);
}

int
main(int argc, char *argv[])
{
	posix_spawnattr_t attr;
	cpu_type_t prefs[MAXPREF];
	size_t npref = 0, ocount;
	pid_t pid;
	int i, error;

	if (argc == 1) {
		printf("i386\n");
		return (0);
	}
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		const char *a = argv[i] + 1;

		if (strcmp(a, "h") == 0)
			usage();
		else if (strcmp(a, "c") == 0) {
			if (environ != NULL)
				environ[0] = NULL;	/* a clean environment */
		} else if (strcmp(a, "d") == 0) {
			if (++i == argc)
				usage();
			unsetenv(argv[i]);
		} else if (strcmp(a, "e") == 0) {
			char *eq = NULL;

			if (++i < argc)
				eq = strchr(argv[i], '=');
			if (eq == NULL)
				usage();
			*eq = '\0';
			if (setenv(argv[i], eq + 1, 1) != 0)
				err(1, "setenv");
		} else if (strcmp(a, "arch") == 0) {
			if (++i == argc)
				usage();
			if (npref == MAXPREF)
				errx(1, "too many architectures");
			prefs[npref++] = lookup(argv[i]);
		} else {
			if (npref == MAXPREF)
				errx(1, "too many architectures");
			prefs[npref++] = lookup(a);
		}
	}
	if (i == argc)
		usage();

	if ((error = posix_spawnattr_init(&attr)) != 0)
		errc(1, error, "posix_spawnattr_init");
	if ((error = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETEXEC)) != 0)
		errc(1, error, "posix_spawnattr_setflags");
	if (npref > 0 && (error = posix_spawnattr_setbinpref_np(&attr, npref,
	    prefs, &ocount)) != 0)
		errc(1, error, "posix_spawnattr_setbinpref_np");
	error = posix_spawnp(&pid, argv[i], NULL, &attr, &argv[i], environ);
	errc(1, error, "%s", argv[i]);
}
