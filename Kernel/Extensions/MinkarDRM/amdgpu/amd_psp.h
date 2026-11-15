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
 * The security processor (PSP) of AMD's GPUs from Navi on (psp_v11_0.c in
 * amdgpu, MIT), as MinkarAMDGPU starts it on Navi 23: its bootloader
 * given the secure OS's parts from AMD's sos file, then its ring, the
 * trusted memory region and the SMU's firmware. Its working memory is a
 * region of VRAM the CPU reaches (amdgpu puts the firmware buffer in VRAM
 * under SR-IOV and with debug_use_vram_fw_buf):
 *
 *	+0	the firmware buffer (1 MB), for what the bootloader loads
 *	+1 MB	the ring (4 KB), its fence (4 KB), its command buffer (4 KB)
 *	+2 MB	an IP's firmware for LOAD_IP_FW (up to 2 MB)
 *	+16 MB	the trusted memory region (up to 16 MB; 10 MB on Navi 23, from
 *		its TOC), 1 MB aligned as amdgpu's PSP_TMR_ALIGNMENT
 */

#ifndef _MINKAR_AMD_PSP_H
#define _MINKAR_AMD_PSP_H

#include <stdint.h>
#include <stddef.h>

enum {
	AMD_PSP_REGION_BYTES = 32 << 20,
	AMD_PSP_FW_OFFSET = 0, AMD_PSP_FW_BYTES = 1 << 20,
	AMD_PSP_RING_OFFSET = 1 << 20, AMD_PSP_RING_BYTES = 4096,
	AMD_PSP_FENCE_OFFSET = (1 << 20) + 4096,
	AMD_PSP_CMD_OFFSET = (1 << 20) + 8192, AMD_PSP_CMD_BYTES = 4096,
	AMD_PSP_IP_FW_OFFSET = 2 << 20, AMD_PSP_IP_FW_BYTES = 2 << 20,
	AMD_PSP_TMR_OFFSET = 16 << 20, AMD_PSP_TMR_BYTES = 16 << 20,
};

/* The sos file's parts (psp_firmware_header_v1_x): offsets into the file. */
struct amd_psp_part {
	uint32_t offset, bytes;
};
struct amd_psp_sos {
	struct amd_psp_part sys, sos, kdb, spl, toc;
	uint32_t version;
};

#ifdef __cplusplus
extern "C" {
#endif

/* 0, or -1 when the file is not a version 1 sos file it can read. */
int amd_psp_parse_sos(const uint8_t *file, size_t bytes, struct amd_psp_sos *out);

#ifdef __cplusplus
}
#endif

#endif
