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
 */

/*
 * sample: ours. Where a running process spends its time: its threads'
 * stacks taken again and again for a while (each thread stopped, its
 * frame pointers followed), symbolized from the Mach-O symbol tables of
 * the images dyld loaded, and shown as a call tree per thread with how
 * often each frame was seen, then the frames most often at the top.
 */

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <err.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_FRAMES	128

struct symbol {
	mach_vm_address_t	addr;
	const char		*name;
};

struct image {
	mach_vm_address_t	base, end;	/* where it is in the process */
	char			path[1024];
	const char		*name;		/* the last path component */
	bool			loaded;		/* symbols read */
	struct symbol		*symbols;
	size_t			nsymbols;
	char			*strings;
};

struct node {
	mach_vm_address_t	pc;
	unsigned		count;
	struct node		*child, *next;
};

struct thread_samples {
	uint64_t		id;
	char			name[64];
	struct node		root;
	bool			seen;
};

static task_t target = MACH_PORT_NULL;
static bool suspended;
static struct image *images;
static size_t nimages;
static struct thread_samples *threads;
static size_t nthreads, athreads;
static struct { mach_vm_address_t pc; unsigned count; } *tops;
static size_t ntops, atops;

static void
resume(void)
{
	if (suspended && target != MACH_PORT_NULL)
		task_resume(target);
	suspended = false;
}

static void
on_signal(int sig)
{
	resume();
	signal(sig, SIG_DFL);
	raise(sig);
}

static void *
xcalloc(size_t n, size_t size)
{
	void *p = calloc(n ? n : 1, size ? size : 1);

	if (p == NULL)
		err(1, "calloc");
	return (p);
}

