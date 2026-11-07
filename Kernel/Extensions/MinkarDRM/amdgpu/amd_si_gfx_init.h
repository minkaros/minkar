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
 * The graphics block's start on GCN 1.0 GPUs (SI), as amdgpu's
 * gfx_v6_0_constants_init (MIT) does it before the RLC and the command
 * processor: the golden settings of the graphics registers, the address
 * configuration for the DRAM's row size, the 32 tiling modes, the
 * rasterizer's configuration over the render backends that are there, the
 * SPI's thread management over the compute units that are, and the
 * fixed settings. Plain C over register accessors, for the driver and for
 * host tests (on a register file). What it found is left in the info, for
 * the clients' drivers (Mesa's radeonsi and radv want it).
 */

#ifndef _MINKAR_AMD_SI_GFX_INIT_H
#define _MINKAR_AMD_SI_GFX_INIT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "amd_si_gfx_tables.h"

struct amd_si_chip {
	const char *	name;			/* as MinkarAMDFamily names it */
	unsigned	shaderEngines, tilePipes, cusPerSH, shsPerSE, rbsPerSE, tccs;
	uint32_t	scFrontend, scBackend, scHiZ, scEarlyZ;	/* PA_SC_FIFO_SIZE */
	uint32_t	addrConfig;		/* GB_ADDR_CONFIG's golden value */
	uint32_t	rasterConfig;		/* with every render backend there */
	const uint32_t (*tiling)[32];		/* [1, 2, 4 KB rows][mode] */
	const uint32_t *golden[3];		/* triplets: register, mask, value */
	size_t		goldenCount[3];		/* dwords */
};

#define AMD_SI_RASTER(rbx2, rbx, rby, pkrm, pkry, sem, sex, sey) \
	((rbx2) << SI_PA_SC_RASTER_CONFIG__RB_XSEL2__SHIFT | \
	 (rbx) << SI_PA_SC_RASTER_CONFIG__RB_XSEL__SHIFT | \
	 (rby) << SI_PA_SC_RASTER_CONFIG__RB_YSEL__SHIFT | \
	 (pkrm) << SI_PA_SC_RASTER_CONFIG__PKR_MAP__SHIFT | \
	 (pkry) << SI_PA_SC_RASTER_CONFIG__PKR_YSEL__SHIFT | \
	 (sem) << SI_PA_SC_RASTER_CONFIG__SE_MAP__SHIFT | \
	 (sex) << SI_PA_SC_RASTER_CONFIG__SE_XSEL__SHIFT | \
	 (sey) << SI_PA_SC_RASTER_CONFIG__SE_YSEL__SHIFT)

#define AMD_SI_N(a)	(sizeof(a) / sizeof((a)[0]))

