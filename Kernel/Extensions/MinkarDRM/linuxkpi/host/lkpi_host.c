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

/* LinuxKPI's backend for host unit tests: the C library and POSIX. */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "lkpi_os.h"

int lkpi_host_log_level = 4;	/* tests may raise it */

void *
lkpi_alloc(size_t size, int zero)
{
	return zero ? calloc(1, size ? size : 1) : malloc(size ? size : 1);
}

void
lkpi_free(void *p)
{
	free(p);
}

struct lkpi_lock {
	pthread_mutex_t m;
};

struct lkpi_lock *
lkpi_lock_alloc(void)
{
	struct lkpi_lock *l = malloc(sizeof(*l));
	if (l != NULL)
		pthread_mutex_init(&l->m, NULL);
	return l;
}

void
lkpi_lock_free(struct lkpi_lock *l)
{
	pthread_mutex_destroy(&l->m);
	free(l);
}

void
lkpi_lock_lock(struct lkpi_lock *l)
{
	pthread_mutex_lock(&l->m);
}

void
lkpi_lock_unlock(struct lkpi_lock *l)
{
	pthread_mutex_unlock(&l->m);
}

void
lkpi_udelay(unsigned long us)
{
	usleep((useconds_t)us);
}

void
lkpi_msleep(unsigned int ms)
{
	usleep((useconds_t)ms * 1000);
}

unsigned long
lkpi_jiffies(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long)ts.tv_sec * LKPI_HZ + (unsigned long)ts.tv_nsec / (1000000000 / LKPI_HZ);
}

void
lkpi_vlog(int level, const char *fmt, va_list ap)
{
	if (level <= lkpi_host_log_level)
		vfprintf(stderr, fmt, ap);
}

void
lkpi_log(int level, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	lkpi_vlog(level, fmt, ap);
	va_end(ap);
}
