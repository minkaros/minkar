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
 * dynamic_pager: set the path prefix of the swap files. The kernel's
 * compressor creates, grows and removes them itself (vm.swapfileprefix,
 * "<prefix>0", "<prefix>1"...); this makes sure their directory exists and
 * tells the kernel the prefix, then exits. -S, -H and -L, sizes and water
 * marks older versions used, are accepted and ignored, as on macOS.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_PREFIX	"/private/var/vm/swapfile"

static void
usage(void)
{
	fprintf(stderr, "usage: dynamic_pager [-F filename] [-S filesize] "
	    "[-H high-water-trigger] [-L low-water-trigger]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	const char *prefix = DEFAULT_PREFIX;
	char dir[PATH_MAX];
	struct stat sb;
	int ch;

	while ((ch = getopt(argc, argv, "F:S:H:L:P:")) != -1) {
		switch (ch) {
		case 'F':
			prefix = optarg;
			break;
		case 'S':
		case 'H':
		case 'L':
		case 'P':
			break;		/* sizes and triggers: the kernel's now */
		default:
			usage();
		}
	}
	if (optind != argc)
		usage();
	if (prefix[0] != '/')
		errx(1, "%s: the swap file prefix must be an absolute path", prefix);

	if (strlcpy(dir, prefix, sizeof(dir)) >= sizeof(dir))
		errx(1, "%s: path too long", prefix);
	char *d = dirname(dir);
	if (stat(d, &sb) == -1) {
		if (errno != ENOENT || mkdir(d, 0755) == -1)
			err(1, "%s", d);
	} else if (!S_ISDIR(sb.st_mode)) {
		errx(1, "%s: not a directory", d);
	}

	if (sysctlbyname("vm.swapfileprefix", NULL, NULL, (void *)prefix,
	    strlen(prefix) + 1) == -1)
		err(1, "vm.swapfileprefix");
	return (0);
}
