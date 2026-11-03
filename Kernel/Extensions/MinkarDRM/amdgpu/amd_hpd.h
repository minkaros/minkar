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
 * A display's hot-plug detect line, sampled now and then: an unplug or a
 * plug once the line has kept its new level for settle samples in a row,
 * so a cable's bounce while it goes in is not taken for two changes.
 */

#ifndef _MINKAR_AMD_HPD_H
#define _MINKAR_AMD_HPD_H

#include <stdint.h>

enum amd_hpd_event {
	AMD_HPD_NONE,
	AMD_HPD_UNPLUGGED,
	AMD_HPD_PLUGGED,
};

struct amd_hpd {
	uint8_t connected;	/* the level taken last */
	uint8_t count;		/* samples in a row at the other level */
};

static inline void
amd_hpd_init(struct amd_hpd *h, int sense)
{
	h->connected = sense != 0;
	h->count = 0;
}

static inline enum amd_hpd_event
amd_hpd_sample(struct amd_hpd *h, int sense, unsigned settle)
{
	if ((sense != 0) == h->connected) {
		h->count = 0;
		return AMD_HPD_NONE;
	}
	if (++h->count < (settle ? settle : 1))
		return AMD_HPD_NONE;
	h->connected = sense != 0;
	h->count = 0;
	return h->connected ? AMD_HPD_PLUGGED : AMD_HPD_UNPLUGGED;
}

#endif