/* gfx_v6_0_constants_init's and gfx_v6_0_raster_config's values. */
static const struct amd_si_chip amd_si_chips[] = {
	{ "Tahiti", 2, 12, 8, 2, 4, 12, 0x20, 0x100, 0x30, 0x130,
	  SI_TAHITI_GB_ADDR_CONFIG_GOLDEN, AMD_SI_RASTER(2, 1, 0, 2, 1, 2, 2, 2),
	  si_tahiti_tiling,
	  { si_tahiti_golden_registers, si_tahiti_golden_rlc_registers, si_tahiti_golden_registers2 },
	  { AMD_SI_N(si_tahiti_golden_registers), AMD_SI_N(si_tahiti_golden_rlc_registers),
	    AMD_SI_N(si_tahiti_golden_registers2) } },
	{ "Pitcairn", 2, 8, 5, 2, 4, 8, 0x20, 0x100, 0x30, 0x130,
	  SI_TAHITI_GB_ADDR_CONFIG_GOLDEN, AMD_SI_RASTER(2, 1, 0, 2, 1, 2, 2, 2),
	  si_pitcairn_tiling,
	  { si_pitcairn_golden_registers, si_pitcairn_golden_rlc_registers, NULL },
	  { AMD_SI_N(si_pitcairn_golden_registers), AMD_SI_N(si_pitcairn_golden_rlc_registers), 0 } },
	{ "Cape Verde", 1, 4, 5, 2, 4, 4, 0x20, 0x40, 0x30, 0x130,
	  SI_VERDE_GB_ADDR_CONFIG_GOLDEN, AMD_SI_RASTER(0, 1, 0, 2, 1, 0, 0, 0),
	  si_verde_tiling,
	  { si_verde_golden_registers, si_verde_golden_rlc_registers, NULL },
	  { AMD_SI_N(si_verde_golden_registers), AMD_SI_N(si_verde_golden_rlc_registers), 0 } },
	{ "Oland", 1, 4, 6, 1, 2, 4, 0x20, 0x40, 0x30, 0x130,
	  SI_VERDE_GB_ADDR_CONFIG_GOLDEN, AMD_SI_RASTER(0, 0, 1, 0, 0, 0, 0, 0),
	  si_oland_tiling,
	  { si_oland_golden_registers, si_oland_golden_rlc_registers, NULL },
	  { AMD_SI_N(si_oland_golden_registers), AMD_SI_N(si_oland_golden_rlc_registers), 0 } },
	{ "Hainan", 1, 4, 5, 1, 1, 2, 0x20, 0x40, 0x30, 0x130,
	  SI_HAINAN_GB_ADDR_CONFIG_GOLDEN, 0,
	  si_hainan_tiling,
	  { si_hainan_golden_registers, si_hainan_golden_registers2, NULL },
	  { AMD_SI_N(si_hainan_golden_registers), AMD_SI_N(si_hainan_golden_registers2), 0 } },
};

static inline const struct amd_si_chip *
amd_si_chip_named(const char *name)
{
	for (size_t i = 0; i < AMD_SI_N(amd_si_chips); i++)
		if (strcmp(amd_si_chips[i].name, name) == 0)
			return &amd_si_chips[i];
	return NULL;
}

/* What the start found and set. */
struct amd_si_gfx_info {
	uint32_t	rowKB;			/* the DRAM's row size */
	uint32_t	addrConfig;		/* GB_ADDR_CONFIG */
	uint32_t	backendMask;		/* render backends there, by SE and SH */
	unsigned	backends;
	uint32_t	rasterConfig;		/* PA_SC_RASTER_CONFIG, 0: the golden one kept */
	uint32_t	cuMask[4][2];		/* compute units there, by SE and SH */
	unsigned	cus;
	unsigned	goldenSet, goldenSkipped;	/* outside the graphics block */
	uint32_t	tiling[32];
	uint32_t	rbDisable[4];		/* CC_RB_BACKEND_DISABLE, by SE (array 0) */
	uint32_t	mcArbRamcfg;		/* MC_ARB_RAMCFG */
};

struct amd_si_regs_io {
	uint32_t (*rd)(void *ctx, uint32_t reg);
	void (*wr)(void *ctx, uint32_t reg, uint32_t value);
	void *ctx;
};

static inline unsigned
amd_si_popcount(uint32_t v)
{
	unsigned n = 0;
	for (; v != 0; v &= v - 1)
		n++;
	return n;
}

static inline uint32_t
amd_si_bitmask(unsigned width)
{
	return width >= 32 ? 0xffffffffu : (1u << width) - 1;
}

/* One shader engine and array (or all of either: ~0u), for the registers
 * that have one copy for each. */
