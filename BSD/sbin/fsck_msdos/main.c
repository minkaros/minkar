/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 1995 Wolfgang Solfrank
 * Copyright (c) 1995 Martin Husemann
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: main.c,v 1.10 1997/10/01 02:18:14 enami Exp $");
#endif /* not lint */

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#include "fsutil.h"
#include "ext.h"

int alwaysno;		/* assume "no" for all questions */
int alwaysyes;		/* assume "yes" for all questions */
int preen;		/* set when preening */
int rdonly;		/* device is opened read only (supersedes above) */
int skipclean;		/* skip clean file systems if preening */
int allow_mmap;		/* Allow the use of mmap(), if possible */
static int quick;	/* -q: only whether it was unmounted cleanly */

static void usage(void) __dead2;
static int quickcheck(const char *);

/*
 * Minkar: -q, as macOS's fsck_msdos (diskarbitrationd asks it before
 * mounting): 0 when the file system was unmounted cleanly, 3 when not
 * (FAT12 keeps no such flag: clean), 8 when it is not a FAT file system.
 */
static int
quickcheck(const char *fname)
{
	struct bootblock boot;
	int fd, ret;

	rdonly = 1;
	if ((fd = open(fname, O_RDONLY, 0)) < 0) {
		perr("Can't open `%s'", fname);
		return 8;
	}
	if (readboot(fd, &boot) == FSFATAL) {
		close(fd);
		return 8;
	}
	if (boot.ClustMask == CLUST12_MASK || checkdirty(fd, &boot))
		ret = 0;
	else
		ret = 3;
	close(fd);
	printf("** %s: %s\n", fname, ret == 0 ? "clean" : "dirty");
	return ret;
}

static void
usage(void)
{

	fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: fsck_msdosfs -p [-f] filesystem ...",
	    "       fsck_msdosfs [-ny] filesystem ...",
	    "       fsck_msdosfs -q filesystem ...");
	exit(1);
}

int
main(int argc, char **argv)
{
	int ret = 0, erg;
	int ch;

	skipclean = 1;
	allow_mmap = 1;
	while ((ch = getopt(argc, argv, "BCfFnpqyM")) != -1) {
		switch (ch) {
		case 'B': /* for fsck_ffs compatibility */
		case 'C':
			break;
		case 'f':
			skipclean = 0;
			break;
		case 'F':
			/*
			 * We can never run in the background.  We must exit
			 * silently with a nonzero exit code so that fsck(8)
			 * can probe our support for -F.  The exit code
			 * doesn't really matter, but we use an unusual one
			 * in case someone tries -F directly.  The -F flag
			 * is intentionally left out of the usage message.
			 */
			exit(5);
		case 'n':
			alwaysno = 1;
			alwaysyes = 0;
			break;
		case 'y':
			alwaysyes = 1;
			alwaysno = 0;
			break;

		case 'p':
			preen = 1;
			break;

		case 'M':
			allow_mmap = 0;
			break;

		case 'q':	/* macOS: only whether it was unmounted cleanly */
			quick = 1;
			break;

		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (!argc)
		usage();

	while (--argc >= 0) {
		if (quick) {
			erg = quickcheck(*argv++);
			if (erg > ret)
				ret = erg;
			continue;
		}
		setcdevname(*argv, preen);
		erg = checkfilesys(*argv++);
		if (erg > ret)
			ret = erg;
	}

	return ret;
}


/*VARARGS*/
int
ask(int def, const char *fmt, ...)
{
	va_list ap;

	char prompt[256];
	int c;

	if (alwaysyes || alwaysno || rdonly)
		def = (alwaysyes && !rdonly && !alwaysno);

	if (preen) {
		if (def)
			printf("FIXED\n");
		return def;
	}

	va_start(ap, fmt);
	vsnprintf(prompt, sizeof(prompt), fmt, ap);
	va_end(ap);
	if (alwaysyes || alwaysno || rdonly) {
		printf("%s? %s\n", prompt, def ? "yes" : "no");
		return def;
	}
	do {
		printf("%s? [yn] ", prompt);
		fflush(stdout);
		c = getchar();
		while (c != '\n' && getchar() != '\n')
			if (feof(stdin))
				return 0;
	} while (c != 'y' && c != 'Y' && c != 'n' && c != 'N');
	return c == 'y' || c == 'Y';
}
