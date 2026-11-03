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

#ifndef _LKPI_LINUX_SLAB_H
#define _LKPI_LINUX_SLAB_H

#include <linux/types.h>
#include <string.h>

#define GFP_KERNEL	0x1u
#define GFP_ATOMIC	0x2u
#define __GFP_ZERO	0x100u

static inline void *kmalloc(size_t size, gfp_t flags)
{
	return lkpi_alloc(size, (flags & __GFP_ZERO) != 0);
}

static inline void *kzalloc(size_t size, gfp_t flags)
{
	(void)flags;
	return lkpi_alloc(size, 1);
}

static inline void *kcalloc(size_t n, size_t size, gfp_t flags)
{
	(void)flags;
	if (size != 0 && n > SIZE_MAX / size)
		return NULL;
	return lkpi_alloc(n * size, 1);
}

static inline void *kmemdup(const void *p, size_t size, gfp_t flags)
{
	void *q = kmalloc(size, flags);
	if (q != NULL)
		memcpy(q, p, size);
	return q;
}

static inline void kfree(const void *p)
{
	lkpi_free((void *)(uintptr_t)p);
}

#define kvfree(p)	kfree(p)
#define kvzalloc(s, f)	kzalloc(s, f)

#endif