static inline void
amd_si_select_se_sh(const struct amd_si_regs_io *io, uint32_t se, uint32_t sh)
{
	uint32_t v = SI_GRBM_GFX_INDEX__INSTANCE_BROADCAST_WRITES_MASK;
	if (se == ~0u && sh == ~0u)
		v |= SI_GRBM_GFX_INDEX__SH_BROADCAST_WRITES_MASK | SI_GRBM_GFX_INDEX__SE_BROADCAST_WRITES_MASK;
	else if (se == ~0u)
		v |= SI_GRBM_GFX_INDEX__SE_BROADCAST_WRITES_MASK | sh << SI_GRBM_GFX_INDEX__SH_INDEX__SHIFT;
	else if (sh == ~0u)
		v |= SI_GRBM_GFX_INDEX__SH_BROADCAST_WRITES_MASK | se << SI_GRBM_GFX_INDEX__SE_INDEX__SHIFT;
	else
		v |= sh << SI_GRBM_GFX_INDEX__SH_INDEX__SHIFT | se << SI_GRBM_GFX_INDEX__SE_INDEX__SHIFT;
	io->wr(io->ctx, SI_mmGRBM_GFX_INDEX, v);
}

/* The golden settings of the graphics block's registers (and the GPU's
 * memory's, from 0x2000 on); the display's and the memory controller's
 * below it are left as the firmware and the display driver set them. */
static inline void
amd_si_apply_golden(const struct amd_si_regs_io *io, const uint32_t *t, size_t n,
    struct amd_si_gfx_info *info)
{
	for (size_t i = 0; t != NULL && i + 2 < n; i += 3) {
		uint32_t reg = t[i], mask = t[i + 1], value = t[i + 2];
		if (reg < 0x2000) {
			info->goldenSkipped++;
			continue;
		}
		if (mask != 0xffffffffu)
			value = (io->rd(io->ctx, reg) & ~mask) | (value & mask);
		io->wr(io->ctx, reg, value);
		info->goldenSet++;
	}
}

/* 0, or -1 for a chip it does not know. */
static inline int
amd_si_gfx_init_engine(const struct amd_si_chip *c, const struct amd_si_regs_io *io,
    struct amd_si_gfx_info *info)
{
	memset(info, 0, sizeof(*info));
	if (c == NULL)
		return -1;
	for (unsigned g = 0; g < 3; g++)
		amd_si_apply_golden(io, c->golden[g], c->goldenCount[g], info);

	io->wr(io->ctx, SI_mmGRBM_CNTL, 0xffu << SI_GRBM_CNTL__READ_TIMEOUT__SHIFT);

	/* The DRAM's row: 4 bytes a column, 2^(8 + NOOFCOLS) columns; 4 KB at most. */
	info->mcArbRamcfg = io->rd(io->ctx, SI_mmMC_ARB_RAMCFG);
	uint32_t cols = (info->mcArbRamcfg & SI_MC_ARB_RAMCFG__NOOFCOLS_MASK) >>
	    SI_MC_ARB_RAMCFG__NOOFCOLS__SHIFT;
	info->rowKB = (4u * (1u << (8 + cols))) / 1024;
	if (info->rowKB > 4)
		info->rowKB = 4;
	uint32_t a = c->addrConfig & ~(uint32_t)SI_GB_ADDR_CONFIG__ROW_SIZE_MASK;
	a |= (info->rowKB == 4 ? 2u : info->rowKB == 2 ? 1u : 0u) << SI_GB_ADDR_CONFIG__ROW_SIZE__SHIFT;
	a &= ~(uint32_t)SI_GB_ADDR_CONFIG__NUM_SHADER_ENGINES_MASK;
	if (c->shaderEngines == 2)
		a |= 1u << SI_GB_ADDR_CONFIG__NUM_SHADER_ENGINES__SHIFT;
	info->addrConfig = a;
	io->wr(io->ctx, SI_mmGB_ADDR_CONFIG, a);
	io->wr(io->ctx, SI_mmDMIF_ADDR_CONFIG, a);
	io->wr(io->ctx, SI_mmDMIF_ADDR_CALC, a);
	io->wr(io->ctx, SI_mmHDP_ADDR_CONFIG, a);
	io->wr(io->ctx, SI_mmDMA_TILING_CONFIG + SI_DMA0_REGISTER_OFFSET, a);
	io->wr(io->ctx, SI_mmDMA_TILING_CONFIG + SI_DMA1_REGISTER_OFFSET, a);

