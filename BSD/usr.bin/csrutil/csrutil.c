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
 * csrutil: System Integrity Protection's configuration as the kernel has
 * it (csr_get_active_config). status describes it; enable, disable and
 * clear change it on macOS only from its Recovery system, which Minkar does
 * not have, and say so, as macOS does outside Recovery.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint32_t csr_config_t;
int csr_get_active_config(csr_config_t *config);	/* libsystem_kernel */

/* What each configuration line describes: the restriction is enabled
 * while its "allow" bit is clear. */
static const struct {
	const char	*name;
	csr_config_t	allow;
} restrictions[] = {
	{ "Apple Internal",		1 << 4 },	/* shown as enabled when allowed */
	{ "Kext Signing",		1 << 0 },
	{ "Filesystem Protections",	1 << 1 },
	{ "Debugging Restrictions",	1 << 2 },
	{ "DTrace Restrictions",	1 << 5 },
	{ "NVRAM Protections",		1 << 6 },
};
#define NRESTRICTIONS	(sizeof(restrictions) / sizeof(restrictions[0]))
#define ALLOW_ALL	((1 << 0) | (1 << 1) | (1 << 2) | (1 << 5) | (1 << 6))

static void
usage(void)
{
	fprintf(stderr, "usage: csrutil <command>\n"
	    "Modify the System Integrity Protection configuration.\n"
	    "Available commands:\n\n"
	    "    clear\n        Clear the existing configuration.\n"
	    "    disable\n        Disable the protection on the machine.\n"
	    "    enable\n        Enable the protection on the machine.\n"
	    "    status\n        Display the current configuration.\n");
	exit(1);
}

static int
status(void)
{
	csr_config_t config = 0;
	unsigned i;

	if (csr_get_active_config(&config) != 0) {
		perror("csrutil: csr_get_active_config");
		return (1);
	}
	if ((config & ALLOW_ALL) == 0 && (config & (1 << 4)) == 0) {
		printf("System Integrity Protection status: enabled.\n");
		return (0);
	}
	if ((config & ALLOW_ALL) == ALLOW_ALL && (config & (1 << 4)) == 0) {
		printf("System Integrity Protection status: disabled.\n");
		return (0);
	}
	printf("System Integrity Protection status: enabled (Custom Configuration).\n\n"
	    "Configuration:\n");
	for (i = 0; i < NRESTRICTIONS; i++) {
		int on = (config & restrictions[i].allow) == 0;

		if (restrictions[i].allow == (1 << 4))
			on = !on;
		printf("\t%s: %s\n", restrictions[i].name, on ? "enabled" : "disabled");
	}
	printf("\nThis is an unsupported configuration, likely to break in the "
	    "future and leave your machine in an unknown state.\n");
	return (0);
}

int
main(int argc, char *argv[])
{
	if (argc != 2)
		usage();
	if (strcmp(argv[1], "status") == 0)
		return (status());
	if (strcmp(argv[1], "enable") == 0 || strcmp(argv[1], "disable") == 0 ||
	    strcmp(argv[1], "clear") == 0) {
		fprintf(stderr, "csrutil: failed to modify system integrity "
		    "configuration. This tool needs to be executed from the "
		    "Recovery OS.\n");
		return (1);
	}
	usage();
	return (1);
}
