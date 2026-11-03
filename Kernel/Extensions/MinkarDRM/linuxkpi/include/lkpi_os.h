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
 * MinkarDRM's LinuxKPI: what Linux drivers (drm-kmod, ADR 0002) expect of
 * the kernel, over a small backend. lkpi_xnu.cpp gives it in the kernel
 * (IOKit and Mach), lkpi_host.c in host unit tests. Only what imported
 * code uses is here; it grows with each import.
 */
#ifndef _LKPI_OS_H
#define _LKPI_OS_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

void *lkpi_alloc(size_t size, int zero);
void lkpi_free(void *p);

struct lkpi_lock;
struct lkpi_lock *lkpi_lock_alloc(void);
void lkpi_lock_free(struct lkpi_lock *l);
void lkpi_lock_lock(struct lkpi_lock *l);
void lkpi_lock_unlock(struct lkpi_lock *l);

void lkpi_udelay(unsigned long us);	/* busy wait */
void lkpi_msleep(unsigned int ms);	/* may sleep */
unsigned long lkpi_jiffies(void);	/* LKPI_HZ per second */

/* Levels as printk's: 3 error, 4 warning, 6 info, 7 debug. */
void lkpi_vlog(int level, const char *fmt, va_list ap);
void lkpi_log(int level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#define LKPI_HZ	1000

#ifdef __cplusplus
}
#endif

#endif
