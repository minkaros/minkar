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

#ifndef _LKPI_LINUX_KERNEL_H
#define _LKPI_LINUX_KERNEL_H

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/printk.h>

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]))
#define min(a, b)	((a) < (b) ? (a) : (b))
#define max(a, b)	((a) > (b) ? (a) : (b))
#define min_t(t, a, b)	((t)(a) < (t)(b) ? (t)(a) : (t)(b))
#define max_t(t, a, b)	((t)(a) > (t)(b) ? (t)(a) : (t)(b))
#define DIV_ROUND_UP(n, d)	(((n) + (d) - 1) / (d))
#define lower_32_bits(n)	((u32)((n) & 0xffffffffu))
#define upper_32_bits(n)	((u32)(((u64)(n)) >> 32))
/* Divides the 64-bit lvalue n in place; the remainder is the value. */
#define do_div(n, base)	({ u32 _b = (base); u32 _r = (u32)((n) % _b); (n) /= _b; _r; })
#define BUG()		__builtin_trap()
#define BUG_ON(c)	do { if (unlikely(c)) BUG(); } while (0)
#define WARN_ON(c)	({ int _w = !!(c); if (unlikely(_w)) \
	lkpi_log(4, "WARN_ON(%s) at %s:%d\n", #c, __FILE__, __LINE__); unlikely(_w); })
#define WARN_ON_ONCE(c)	WARN_ON(c)

#endif
