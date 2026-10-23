/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The ravynOS contributors
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
 * libcasper for the FreeBSD commands built on ravynOS: there is no
 * capability mode, so no Casper services; the cap_* helpers (casper/)
 * do the plain operations.
 */

#ifndef _RAVYNOS_COMPAT_LIBCASPER_H
#define _RAVYNOS_COMPAT_LIBCASPER_H

#include <sys/capsicum.h>
#include <stddef.h>

typedef struct cap_channel { int unused; } cap_channel_t;

static inline cap_channel_t *cap_init(void) { static cap_channel_t c; return (&c); }
static inline cap_channel_t *cap_service_open(cap_channel_t *c, const char *name) { (void)name; return (c); }
static inline void cap_close(cap_channel_t *c) { (void)c; }
static inline int cap_limit_set(cap_channel_t *c, void *limits) { (void)c; (void)limits; return (0); }

#endif /* !_RAVYNOS_COMPAT_LIBCASPER_H */
