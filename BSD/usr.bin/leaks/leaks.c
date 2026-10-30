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
 * leaks: the malloc blocks a process can no longer reach.
 *
 * The process is stopped; libmalloc's own enumerators list its blocks
 * (each zone's introspection, read from the process); then, from the
 * roots (its writable memory outside malloc: globals and stacks, and its
 * threads' registers), every word that points into a block marks it,
 * and the blocks it reaches in turn. What is left unmarked leaked: no
 * pointer to it is left.
 *
 * libmalloc is not at the same address in every process here (no shared
 * cache): the process's image list gives its libmalloc's address, and
 * the zone list and the enumerators are translated by the difference.
 *
 *	leaks [-nocontext] [-quiet] pid | name
 */

#include <sys/types.h>
#include <sys/sysctl.h>

#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <malloc/malloc.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct block {
	mach_vm_address_t	addr;
	mach_vm_size_t		size;
	unsigned		zone;	/* index in zones[] */
	bool			reached;
};

struct range {
	mach_vm_address_t	lo, hi;
};

static task_t target = MACH_PORT_NULL;
static bool suspended;

/* Our libmalloc and the target's: an address in ours is translated. */
static mach_vm_address_t our_lo, our_hi, their_base;

static struct block *blocks;
static size_t nblocks, ablocks;
static struct range *skip;	/* malloc's own regions: not roots */
static size_t nskip, askip;
static unsigned cur_zone;

static char zone_names[64][96];
static unsigned nzones;
/* The zones' own pointers (their names), which live outside malloc. */
static mach_vm_address_t zone_refs[64];
static unsigned nzone_refs;

/* Reads go to buffers freed at the end. */
static void **reads;
static size_t nreads, areads;

static void
cleanup(void)
{
	if (suspended && target != MACH_PORT_NULL) {
		task_resume(target);
		suspended = false;
	}
}

static void
on_signal(int sig)
{
	cleanup();
	signal(sig, SIG_DFL);
	raise(sig);
}

static void *
xrealloc(void *p, size_t n)
{
	if ((p = realloc(p, n)) == NULL)
		err(2, NULL);
	return (p);
}

static bool
read_target(mach_vm_address_t addr, void *buf, mach_vm_size_t size)
{
	mach_vm_size_t got = 0;

	return (mach_vm_read_overwrite(target, addr, size,
	    (mach_vm_address_t)(uintptr_t)buf, &got) == KERN_SUCCESS &&
	    got == size);
}

/* libmalloc's memory reader: a copy of the target's memory. */
static kern_return_t
reader(task_t task, vm_address_t remote, vm_size_t size, void **local)
{
	void *buf;

	(void)task;
	if (remote >= our_lo && remote < our_hi)
		remote = remote - our_lo + their_base;
	if ((buf = malloc(size ? size : 1)) == NULL)
		return (KERN_RESOURCE_SHORTAGE);
	if (!read_target(remote, buf, size)) {
		free(buf);
		return (KERN_INVALID_ADDRESS);
	}
	if (nreads == areads) {
		areads = areads ? areads * 2 : 256;
		reads = xrealloc(reads, areads * sizeof(*reads));
	}
	reads[nreads++] = buf;
	*local = buf;
	return (KERN_SUCCESS);
}

static void
free_reads(void)
{
	while (nreads > 0)
		free(reads[--nreads]);
}

static void
recorder(task_t task, void *context, unsigned type, vm_range_t *r,
    unsigned count)
{
	unsigned i;

	(void)task;
	(void)context;
	for (i = 0; i < count; i++) {
		if (type & MALLOC_PTR_IN_USE_RANGE_TYPE) {
			if (nblocks == ablocks) {
				ablocks = ablocks ? ablocks * 2 : 1024;
				blocks = xrealloc(blocks, ablocks * sizeof(*blocks));
			}
			blocks[nblocks].addr = r[i].address;
			blocks[nblocks].size = r[i].size;
			blocks[nblocks].zone = cur_zone;
			blocks[nblocks].reached = false;
			nblocks++;
		} else {
			if (nskip == askip) {
				askip = askip ? askip * 2 : 256;
				skip = xrealloc(skip, askip * sizeof(*skip));
			}
			skip[nskip].lo = r[i].address;
			skip[nskip].hi = r[i].address + r[i].size;
			nskip++;
		}
	}
}

/* The extent of the image a mach header starts. */
static mach_vm_size_t
image_size(const struct mach_header_64 *mh)
{
	const struct load_command *lc = (const void *)(mh + 1);
	uint64_t lo = UINT64_MAX, hi = 0;
	uint32_t i;

	for (i = 0; i < mh->ncmds; i++) {
		if (lc->cmd == LC_SEGMENT_64) {
			const struct segment_command_64 *sc = (const void *)lc;

			if (sc->vmsize != 0 && strcmp(sc->segname, "__PAGEZERO") != 0) {
				if (sc->vmaddr < lo)
					lo = sc->vmaddr;
				if (sc->vmaddr + sc->vmsize > hi)
					hi = sc->vmaddr + sc->vmsize;
			}
		}
		lc = (const void *)((const char *)lc + lc->cmdsize);
	}
	return (hi > lo ? hi - lo : 0);
}

/* Where the target's libmalloc is, from its image list. */
static bool
find_their_libmalloc(const char *path)
{
	struct task_dyld_info di;
	mach_msg_type_number_t n = TASK_DYLD_INFO_COUNT;
	struct dyld_all_image_infos all;
	struct dyld_image_info *infos;
	uint32_t i;
	bool found = false;

	if (task_info(target, TASK_DYLD_INFO, (task_info_t)&di, &n) !=
	    KERN_SUCCESS || !read_target(di.all_image_info_addr, &all,
	    sizeof(all)) || all.infoArray == NULL || all.infoArrayCount == 0)
		return (false);
	infos = calloc(all.infoArrayCount, sizeof(*infos));
	if (infos == NULL || !read_target((mach_vm_address_t)(uintptr_t)
	    all.infoArray, infos, all.infoArrayCount * sizeof(*infos))) {
		free(infos);
		return (false);
	}
	for (i = 0; i < all.infoArrayCount && !found; i++) {
		char p[1024];

		memset(p, 0, sizeof(p));
		/* A path may end before the buffer does: read what can be. */
		if (!read_target((mach_vm_address_t)(uintptr_t)
		    infos[i].imageFilePath, p, sizeof(p) - 1) &&
		    !read_target((mach_vm_address_t)(uintptr_t)
		    infos[i].imageFilePath, p, 256))
			continue;
		if (strcmp(p, path) == 0) {
			their_base = (mach_vm_address_t)(uintptr_t)
			    infos[i].imageLoadAddress;
			found = true;
		}
	}
	free(infos);
	return (found);
}

static int
cmp_blocks(const void *a, const void *b)
{
	const struct block *x = a, *y = b;

	return (x->addr < y->addr ? -1 : x->addr > y->addr);
}

static int
cmp_ranges(const void *a, const void *b)
{
	const struct range *x = a, *y = b;

	return (x->lo < y->lo ? -1 : x->lo > y->lo);
}

/* The block a value points into, if any. */
static struct block *
find_block(mach_vm_address_t v)
{
	size_t lo = 0, hi = nblocks;

	while (lo < hi) {
		size_t mid = (lo + hi) / 2;

		if (v < blocks[mid].addr)
			hi = mid;
		else if (v >= blocks[mid].addr + blocks[mid].size)
			lo = mid + 1;
		else
			return (&blocks[mid]);
	}
	return (NULL);
}

static bool
skipped(mach_vm_address_t a)
{
	size_t lo = 0, hi = nskip;

	while (lo < hi) {
		size_t mid = (lo + hi) / 2;

		if (a < skip[mid].lo)
			hi = mid;
		else if (a >= skip[mid].hi)
			lo = mid + 1;
		else
			return (true);
	}
	return (false);
}

static struct block **work;
static size_t nwork, awork;

static void
reach(mach_vm_address_t v)
{
	struct block *b = find_block(v);

	if (b == NULL || b->reached)
		return;
	b->reached = true;
	if (nwork == awork) {
		awork = awork ? awork * 2 : 1024;
		work = xrealloc(work, awork * sizeof(*work));
	}
	work[nwork++] = b;
}

/* Every pointer-sized word of [lo, hi) in the target, as a reference. */
static void
scan(mach_vm_address_t lo, mach_vm_address_t hi, bool roots)
{
	static uint64_t buf[65536];
	mach_vm_address_t a;

	lo = (lo + 7) & ~(mach_vm_address_t)7;
	for (a = lo; a < hi; ) {
		mach_vm_size_t n = hi - a;
		size_t i;

		if (n > sizeof(buf))
			n = sizeof(buf);
		if (!read_target(a, buf, n)) {
			/* An unreadable page: go on with the next one. */
			mach_vm_address_t next = (a + vm_page_size) &
			    ~((mach_vm_address_t)vm_page_size - 1);

			n = (next > a ? next : a + vm_page_size) - a;
			if (n > hi - a)
				n = hi - a;
			if (n > sizeof(buf) || !read_target(a, buf, n)) {
				a += n;
				continue;
			}
		}
		for (i = 0; i < n / 8; i++) {
			/* Roots: not malloc's regions, nor a block's memory. */
			if (roots && (skipped(a + i * 8) ||
			    find_block(a + i * 8) != NULL))
				continue;
			reach(buf[i]);
		}
		a += n;
	}
}

/*
 * The roots: the threads' registers, and writable memory outside
 * malloc's; a stack only from its thread's stack pointer up (below it
 * are dead frames, which still hold old pointers).
 */
static void
scan_roots(void)
{
	mach_vm_address_t addr = 0, sps[256];
	thread_act_array_t threads;
	mach_msg_type_number_t nthreads = 0, i;
	unsigned nsps = 0, k;

	if (task_threads(target, &threads, &nthreads) == KERN_SUCCESS) {
		for (i = 0; i < nthreads; i++) {
			x86_thread_state64_t st;
			mach_msg_type_number_t n = x86_THREAD_STATE64_COUNT;

			if (thread_get_state(threads[i], x86_THREAD_STATE64,
			    (thread_state_t)&st, &n) == KERN_SUCCESS) {
				uint64_t *r = (uint64_t *)&st;
				size_t j;

				for (j = 0; j < sizeof(st) / sizeof(uint64_t); j++)
					reach(r[j]);
				if (nsps < 256)
					sps[nsps++] = st.__rsp;
			}
			mach_port_deallocate(mach_task_self(), threads[i]);
		}
		vm_deallocate(mach_task_self(), (vm_address_t)threads,
		    nthreads * sizeof(*threads));
	}
	for (k = 0; k < nzone_refs; k++)
		reach(zone_refs[k]);

	/*
	 * The map, first: a stack can come back as several regions (the
	 * map splits its entries), and all of a stack's regions below its
	 * thread's stack pointer are dead frames.
	 */
	struct region {
		mach_vm_address_t	lo, hi;
		bool			rw, stack;
	} *rg = NULL;
	size_t nrg = 0, arg = 0, r;

	for (;;) {
		mach_vm_size_t size;
		natural_t depth = 1;
		vm_region_submap_info_data_64_t info;
		mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;

		if (mach_vm_region_recurse(target, &addr, &size, &depth,
		    (vm_region_recurse_info_t)&info, &count) != KERN_SUCCESS)
			break;
		if (!info.is_submap) {
			if (nrg == arg) {
				arg = arg ? arg * 2 : 256;
				rg = xrealloc(rg, arg * sizeof(*rg));
			}
			rg[nrg].lo = addr;
			rg[nrg].hi = addr + size;
			rg[nrg].rw = (info.protection & (VM_PROT_READ |
			    VM_PROT_WRITE)) == (VM_PROT_READ | VM_PROT_WRITE);
			rg[nrg].stack = info.user_tag == VM_MEMORY_STACK;
			nrg++;
		}
		addr += size;
	}
	for (r = 0; r < nrg; r++) {
		mach_vm_address_t lo = rg[r].lo;

		if (!rg[r].rw)
			continue;
		if (rg[r].stack) {
			/* The stack this region is part of: contiguous ones. */
			size_t first = r, last = r;

			while (first > 0 && rg[first - 1].stack &&
			    rg[first - 1].hi == rg[first].lo)
				first--;
			while (last + 1 < nrg && rg[last + 1].stack &&
			    rg[last + 1].lo == rg[last].hi)
				last++;
			for (k = 0; k < nsps; k++) {
				/* x86-64's red zone: 128 bytes under the pointer. */
				mach_vm_address_t live = sps[k] - 128;

				if (sps[k] >= rg[first].lo && sps[k] < rg[last].hi &&
				    live > lo)
					lo = live;
			}
			if (lo >= rg[r].hi)
				continue;	/* all of it dead frames */
		}
		scan(lo, rg[r].hi, true);
	}
	free(rg);
}

/* From the reached blocks, the blocks they point to, and so on. */
static void
propagate(void)
{
	while (nwork > 0) {
		struct block *b = work[--nwork];

		scan(b->addr, b->addr + b->size, false);
	}
}

static void
enumerate_zones(void)
{
	Dl_info dl;
	vm_address_t *zones;
	unsigned count, i;

	if (!dladdr((void *)malloc, &dl) || dl.dli_fbase == NULL)
		errx(2, "cannot find libmalloc");
	our_lo = (mach_vm_address_t)(uintptr_t)dl.dli_fbase;
	our_hi = our_lo + image_size(dl.dli_fbase);
	if (!find_their_libmalloc(dl.dli_fname))
		errx(2, "the process has no %s", dl.dli_fname);
	if (malloc_get_all_zones(target, reader, &zones, &count) != KERN_SUCCESS)
		errx(2, "cannot read the process's malloc zones");
	for (i = 0; i < count && nzones < 64; i++) {
		malloc_zone_t z;
		malloc_introspection_t in;
		char name[80] = "";
		vm_address_t za = zones[i];
		kern_return_t (*enumerator)(task_t, void *, unsigned,
		    vm_address_t, memory_reader_t, vm_range_recorder_t);

		if (!read_target(za, &z, sizeof(z)) || z.introspect == NULL ||
		    !read_target((mach_vm_address_t)(uintptr_t)z.introspect,
		    &in, sizeof(in)) || in.enumerator == NULL)
			continue;
		if (z.zone_name != NULL) {
			read_target((mach_vm_address_t)(uintptr_t)z.zone_name,
			    name, sizeof(name) - 1);
			zone_refs[nzone_refs++] =
			    (mach_vm_address_t)(uintptr_t)z.zone_name;
		}
		snprintf(zone_names[nzones], sizeof(zone_names[nzones]),
		    "%s_0x%llx", name[0] ? name : "Zone",
		    (unsigned long long)za);
		/* Their enumerator is ours, by the difference. */
		enumerator = (void *)((uintptr_t)in.enumerator -
		    (uintptr_t)their_base + (uintptr_t)our_lo);
		cur_zone = nzones++;
		enumerator(target, NULL, MALLOC_PTR_IN_USE_RANGE_TYPE |
		    MALLOC_PTR_REGION_RANGE_TYPE | MALLOC_ADMIN_REGION_RANGE_TYPE,
		    za, reader, recorder);
		free_reads();
	}
}

static void
print_context(const struct block *b)
{
	unsigned char bytes[32];
	mach_vm_size_t n = b->size < sizeof(bytes) ? b->size : sizeof(bytes);
	size_t i;

	if (!read_target(b->addr, bytes, n))
		return;
	printf("\t");
	for (i = 0; i + 4 <= n; i += 4)
		printf("0x%02x%02x%02x%02x ", bytes[i + 3], bytes[i + 2],
		    bytes[i + 1], bytes[i]);
	printf("\t");
	for (i = 0; i < n; i++)
		putchar(bytes[i] >= 0x20 && bytes[i] < 0x7f ? bytes[i] : '.');
	printf("\n");
}

static pid_t
find_pid(const char *arg)
{
	char *end;
	long v = strtol(arg, &end, 10);
	pid_t pids[4096], found = -1;
	int n, i, matches = 0;

	if (*arg != '\0' && *end == '\0')
		return ((pid_t)v);
	n = proc_listpids(PROC_ALL_PIDS, 0, pids, sizeof(pids));
	for (i = 0; i < n / (int)sizeof(pid_t); i++) {
		char name[2 * MAXCOMLEN + 1];

		if (pids[i] <= 0 || pids[i] == getpid() ||
		    proc_name(pids[i], name, sizeof(name)) <= 0)
			continue;
		if (strstr(name, arg) != NULL) {
			found = pids[i];
			matches++;
		}
	}
	if (matches > 1)
		errx(2, "more than one process is named %s: give its pid", arg);
	if (found < 0)
		errx(2, "no process is named %s", arg);
	return (found);
}

static void
usage(void)
{
	fprintf(stderr, "usage: leaks [-nocontext] [-quiet] pid | name\n");
	exit(2);
}

int
main(int argc, char **argv)
{
	bool context = true, quiet = false;
	char name[2 * MAXCOMLEN + 1] = "", path[PROC_PIDPATHINFO_MAXSIZE] = "";
	unsigned long long total = 0, leaked = 0;
	size_t i, nleaks = 0;
	pid_t pid;
	kern_return_t kr;

	for (argc--, argv++; argc > 0 && argv[0][0] == '-'; argc--, argv++) {
		if (strcmp(argv[0], "-nocontext") == 0)
			context = false;
		else if (strcmp(argv[0], "-quiet") == 0 ||
		    strcmp(argv[0], "-q") == 0)
			quiet = true;
		else
			usage();
	}
	if (argc != 1)
		usage();
	pid = find_pid(argv[0]);
	if (pid == getpid())
		errx(2, "leaks cannot examine itself");
	proc_name(pid, name, sizeof(name));
	proc_pidpath(pid, path, sizeof(path));
	if ((kr = task_for_pid(mach_task_self(), pid, &target)) != KERN_SUCCESS) {
		const char *why = kr == KERN_FAILURE ?
		    "not permitted (run as root)" : mach_error_string(kr);

		/* Another user's process may not even tell its name. */
		if (name[0] != '\0')
			errx(2, "cannot examine process %d (%s): %s", pid, name, why);
		errx(2, "cannot examine process %d: %s", pid, why);
	}

	/* The process goes on whatever happens to leaks. */
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	signal(SIGHUP, on_signal);
	signal(SIGSEGV, on_signal);
	signal(SIGBUS, on_signal);
	signal(SIGABRT, on_signal);
	if (task_suspend(target) == KERN_SUCCESS)
		suspended = true;

	enumerate_zones();
	qsort(blocks, nblocks, sizeof(*blocks), cmp_blocks);
	qsort(skip, nskip, sizeof(*skip), cmp_ranges);
	scan_roots();
	propagate();

	for (i = 0; i < nblocks; i++) {
		total += blocks[i].size;
		if (!blocks[i].reached) {
			nleaks++;
			leaked += blocks[i].size;
		}
	}
	if (!quiet) {
		printf("Process:         %s [%d]\n", name, pid);
		printf("Path:            %s\n", path);
		printf("\n----\n\n");
	}
	printf("Process %d: %zu nodes malloced for %llu KB\n", pid, nblocks,
	    (total + 1023) / 1024);
	printf("Process %d: %zu leak%s for %llu total leaked bytes.\n", pid,
	    nleaks, nleaks == 1 ? "" : "s", leaked);
	if (!quiet) {
		if (nleaks > 0)
			printf("\n");
		for (i = 0; i < nblocks; i++) {
			if (blocks[i].reached)
				continue;
			printf("Leak: 0x%llx  size=%llu  zone: %s\n",
			    (unsigned long long)blocks[i].addr,
			    (unsigned long long)blocks[i].size,
			    zone_names[blocks[i].zone]);
			if (context)
				print_context(&blocks[i]);
		}
	}
	cleanup();
	return (nleaks > 0 ? 1 : 0);
}
