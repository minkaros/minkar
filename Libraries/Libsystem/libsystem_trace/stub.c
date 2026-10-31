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
 * Noisy stubs (stub.h): each stub's first call is logged once per
 * process, with the image that called it.
 */

#include <dlfcn.h>
#include <stdarg.h>
#include <os/lock.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "log_internal.h"
#include "format.h"
#include "stub.h"

#define STUB_SLOTS 1024		/* distinct stubs remembered */

static os_unfair_lock stub_lock = OS_UNFAIR_LOCK_INIT;
static char *stub_seen[STUB_SLOTS];
static unsigned stub_count;

static uint32_t
stub_hash(const char *s)
{
	uint32_t h = 2166136261u;

	while (*s)
		h = (h ^ (uint8_t)*s++) * 16777619u;
	return h;
}

/* Whether this is the stub's first call (and remember it). */
static int
stub_first(const char *what)
{
	uint32_t i = stub_hash(what) % STUB_SLOTS, n;
	int first = 0;

	os_unfair_lock_lock(&stub_lock);
	for (n = 0; n < STUB_SLOTS; n++, i = (i + 1) % STUB_SLOTS) {
		if (stub_seen[i] == NULL) {
			if (stub_count < STUB_SLOTS - 1) {
				stub_seen[i] = strdup(what);
				stub_count++;
			}
			first = 1;
			break;
		}
		if (strcmp(stub_seen[i], what) == 0)
			break;
	}
	os_unfair_lock_unlock(&stub_lock);
	return first;
}

static void
stub_log(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_os_log_emit_va("org.minkar.stubs", "unimplemented", OS_LOG_TYPE_ERROR,
	    fmt, ap, true);
	va_end(ap);
}

void
_minkar_stub(const char *what, const void *caller)
{
	Dl_info info;
	const char *image = "?";

	if (what == NULL || !stub_first(what))
		return;
	if (caller != NULL && dladdr(caller, &info) && info.dli_fname != NULL)
		image = info.dli_fname;
	stub_log("stub called: %{public}s (from %{public}s)", what, image);
}