static void *
xrealloc(void *p, size_t n)
{
	if ((p = realloc(p, n ? n : 1)) == NULL)
		err(1, "realloc");
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

/* The images dyld loaded, with where each one's __TEXT begins and ends. */
static void
read_images(void)
{
	struct task_dyld_info di;
	mach_msg_type_number_t n = TASK_DYLD_INFO_COUNT;
	struct dyld_all_image_infos all;
	struct dyld_image_info *infos;
	uint32_t i;

	if (task_info(target, TASK_DYLD_INFO, (task_info_t)&di, &n) !=
	    KERN_SUCCESS || !read_target(di.all_image_info_addr, &all,
	    sizeof(all)) || all.infoArray == NULL || all.infoArrayCount == 0)
		return;
	infos = xcalloc(all.infoArrayCount, sizeof(*infos));
	if (!read_target((mach_vm_address_t)(uintptr_t)all.infoArray, infos,
	    all.infoArrayCount * sizeof(*infos))) {
		free(infos);
		return;
	}
	images = xcalloc(all.infoArrayCount, sizeof(*images));
	for (i = 0; i < all.infoArrayCount; i++) {
		struct image *im = &images[nimages];
		struct mach_header_64 mh;
		char *cmds, *p;
		uint32_t c;

		im->base = (mach_vm_address_t)(uintptr_t)infos[i].imageLoadAddress;
		if (!read_target(im->base, &mh, sizeof(mh)) ||
		    mh.magic != MH_MAGIC_64)
			continue;
		cmds = xcalloc(1, mh.sizeofcmds);
		if (!read_target(im->base + sizeof(mh), cmds, mh.sizeofcmds)) {
			free(cmds);
			continue;
		}
		im->end = im->base;
		for (c = 0, p = cmds; c < mh.ncmds &&
		    p + sizeof(struct load_command) <= cmds + mh.sizeofcmds; c++) {
			struct load_command *lc = (struct load_command *)p;

			if (lc->cmdsize == 0)
				break;
			if (lc->cmd == LC_SEGMENT_64) {
				struct segment_command_64 *sc = (void *)p;

				if (strcmp(sc->segname, "__TEXT") == 0)
					im->end = im->base + sc->vmsize;
			}
			p += lc->cmdsize;
		}
		free(cmds);
		memset(im->path, 0, sizeof(im->path));
		if (!read_target((mach_vm_address_t)(uintptr_t)
		    infos[i].imageFilePath, im->path, sizeof(im->path) - 1))
			read_target((mach_vm_address_t)(uintptr_t)
			    infos[i].imageFilePath, im->path, 256);
		im->name = strrchr(im->path, '/') ? strrchr(im->path, '/') + 1 :
		    im->path;
		nimages++;
	}
	free(infos);
}

static int
cmp_symbols(const void *a, const void *b)
{
	const struct symbol *x = a, *y = b;

	return (x->addr < y->addr ? -1 : x->addr > y->addr);
}

/* An image's defined symbols, read from its __LINKEDIT in the process. */
static void
load_symbols(struct image *im)
{
	struct mach_header_64 mh;
	struct symtab_command st = { 0 };
	mach_vm_address_t text_vm = 0, link_vm = 0, link_off = 0, slide;
	struct nlist_64 *nl;
	char *cmds, *p;
	uint32_t c, i;
	bool have_text = false;

	im->loaded = true;
	if (!read_target(im->base, &mh, sizeof(mh)))
		return;
	cmds = xcalloc(1, mh.sizeofcmds);
	if (!read_target(im->base + sizeof(mh), cmds, mh.sizeofcmds)) {
		free(cmds);
		return;
	}
	for (c = 0, p = cmds; c < mh.ncmds &&
	    p + sizeof(struct load_command) <= cmds + mh.sizeofcmds; c++) {
		struct load_command *lc = (struct load_command *)p;

		if (lc->cmdsize == 0)
			break;
		if (lc->cmd == LC_SEGMENT_64) {
			struct segment_command_64 *sc = (void *)p;

			if (strcmp(sc->segname, "__TEXT") == 0) {
				text_vm = sc->vmaddr;
				have_text = true;
			} else if (strcmp(sc->segname, "__LINKEDIT") == 0) {
				link_vm = sc->vmaddr;
				link_off = sc->fileoff;
			}
		} else if (lc->cmd == LC_SYMTAB)
			memcpy(&st, p, sizeof(st));
		p += lc->cmdsize;
	}
	free(cmds);
	if (!have_text || st.nsyms == 0 || link_vm == 0)
		return;
	slide = im->base - text_vm;
	nl = xcalloc(st.nsyms, sizeof(*nl));
	im->strings = xcalloc(1, st.strsize + 1);
	if (!read_target(link_vm + slide + (st.symoff - link_off), nl,
	    (mach_vm_size_t)st.nsyms * sizeof(*nl)) ||
	    !read_target(link_vm + slide + (st.stroff - link_off), im->strings,
	    st.strsize)) {
		free(nl);
		free(im->strings);
		im->strings = NULL;
		return;
	}
	im->symbols = xcalloc(st.nsyms, sizeof(*im->symbols));
	for (i = 0; i < st.nsyms; i++) {
		if ((nl[i].n_type & N_STAB) != 0 ||
		    (nl[i].n_type & N_TYPE) != N_SECT ||
		    nl[i].n_un.n_strx == 0 || nl[i].n_un.n_strx >= st.strsize)
			continue;
		im->symbols[im->nsymbols].addr = nl[i].n_value + slide;
		im->symbols[im->nsymbols].name = im->strings + nl[i].n_un.n_strx;
		im->nsymbols++;
	}
	free(nl);
	qsort(im->symbols, im->nsymbols, sizeof(*im->symbols), cmp_symbols);
}

static struct image *
image_at(mach_vm_address_t pc)
{
	size_t i;

	for (i = 0; i < nimages; i++)
		if (pc >= images[i].base && pc < images[i].end)
			return (&images[i]);
	return (NULL);
}

/* "name  (in image) + offset", or the image and its offset, or "???". */
static void
describe(mach_vm_address_t pc, char *buf, size_t size)
{
	struct image *im = image_at(pc);
	size_t lo, hi;

	if (im == NULL) {
		snprintf(buf, size, "???  [0x%llx]", (unsigned long long)pc);
		return;
	}
	if (!im->loaded)
		load_symbols(im);
	lo = 0;
	hi = im->nsymbols;
	while (lo < hi) {
		size_t mid = (lo + hi) / 2;

		if (im->symbols[mid].addr <= pc)
			lo = mid + 1;
		else
			hi = mid;
	}
	if (lo > 0) {
		const char *name = im->symbols[lo - 1].name;

		/* C symbols carry a leading underscore. */
		if (name[0] == '_')
			name++;
		snprintf(buf, size, "%s  (in %s) + %llu  [0x%llx]", name,
		    im->name, (unsigned long long)(pc - im->symbols[lo - 1].addr),
		    (unsigned long long)pc);
	} else
		snprintf(buf, size, "???  (in %s) + %llu  [0x%llx]", im->name,
		    (unsigned long long)(pc - im->base), (unsigned long long)pc);
}

static struct thread_samples *
thread_named(uint64_t id)
{
	size_t i;

	for (i = 0; i < nthreads; i++)
		if (threads[i].id == id)
			return (&threads[i]);
	if (nthreads == athreads) {
		athreads = athreads ? athreads * 2 : 16;
		threads = xrealloc(threads, athreads * sizeof(*threads));
	}
	memset(&threads[nthreads], 0, sizeof(*threads));
	threads[nthreads].id = id;
	return (&threads[nthreads++]);
}

static void
note_top(mach_vm_address_t pc)
{
	size_t i;

	for (i = 0; i < ntops; i++)
		if (tops[i].pc == pc) {
			tops[i].count++;
			return;
		}
	if (ntops == atops) {
		atops = atops ? atops * 2 : 64;
		tops = xrealloc(tops, atops * sizeof(*tops));
	}
	tops[ntops].pc = pc;
	tops[ntops++].count = 1;
}

/* One stack, outermost frame first, into the thread's tree. */
static void
add_stack(struct thread_samples *t, mach_vm_address_t *pcs, int n)
{
	struct node *parent = &t->root;
	int i;

	parent->count++;
	for (i = n - 1; i >= 0; i--) {
		struct node *c;

		for (c = parent->child; c != NULL; c = c->next)
			if (c->pc == pcs[i])
				break;
		if (c == NULL) {
			c = xcalloc(1, sizeof(*c));
			c->pc = pcs[i];
			c->next = parent->child;
			parent->child = c;
		}
		c->count++;
		parent = c;
	}
	if (n > 0)
		note_top(pcs[0]);
}

static void
sample_once(void)
{
	thread_act_array_t list;
	mach_msg_type_number_t count, i;

	if (task_suspend(target) != KERN_SUCCESS)
		return;
	suspended = true;
	if (task_threads(target, &list, &count) != KERN_SUCCESS) {
		resume();
		return;
	}
	for (i = 0; i < count; i++) {
		x86_thread_state64_t state;
		mach_msg_type_number_t sc = x86_THREAD_STATE64_COUNT;
		thread_identifier_info_data_t ident;
		mach_msg_type_number_t ic = THREAD_IDENTIFIER_INFO_COUNT;
		struct thread_samples *t;
		mach_vm_address_t pcs[MAX_FRAMES], fp;
		int n = 0;

		if (thread_get_state(list[i], x86_THREAD_STATE64,
		    (thread_state_t)&state, &sc) != KERN_SUCCESS)
			goto next;
		if (thread_info(list[i], THREAD_IDENTIFIER_INFO,
		    (thread_info_t)&ident, &ic) != KERN_SUCCESS)
			ident.thread_id = list[i];
		t = thread_named(ident.thread_id);
		if (!t->seen) {
			struct proc_threadinfo pti;
			int pid;

			t->seen = true;
			if (pid_for_task(target, &pid) == KERN_SUCCESS &&
			    proc_pidinfo(pid, PROC_PIDTHREADID64INFO,
			    ident.thread_handle, &pti, sizeof(pti)) ==
			    (int)sizeof(pti))
				strlcpy(t->name, pti.pth_name, sizeof(t->name));
		}
		pcs[n++] = state.__rip;
		/* The caller's frame: the return address over the saved frame
		   pointer, while the chain goes up the stack. */
		for (fp = state.__rbp; n < MAX_FRAMES && fp != 0 && (fp & 7) == 0;) {
			mach_vm_address_t frame[2];

			/* The outermost frame's return address is no code. */
			if (!read_target(fp, frame, sizeof(frame)) ||
			    frame[1] < 0x1000)
				break;
			pcs[n++] = frame[1];
			if (frame[0] <= fp)
				break;
			fp = frame[0];
		}
		add_stack(t, pcs, n);
next:
		mach_port_deallocate(mach_task_self(), list[i]);
	}
	vm_deallocate(mach_task_self(), (vm_address_t)list,
	    count * sizeof(*list));
	resume();
}

static void
print_tree(FILE *out, struct node *n, int depth)
{
	struct node **kids, *c;
	size_t nkids = 0, i, j;
	char buf[1200];

	for (c = n->child; c != NULL; c = c->next)
		nkids++;
	kids = xcalloc(nkids, sizeof(*kids));
	for (i = 0, c = n->child; c != NULL; c = c->next)
		kids[i++] = c;
	/* The most seen first. */
	for (i = 1; i < nkids; i++)
		for (j = i; j > 0 && kids[j]->count > kids[j - 1]->count; j--) {
			c = kids[j];
			kids[j] = kids[j - 1];
			kids[j - 1] = c;
		}
	for (i = 0; i < nkids; i++) {
		describe(kids[i]->pc, buf, sizeof(buf));
		fprintf(out, "    %*s+ %u %s\n", depth * 2, "", kids[i]->count, buf);
		print_tree(out, kids[i], depth + 1);
	}
	free(kids);
}

static int
cmp_tops(const void *a, const void *b)
{
	const unsigned *x = (const unsigned *)((const char *)a +
	    sizeof(mach_vm_address_t));
	const unsigned *y = (const unsigned *)((const char *)b +
	    sizeof(mach_vm_address_t));

	return (*x > *y ? -1 : *x < *y);
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
	fprintf(stderr, "usage: sample pid | name [duration [interval]] "
	    "[-file path] [-mayDie]\n"
	    "       duration in seconds (10), interval in milliseconds (1)\n");
	exit(2);
}

int
main(int argc, char **argv)
{
	char name[2 * MAXCOMLEN + 1] = "", path[PROC_PIDPATHINFO_MAXSIZE] = "";
	const char *file = NULL, *what = NULL;
	double duration = 10, interval_ms = 1;
	int positional = 0, i;
	unsigned samples = 0;
	bool may_die = false;
	struct timeval start, now;
	FILE *out = stdout;
	pid_t pid;
	kern_return_t kr;
	size_t t;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-file") == 0 && i + 1 < argc)
			file = argv[++i];
		else if (strcmp(argv[i], "-mayDie") == 0)
			may_die = true;
		else if (argv[i][0] == '-')
			usage();
		else if (positional == 0) {
			what = argv[i];
			positional++;
		} else if (positional == 1) {
			duration = atof(argv[i]);
			positional++;
		} else if (positional == 2) {
			interval_ms = atof(argv[i]);
			positional++;
		} else
			usage();
	}
	if (what == NULL || duration <= 0 || interval_ms <= 0)
		usage();
	pid = find_pid(what);
	if (pid == getpid())
		errx(2, "sample cannot examine itself");
	proc_name(pid, name, sizeof(name));
	proc_pidpath(pid, path, sizeof(path));
	if ((kr = task_for_pid(mach_task_self(), pid, &target)) != KERN_SUCCESS)
		errx(2, "cannot examine process %d%s%s%s: %s", pid,
		    name[0] ? " (" : "", name, name[0] ? ")" : "",
		    kr == KERN_FAILURE ? "not permitted (run as root)" :
		    mach_error_string(kr));
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	signal(SIGHUP, on_signal);
	signal(SIGSEGV, on_signal);
	signal(SIGBUS, on_signal);

	read_images();
	fprintf(stderr, "Sampling process %d for %g seconds with %g "
	    "millisecond of run time between samples\n", pid, duration,
	    interval_ms);
	gettimeofday(&start, NULL);
	for (;;) {
		gettimeofday(&now, NULL);
		if ((now.tv_sec - start.tv_sec) +
		    (now.tv_usec - start.tv_usec) / 1e6 >= duration)
			break;
		if (kill(pid, 0) != 0) {
			if (!may_die)
				fprintf(stderr, "sample: process %d exited\n", pid);
			break;
		}
		sample_once();
		samples++;
		usleep((useconds_t)(interval_ms * 1000));
	}
	fprintf(stderr, "Sampling completed, processing symbols...\n");

	if (file != NULL && (out = fopen(file, "w")) == NULL)
		err(1, "%s", file);
	fprintf(out, "Analysis of sampling %s (pid %d) every %g millisecond%s\n",
	    name[0] ? name : "?", pid, interval_ms, interval_ms == 1 ? "" : "s");
	fprintf(out, "Process:         %s [%d]\n", name, pid);
	fprintf(out, "Path:            %s\n", path);
	fprintf(out, "Samples:         %u\n\n", samples);
	fprintf(out, "Call graph:\n");
	for (t = 0; t < nthreads; t++) {
		fprintf(out, "    %u Thread_%llu%s%s\n", threads[t].root.count,
		    (unsigned long long)threads[t].id,
		    threads[t].name[0] ? "   " : "", threads[t].name);
		print_tree(out, &threads[t].root, 0);
	}
	fprintf(out, "\nTotal number in stack (recursive counted multiple, "
	    "when >=5):\n\nSort by top of stack, same collapsed (when >= 5):\n");
	qsort(tops, ntops, sizeof(*tops), cmp_tops);
	for (t = 0; t < ntops && tops[t].count >= 5; t++) {
		char buf[1200];

		describe(tops[t].pc, buf, sizeof(buf));
		fprintf(out, "        %-60s %u\n", buf, tops[t].count);
	}
	fprintf(out, "\nBinary Images:\n");
	for (t = 0; t < nimages; t++)
		fprintf(out, "    0x%llx - 0x%llx %s\n",
		    (unsigned long long)images[t].base,
		    (unsigned long long)images[t].end - 1, images[t].path);
	if (out != stdout) {
		fclose(out);
		fprintf(stderr, "Sample analysis of process %d written to file "
		    "%s\n", pid, file);
	}
	return (0);
}
