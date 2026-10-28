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
 * Capsicum for the FreeBSD commands built on Minkar: Darwin has no
 * capability mode, so entering it and limiting rights do nothing and
 * succeed (the command keeps the rights it already has).
 */

#ifndef _MINKAR_COMPAT_SYS_CAPSICUM_H
#define _MINKAR_COMPAT_SYS_CAPSICUM_H

#include <sys/types.h>
/* FreeBSD's <sys/capsicum.h> brings <sys/param.h>; commands count on it
 * for the limits (INT_MAX and the like). */
#include <sys/param.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct { uint64_t cr_rights[2]; } cap_rights_t;
typedef uint64_t cap_ioctl_t;

#define CAP_READ	0x01ULL
#define CAP_WRITE	0x02ULL
#define CAP_SEEK	0x04ULL
#define CAP_FSTAT	0x08ULL
#define CAP_FCNTL	0x10ULL
#define CAP_IOCTL	0x20ULL
#define CAP_MMAP_R	0x40ULL
#define CAP_EVENT	0x80ULL
#define CAP_LOOKUP	0x100ULL
#define CAP_FSTATAT	0x200ULL
#define CAP_FTRUNCATE	0x400ULL
#define CAP_CREATE	0x800ULL
#define CAP_PREAD	0x1000ULL
#define CAP_PWRITE	0x2000ULL
#define CAP_FCHMOD	0x4000ULL
#define CAP_FCHOWN	0x8000ULL
#define CAP_FUTIMES	0x10000ULL
#define CAP_CONNECT	0x20000ULL
#define CAP_SEND	0x40000ULL
#define CAP_RECV	0x80000ULL
#define CAP_MMAP_RW	0x100000ULL
#define CAP_SETSOCKOPT	0x200000ULL
#define CAP_GETSOCKOPT	0x400000ULL
#define CAP_ACCEPT	0x800000ULL
#define CAP_BIND	0x1000000ULL
#define CAP_SHUTDOWN	0x2000000ULL

#define CAP_FCNTL_GETFL	0x08

#define cap_rights_init(r, ...)		((void)(r), (r))
#define cap_rights_set(r, ...)		((void)(r), (r))
#define cap_rights_clear(r, ...)	((void)(r), (r))
#define cap_rights_is_set(r, ...)	((void)(r), 1)

static inline int cap_enter(void) { return (0); }
static inline int cap_getmode(unsigned int *modep) { *modep = 0; return (0); }
static inline bool cap_sandboxed(void) { return (false); }
static inline int cap_rights_limit(int fd, const cap_rights_t *r) { (void)fd; (void)r; return (0); }
static inline int cap_ioctls_limit(int fd, const cap_ioctl_t *c, size_t n) { (void)fd; (void)c; (void)n; return (0); }
static inline int cap_fcntls_limit(int fd, uint32_t f) { (void)fd; (void)f; return (0); }

#endif /* !_MINKAR_COMPAT_SYS_CAPSICUM_H */