	/* The tiling modes for that row size. */
	const uint32_t *modes = c->tiling[info->rowKB == 4 ? 2 : info->rowKB == 2 ? 1 : 0];
	for (unsigned i = 0; i < 32; i++) {
		info->tiling[i] = modes[i];
		io->wr(io->ctx, SI_mmGB_TILE_MODE0 + i, modes[i]);
	}

	/* The render backends there, by shader engine and array; the
	 * rasterizer over them (all of them: the configuration as is). */
	unsigned perSH = c->rbsPerSE / c->shsPerSE;
	for (unsigned se = 0; se < c->shaderEngines; se++)
		for (unsigned sh = 0; sh < c->shsPerSE; sh++) {
			amd_si_select_se_sh(io, se, sh);
			uint32_t cc = io->rd(io->ctx, SI_mmCC_RB_BACKEND_DISABLE);
			if (sh == 0 && se < 4)
				info->rbDisable[se] = cc;
			uint32_t off = (cc | io->rd(io->ctx, SI_mmGC_USER_RB_BACKEND_DISABLE)) &
			    SI_GC_USER_RB_BACKEND_DISABLE__BACKEND_DISABLE_MASK;
			off >>= SI_GC_USER_RB_BACKEND_DISABLE__BACKEND_DISABLE__SHIFT;
			info->backendMask |= (~off & amd_si_bitmask(perSH)) << ((se * c->shsPerSE + sh) * perSH);
		}
	amd_si_select_se_sh(io, ~0u, ~0u);
	info->backends = amd_si_popcount(info->backendMask);
	unsigned pipes = c->rbsPerSE * c->shaderEngines;
	if (pipes > 16)
		pipes = 16;
	if (info->backendMask == 0 || info->backends >= pipes) {
		info->rasterConfig = c->rasterConfig;
		io->wr(io->ctx, SI_mmPA_SC_RASTER_CONFIG, c->rasterConfig);
	}
	/* (Some backends off: the golden configuration stays, which
	 * gfx_v6_0_write_harvested_raster_configs would refine.) */

	/* The compute units there; the SPI's static thread management off
	 * for the first, as gfx_v6_0_setup_spi does. */
	for (unsigned se = 0; se < c->shaderEngines && se < 4; se++)
		for (unsigned sh = 0; sh < c->shsPerSE && sh < 2; sh++) {
			amd_si_select_se_sh(io, se, sh);
			uint32_t off = (io->rd(io->ctx, SI_mmCC_GC_SHADER_ARRAY_CONFIG) |
			    io->rd(io->ctx, SI_mmGC_USER_SHADER_ARRAY_CONFIG)) &
			    SI_GC_USER_SHADER_ARRAY_CONFIG__INACTIVE_CUS_MASK;
			off >>= SI_GC_USER_SHADER_ARRAY_CONFIG__INACTIVE_CUS__SHIFT;
			uint32_t on = ~off & amd_si_bitmask(c->cusPerSH);
			info->cuMask[se][sh] = on;
			info->cus += amd_si_popcount(on);
			uint32_t data = io->rd(io->ctx, SI_mmSPI_STATIC_THREAD_MGMT_3), mask = 1;
			for (unsigned k = 0; k < 16; k++) {
				mask <<= k;
				if (on & mask) {
					data &= ~mask;
					io->wr(io->ctx, SI_mmSPI_STATIC_THREAD_MGMT_3, data);
					break;
				}
			}
		}
	amd_si_select_se_sh(io, ~0u, ~0u);

