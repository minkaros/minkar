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
 * uuidgen: macOS's, written for Minkar. Prints a new random (version 4)
 * UUID in upper case; -hdr prints it as a C header line for Core
 * Foundation, as macOS does.
 */

#include <stdio.h>
#include <string.h>

#include <uuid/uuid.h>

static void
usage(void)
{
	fprintf(stderr, "usage: uuidgen [-hdr]\n");
}

int
main(int argc, char *argv[])
{
	uuid_t u;
	uuid_string_t s;
	int hdr = 0, i;

	if (argc == 2 && strcmp(argv[1], "-hdr") == 0)
		hdr = 1;
	else if (argc != 1) {
		usage();
		return (1);
	}

	uuid_generate_random(u);
	uuid_unparse_upper(u, s);
	if (!hdr) {
		printf("%s\n", s);
		return (0);
	}
	printf("// %s\n#define <#Name#> CFUUIDGetConstantUUIDWithBytes("
	    "kCFAllocatorSystemDefault, ", s);
	for (i = 0; i < 16; i++)
		printf("0x%02X%s", u[i], i < 15 ? ", " : ")\n");
	return (0);
}
