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

/* LinuxKPI's backend in the kernel: IOKit's allocator, locks and delays. */

#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <kern/clock.h>
#include "lkpi_os.h"

extern "C" {

/* IOFree needs the size: it is kept in front of each block. */
struct lkpi_block {
	size_t size;
	size_t pad;	/* 16-byte alignment for what follows */
};

void *
lkpi_alloc(size_t size, int zero)
{
	size_t total = size + sizeof(struct lkpi_block);
	if (total < size)
		return NULL;
	struct lkpi_block *b = (struct lkpi_block *)IOMalloc(total);
	if (b == NULL)
		return NULL;
	b->size = total;
	if (zero)
		bzero(b + 1, size);
	return b + 1;
}

void
lkpi_free(void *p)
{
	if (p == NULL)
		return;
	struct lkpi_block *b = (struct lkpi_block *)p - 1;
	IOFree(b, b->size);
}

struct lkpi_lock *
lkpi_lock_alloc(void)
{
	return (struct lkpi_lock *)IOLockAlloc();
}

void
lkpi_lock_free(struct lkpi_lock *l)
{
	IOLockFree((IOLock *)l);
}

void
lkpi_lock_lock(struct lkpi_lock *l)
{
	IOLockLock((IOLock *)l);
}

void
lkpi_lock_unlock(struct lkpi_lock *l)
{
	IOLockUnlock((IOLock *)l);
}

void
lkpi_udelay(unsigned long us)
{
	IODelay((unsigned int)us);
}

void
lkpi_msleep(unsigned int ms)
{
	IOSleep(ms);
}

unsigned long
lkpi_jiffies(void)
{
	uint64_t ns;
	absolutetime_to_nanoseconds(mach_absolute_time(), &ns);
	return (unsigned long)(ns / (1000000000ULL / LKPI_HZ));
}

int lkpi_log_level = 6;	/* debug messages are dropped */

void
lkpi_vlog(int level, const char *fmt, va_list ap)
{
	if (level <= lkpi_log_level)
		IOLogv(fmt, ap);
}

void
lkpi_log(int level, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	lkpi_vlog(level, fmt, ap);
	va_end(ap);
}

}