	/* The fixed settings. */
	io->wr(io->ctx, SI_mmCP_QUEUE_THRESHOLDS, 0x16u << SI_CP_QUEUE_THRESHOLDS__ROQ_IB1_START__SHIFT |
	    0x2bu << SI_CP_QUEUE_THRESHOLDS__ROQ_IB2_START__SHIFT);
	io->wr(io->ctx, SI_mmCP_MEQ_THRESHOLDS, 0x30u << SI_CP_MEQ_THRESHOLDS__MEQ1_START__SHIFT |
	    0x60u << SI_CP_MEQ_THRESHOLDS__MEQ2_START__SHIFT);
	io->wr(io->ctx, SI_mmSX_DEBUG_1, io->rd(io->ctx, SI_mmSX_DEBUG_1));
	io->wr(io->ctx, SI_mmSPI_CONFIG_CNTL_1, 4u << SI_SPI_CONFIG_CNTL_1__VTX_DONE_DELAY__SHIFT);
	io->wr(io->ctx, SI_mmPA_SC_FIFO_SIZE,
	    c->scFrontend << SI_PA_SC_FIFO_SIZE__SC_FRONTEND_PRIM_FIFO_SIZE__SHIFT |
	    c->scBackend << SI_PA_SC_FIFO_SIZE__SC_BACKEND_PRIM_FIFO_SIZE__SHIFT |
	    c->scHiZ << SI_PA_SC_FIFO_SIZE__SC_HIZ_TILE_FIFO_SIZE__SHIFT |
	    c->scEarlyZ << SI_PA_SC_FIFO_SIZE__SC_EARLYZ_TILE_FIFO_SIZE__SHIFT);
	io->wr(io->ctx, SI_mmVGT_NUM_INSTANCES, 1);
	io->wr(io->ctx, SI_mmCP_PERFMON_CNTL, 0);
	io->wr(io->ctx, SI_mmSQ_CONFIG, 0);
	io->wr(io->ctx, SI_mmPA_SC_FORCE_EOV_MAX_CNTS,
	    4095u << SI_PA_SC_FORCE_EOV_MAX_CNTS__FORCE_EOV_MAX_CLK_CNT__SHIFT |
	    255u << SI_PA_SC_FORCE_EOV_MAX_CNTS__FORCE_EOV_MAX_REZ_CNT__SHIFT);
	io->wr(io->ctx, SI_mmVGT_CACHE_INVALIDATION,
	    SI_VC_AND_TC << SI_VGT_CACHE_INVALIDATION__CACHE_INVALIDATION__SHIFT |
	    SI_ES_AND_GS_AUTO << SI_VGT_CACHE_INVALIDATION__AUTO_INVLD_EN__SHIFT);
	io->wr(io->ctx, SI_mmVGT_GS_VERTEX_REUSE, 16);
	io->wr(io->ctx, SI_mmPA_SC_LINE_STIPPLE_STATE, 0);
	static const uint32_t perf[] = {
		SI_mmCB_PERFCOUNTER0_SELECT0, SI_mmCB_PERFCOUNTER0_SELECT1, SI_mmCB_PERFCOUNTER1_SELECT0,
		SI_mmCB_PERFCOUNTER1_SELECT1, SI_mmCB_PERFCOUNTER2_SELECT0, SI_mmCB_PERFCOUNTER2_SELECT1,
		SI_mmCB_PERFCOUNTER3_SELECT0, SI_mmCB_PERFCOUNTER3_SELECT1,
	};
	for (size_t i = 0; i < AMD_SI_N(perf); i++)
		io->wr(io->ctx, perf[i], 0);
	io->wr(io->ctx, SI_mmHDP_HOST_PATH_CNTL, io->rd(io->ctx, SI_mmHDP_HOST_PATH_CNTL));
	io->wr(io->ctx, SI_mmPA_CL_ENHANCE, SI_PA_CL_ENHANCE__CLIP_VTX_REORDER_ENA_MASK |
	    3u << SI_PA_CL_ENHANCE__NUM_CLIP_SEQ__SHIFT);
	return 0;
}

#endif
