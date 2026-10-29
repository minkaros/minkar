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
 * vm_stat: the Mach virtual memory statistics, as macOS shows them: once,
 * a line each, or every interval seconds as a table whose first row has
 * the totals and the next ones what changed since the row before.
 */

#include <mach/mach.h>
#include <mach/mach_host.h>

#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static vm_size_t pagesize;

static void
usage(void)
{

	fprintf(stderr, "usage: vm_stat [[-c count] interval]\n");
	exit(1);
}

static void
get(vm_statistics64_data_t *vs)
{
	mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
	kern_return_t kr;

	kr = host_statistics64(mach_host_self(), HOST_VM_INFO64,
	    (host_info64_t)vs, &count);
	if (kr != KERN_SUCCESS)
		errx(1, "host_statistics64: %s", mach_error_string(kr));
}

/* The counters, in the order and with the names macOS uses. */
#define	NFIELDS	22

static void
fields(const vm_statistics64_data_t *v, unsigned long long f[NFIELDS])
{

	f[0] = (unsigned long long)v->free_count - v->speculative_count;
	f[1] = v->active_count;
	f[2] = v->inactive_count;
	f[3] = v->speculative_count;
	f[4] = v->throttled_count;
	f[5] = v->wire_count;
	f[6] = v->purgeable_count;
	f[7] = v->faults;
	f[8] = v->cow_faults;
	f[9] = v->zero_fill_count;
	f[10] = v->reactivations;
	f[11] = v->purges;
	f[12] = v->external_page_count;
	f[13] = v->internal_page_count;
	f[14] = v->total_uncompressed_pages_in_compressor;
	f[15] = v->compressor_page_count;
	f[16] = v->decompressions;
	f[17] = v->compressions;
	f[18] = v->pageins;
	f[19] = v->pageouts;
	f[20] = v->swapins;
	f[21] = v->swapouts;
}

static const char *const names[NFIELDS] = {
	"Pages free:", "Pages active:", "Pages inactive:",
	"Pages speculative:", "Pages throttled:", "Pages wired down:",
	"Pages purgeable:", "\"Translation faults\":", "Pages copy-on-write:",
	"Pages zero filled:", "Pages reactivated:", "Pages purged:",
	"File-backed pages:", "Anonymous pages:",
	"Pages stored in compressor:", "Pages occupied by compressor:",
	"Decompressions:", "Compressions:", "Pageins:", "Pageouts:",
	"Swapins:", "Swapouts:",
};

/* Counts of pages now (as opposed to events since boot). */
static const int gauge[NFIELDS] = {
	1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
};

static const char *const heads[NFIELDS] = {
	"free", "active", "specul", "inactive", "throttle", "wired",
	"prgable", "faults", "copy", "0fill", "reactive", "purged",
	"file-backed", "anonymous", "cmprssed", "cmprssor", "dcomprs",
	"comprs", "pageins", "pageout", "swapins", "swapouts",
};

static void
banner(void)
{

	printf("Mach Virtual Memory Statistics: (page size of %llu bytes)\n",
	    (unsigned long long)pagesize);
}

static void
once(void)
{
	vm_statistics64_data_t v;
	unsigned long long f[NFIELDS];
	int i;

	get(&v);
	fields(&v, f);
	banner();
	for (i = 0; i < NFIELDS; i++)
		printf("%-30s%16llu.\n", names[i], f[i]);
}

static void
header(void)
{
	int i;

	for (i = 0; i < NFIELDS; i++)
		printf("%*s", (int)(strlen(heads[i]) > 8 ? strlen(heads[i]) + 1 : 9),
		    heads[i]);
	printf("\n");
}

static void
row(const unsigned long long f[NFIELDS], const unsigned long long prev[NFIELDS],
    int first)
{
	char buf[32];
	unsigned long long n;
	int i, w;

	for (i = 0; i < NFIELDS; i++) {
		n = (first || gauge[i]) ? f[i] : f[i] - prev[i];
		w = strlen(heads[i]) > 8 ? (int)strlen(heads[i]) + 1 : 9;
		/* Big numbers shortened, as K, M and G. */
		if (n >= 100000000000ULL)
			snprintf(buf, sizeof(buf), "%lluG", n / 1000000000ULL);
		else if (n >= 100000000ULL)
			snprintf(buf, sizeof(buf), "%lluM", n / 1000000ULL);
		else if (n >= 100000ULL && w <= 9)
			snprintf(buf, sizeof(buf), "%lluK", n / 1000ULL);
		else
			snprintf(buf, sizeof(buf), "%llu", n);
		printf("%*s", w, buf);
	}
	printf("\n");
}

int
main(int argc, char *argv[])
{
	vm_statistics64_data_t v;
	unsigned long long f[NFIELDS], prev[NFIELDS];
	long count = -1, interval, lines;
	char *end;
	int ch;

	if (host_page_size(mach_host_self(), &pagesize) != KERN_SUCCESS)
		pagesize = (vm_size_t)getpagesize();
	while ((ch = getopt(argc, argv, "c:")) != -1)
		switch (ch) {
		case 'c':
			count = strtol(optarg, &end, 10);
			if (*optarg == '\0' || *end != '\0' || count <= 0)
				usage();
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc == 0) {
		if (count != -1)
			usage();
		once();
		return (0);
	}
	if (argc != 1)
		usage();
	interval = strtol(argv[0], &end, 10);
	if (*argv[0] == '\0' || *end != '\0' || interval <= 0 ||
	    interval > INT_MAX)
		usage();

	banner();
	for (lines = 0;; lines++) {
		if (lines % 20 == 0)
			header();
		get(&v);
		fields(&v, f);
		row(f, prev, lines == 0);
		memcpy(prev, f, sizeof(prev));
		fflush(stdout);
		if (count != -1 && lines + 1 >= count)
			break;
		sleep((unsigned)interval);
	}
	return (0);
}
