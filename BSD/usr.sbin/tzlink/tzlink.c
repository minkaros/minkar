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
 * tzlink: set the system's time zone, as on macOS: /etc/localtime becomes
 * a link to the zone's file in /var/db/timezone/zoneinfo, replaced in one
 * rename, and running programs are told (the notification libc's
 * localtime listens to).
 */

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <notify.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	ZONEDIR		"/var/db/timezone/zoneinfo"
#define	LOCALTIME	"/etc/localtime"
#define	TZ_NOTIFY	"com.apple.system.timezone"	/* libc's name for it */

int
main(int argc, char *argv[])
{
	char target[PATH_MAX], tmp[PATH_MAX];
	struct stat st;
	const char *zone;

	if (argc != 2) {
		fprintf(stderr, "usage: tzlink zone\n");
		return (1);
	}
	zone = argv[1];
	if (zone[0] == '/' || strstr(zone, "..") != NULL)
		errx(1, "%s: not a time zone name", zone);
	if (snprintf(target, sizeof(target), "%s/%s", ZONEDIR, zone) >=
	    (int)sizeof(target))
		errx(1, "%s: name too long", zone);
	if (stat(target, &st) == -1 || !S_ISREG(st.st_mode))
		errx(1, "%s: no such time zone", zone);
	if (geteuid() != 0)
		errx(1, "only the super-user may change the time zone");

	snprintf(tmp, sizeof(tmp), "%s.tzlink.%d", LOCALTIME, (int)getpid());
	(void)unlink(tmp);
	if (symlink(target, tmp) == -1)
		err(1, "%s", tmp);
	if (rename(tmp, LOCALTIME) == -1) {
		int e = errno;

		(void)unlink(tmp);
		errc(1, e, "%s", LOCALTIME);
	}
	(void)notify_post(TZ_NOTIFY);
	return (0);
}
