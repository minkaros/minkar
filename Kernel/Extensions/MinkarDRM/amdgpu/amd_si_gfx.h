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
 * The graphics command processor of GCN 1.0 GPUs (SI): its registers, the
 * PM4 packets the driver writes to its ring, and AMD's microcode files
 * for it (the prefetch parser, the micro engine and the constant engine),
 * after the layout amdgpu's gfx_v6_0.c, sid.h and amdgpu_ucode.h (MIT)
 * describe. Plain C, for the driver and for host tests.
 */

#ifndef _MINKAR_AMD_SI_GFX_H
#define _MINKAR_AMD_SI_GFX_H

#include <stddef.h>
#include <stdint.h>

/* Register dword indices (gca/gfx_6_0_d.h). */
#define mmSI_GRBM_STATUS		0x2004
#define mmSI_CP_STAT			0x21a0
#define mmSI_CP_ME_CNTL			0x21b6
#define   SI_CP_CE_HALT			(1u << 24)
#define   SI_CP_PFP_HALT		(1u << 26)
#define   SI_CP_ME_HALT			(1u << 28)
#define mmSI_CP_RB0_RPTR		0x21c0
#define mmSI_CP_RB_WPTR_DELAY		0x21c1
#define mmSI_CP_SEM_WAIT_TIMER		0x216f
#define mmSI_CP_SEM_INCOMPLETE_TIMER_CNTL 0x2172
#define mmSI_SCRATCH_REG0		0x2140
#define mmSI_SCRATCH_UMSK		0x2150
#define mmSI_SCRATCH_ADDR		0x2151
#define mmSI_CP_RB0_BASE		0x3040	/* address >> 8 */
#define mmSI_CP_RB0_CNTL		0x3041
#define   SI_CP_RB_NO_UPDATE		(1u << 27)	/* no read pointer write-back */
#define   SI_CP_RB_RPTR_WR_ENA		(1u << 31)
#define mmSI_CP_RB0_RPTR_ADDR		0x3043
#define mmSI_CP_RB0_RPTR_ADDR_HI	0x3044
#define mmSI_CP_RB0_WPTR		0x3045	/* in dwords */
#define mmSI_CP_PFP_UCODE_ADDR		0x3054
#define mmSI_CP_PFP_UCODE_DATA		0x3055
#define mmSI_CP_ME_RAM_RADDR		0x3056
#define mmSI_CP_ME_RAM_WADDR		0x3057
#define mmSI_CP_ME_RAM_DATA		0x3058
#define mmSI_CP_CE_UCODE_ADDR		0x305a
#define mmSI_CP_CE_UCODE_DATA		0x305b
#define mmSI_CP_DEBUG			0x307f

/* PM4 type 3 packets: an opcode and n + 1 dwords after the header. */
#define SI_PACKET3(op, n)	((3u << 30) | (((uint32_t)(n) & 0x3fff) << 16) | \
				 (((uint32_t)(op) & 0xff) << 8))
enum {
	kSIPacket3NOP			= 0x10,
	kSIPacket3SetBase		= 0x11,
	kSIPacket3WriteData		= 0x37,
	kSIPacket3IndirectBuffer	= 0x3f,
	kSIPacket3MEInitialize		= 0x44,
	kSIPacket3SetConfigReg		= 0x68,
};
#define kSIConfigRegStart		0x2000
#define kSICEPartitionBase		3
#define kSIMaxHWContexts		8	/* Tahiti, Pitcairn, Verde */

/*
 * One of AMD's microcode files for amdgpu: a header (little endian, the
 * common header's version 1.x: total size, header size, versions, the
 * payload's size and offset) and its payload, whole dwords.
 */
struct amd_ucode {
	const uint8_t *	payload;
	uint32_t	words;
	uint32_t	version;	/* the microcode's own */
	uint16_t	ipMajor, ipMinor;
};

static inline uint32_t
amd_le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* 0, or -1 when the file is not one (truncated, sizes that do not add up). */
static inline int
amd_ucode_parse(const uint8_t *file, size_t bytes, struct amd_ucode *u)
{
	if (bytes < 32)
		return -1;
	uint32_t total = amd_le32(file), header = amd_le32(file + 4);
	uint16_t major = (uint16_t)(file[8] | file[9] << 8);
	uint32_t size = amd_le32(file + 20), offset = amd_le32(file + 24);
	if (total != bytes || header < 32 || header > bytes || major != 1 ||
	    size == 0 || (size & 3) != 0 || offset < header || offset > bytes ||
	    size > bytes - offset)
		return -1;
	u->payload = file + offset;
	u->words = size / 4;
	u->version = amd_le32(file + 16);
	u->ipMajor = (uint16_t)(file[12] | file[13] << 8);
	u->ipMinor = (uint16_t)(file[14] | file[15] << 8);
	return 0;
}

/* The payload's word i. */
static inline uint32_t
amd_ucode_word(const struct amd_ucode *u, uint32_t i)
{
	return amd_le32(u->payload + 4 * (size_t)i);
}

/* CP_RB0_CNTL for a ring of bytes (a power of two, 4 KB at least): its
 * size and block size as log2 of quadwords, no write-back. */
static inline uint32_t
amd_si_gfx_ring_cntl(uint32_t bytes)
{
	uint32_t bufsz = 0, blksz = 0;
	while ((8u << bufsz) < bytes)
		bufsz++;
	while ((8u << blksz) < 4096)
		blksz++;
	return bufsz | blksz << 8 | SI_CP_RB_NO_UPDATE;
}

/* The micro engine's start: ME_INITIALIZE, then the constant engine's
 * partition; 11 dwords. */
static inline unsigned
amd_si_gfx_emit_start(uint32_t *p)
{
	unsigned n = 0;
	p[n++] = SI_PACKET3(kSIPacket3MEInitialize, 5);
	p[n++] = 0x1;
	p[n++] = 0x0;
	p[n++] = kSIMaxHWContexts - 1;
	p[n++] = 1u << 16;		/* device id 1 */
	p[n++] = 0;
	p[n++] = 0;
	p[n++] = SI_PACKET3(kSIPacket3SetBase, 2);
	p[n++] = kSICEPartitionBase;
	p[n++] = 0xc000;
	p[n++] = 0xe000;
	return n;
}

/* A config register set through the ring (a scratch register, for the
 * ring's test); 3 dwords. */
static inline unsigned
amd_si_gfx_emit_set_config(uint32_t *p, uint32_t reg, uint32_t value)
{
	p[0] = SI_PACKET3(kSIPacket3SetConfigReg, 1);
	p[1] = reg - kSIConfigRegStart;
	p[2] = value;
	return 3;
}

#endif
