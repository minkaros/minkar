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

/* The sos file's parts, as amdgpu's psp_init_sos_microcode reads a
 * version 1 header (1.0 to 1.3); MIT. */

#include "amd_psp.h"

static uint32_t
le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* A part at offset (from the payload) and of bytes, kept if it lies
 * inside the file. */
static void
part(struct amd_psp_part *p, uint32_t payload, uint32_t offset, uint32_t bytes, size_t file)
{
	p->offset = payload + offset;
	p->bytes = bytes;
	if (bytes == 0 || (uint64_t)p->offset + bytes > file) {
		p->offset = 0;
		p->bytes = 0;
	}
}

int
amd_psp_parse_sos(const uint8_t *f, size_t bytes, struct amd_psp_sos *out)
{
	/* common_firmware_header: 32 bytes; then sos (version, offset,
	 * size); 1.1/1.3: toc, kdb; 1.3: spl, rl, sys_drv_aux, sos_aux. */
	if (bytes < 44)
		return -1;
	uint32_t size = le32(f), payload = le32(f + 24);
	uint16_t major = (uint16_t)(f[8] | f[9] << 8), minor = (uint16_t)(f[10] | f[11] << 8);
	if (major != 1 || size > bytes || payload >= bytes)
		return -1;
	for (unsigned i = 0; i < sizeof(*out); i++)
		((uint8_t *)out)[i] = 0;
	out->version = le32(f + 16);
	/* The system driver first, then the secure OS. */
	part(&out->sys, payload, 0, le32(f + 36), bytes);
	part(&out->sos, payload, le32(f + 36), le32(f + 40), bytes);
	if ((minor == 1 || minor == 3) && bytes >= 68) {
		part(&out->toc, payload, le32(f + 48), le32(f + 52), bytes);
		part(&out->kdb, payload, le32(f + 60), le32(f + 64), bytes);
	}
	if (minor == 2 && bytes >= 68)
		part(&out->kdb, payload, le32(f + 60), le32(f + 64), bytes);
	if (minor == 3 && bytes >= 80)
		part(&out->spl, payload, le32(f + 72), le32(f + 76), bytes);
	return out->sys.bytes != 0 && out->sos.bytes != 0 ? 0 : -1;
}
