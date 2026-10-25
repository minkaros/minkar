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
 * uptime: how long the system has been running, as macOS prints it:
 *
 *   14:32  up 3 days,  2:01, 2 users, load averages: 1.23 1.10 0.98
 */

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>

static void
usage(void)
{
	fprintf(stderr, "usage: uptime\n");
	exit(1);
}

static int
count_users(void)
{
	struct utmpx *ut;
	int n = 0;

	setutxent();
	while ((ut = getutxent()) != NULL)
		if (ut->ut_type == USER_PROCESS)
			n++;
	endutxent();
	return n;
}

int
main(int argc, char *argv[])
{
	int mib[2] = { CTL_KERN, KERN_BOOTTIME };
	struct timeval boottime;
	size_t len = sizeof(boottime);
	double load[3];
	time_t now, up;
	char clock[16];
	int days, hours, mins, users;

	if (getopt(argc, argv, "") != -1 || argc > 1)
		usage();

	if (sysctl(mib, 2, &boottime, &len, NULL, 0) == -1 ||
	    boottime.tv_sec == 0)
		err(1, "sysctl kern.boottime");
	time(&now);
	up = now - boottime.tv_sec;
	if (up < 0)
		up = 0;
	up += 30;	/* round to the minute */
	days = (int)(up / 86400);
	hours = (int)(up % 86400 / 3600);
	mins = (int)(up % 3600 / 60);

	strftime(clock, sizeof(clock), "%k:%M", localtime(&now));
	printf("%s  up", clock);
	if (days > 0)
		printf(" %d day%s,", days, days == 1 ? "" : "s");
	if (hours > 0 && mins > 0)
		printf(" %2d:%02d,", hours, mins);
	else if (hours > 0)
		printf(" %d hr%s,", hours, hours == 1 ? "" : "s");
	else
		printf(" %d min%s,", mins, mins == 1 ? "" : "s");

	users = count_users();
	printf(" %d user%s", users, users == 1 ? "" : "s");

	if (getloadavg(load, 3) == 3)
		printf(", load averages: %.2f %.2f %.2f", load[0], load[1], load[2]);
	printf("\n");
	return 0;
}
