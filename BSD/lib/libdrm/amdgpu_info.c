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
 * amdgpu's kernel commands (DRM_AMDGPU_*), answered on the user client and
 * the IORegistry instead of Linux's ioctls: drmCommandWrite* lands here.
 * The device's information comes from what MinkarAMDGPU publishes
 * (MinkarGFXConfig, the PCI device's identity), its memory from the user
 * client's Info, the microcode's versions from its files; registers are
 * the ones the graphics block's start sets (MinkarGFXConfig), as amdgpu
 * gives clients its cached values.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>
#include <xf86drm.h>
#include <amdgpu_drm.h>
#include "MinkarAMDGPUInterface.h"
#include "minkar_drm_private.h"

#define MINKAR_VA_START		(1ull << 20)
#define MINKAR_VA_END		(64ull << 30)

/* The GPU behind a descriptor and its MinkarGFXConfig, retained. */
struct gpu_view {
	struct minkar_drm_gpu gpu;
	CFDictionaryRef config;
	io_connect_t conn;
};

static bool
viewFor(int fd, struct gpu_view *v)
{
	struct minkar_drm_gpu gpus[MINKAR_DRM_MAX_GPUS];
	unsigned n = minkar_drm_gpus(gpus, MINKAR_DRM_MAX_GPUS);
	int i = minkar_drm_gpu_for_minor(gpus, n, minkar_drm_fd_minor(fd));

	memset(v, 0, sizeof(*v));
	if (i < 0) {
		minkar_drm_release_gpus(gpus, n);
		return false;
	}
	v->gpu = gpus[i];
	IOObjectRetain(v->gpu.service);
	minkar_drm_release_gpus(gpus, n);
	CFTypeRef c = IORegistryEntryCreateCFProperty(v->gpu.service, CFSTR("MinkarGFXConfig"),
	    kCFAllocatorDefault, 0);
	if (c != NULL && CFGetTypeID(c) == CFDictionaryGetTypeID())
		v->config = c;
	else if (c != NULL)
		CFRelease(c);
	v->conn = minkar_drm_connection(fd);
	return true;
}

static void
viewRelease(struct gpu_view *v)
{
	if (v->config != NULL)
		CFRelease(v->config);
	IOObjectRelease(v->gpu.service);
}

static uint32_t
configNumber(const struct gpu_view *v, const char *key)
{
	uint32_t value = 0;
	if (v->config == NULL)
		return 0;
	CFStringRef k = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
	CFTypeRef n = CFDictionaryGetValue(v->config, k);
	if (n != NULL && CFGetTypeID(n) == CFNumberGetTypeID())
		CFNumberGetValue(n, kCFNumberSInt32Type, &value);
	CFRelease(k);
	return value;
}

/* Up to count dwords of an OSData in the config; how many. */
static unsigned
configWords(const struct gpu_view *v, const char *key, uint32_t *out, unsigned count)
{
	if (v->config == NULL)
		return 0;
	CFStringRef k = CFStringCreateWithCString(kCFAllocatorDefault, key, kCFStringEncodingUTF8);
	CFTypeRef d = CFDictionaryGetValue(v->config, k);
	CFRelease(k);
	if (d == NULL || CFGetTypeID(d) != CFDataGetTypeID())
		return 0;
	unsigned n = (unsigned)CFDataGetLength(d) / 4;
	n = n < count ? n : count;
	memcpy(out, CFDataGetBytePtr(d), n * 4);
	return n;
}

static uint32_t
popcount(uint32_t x)
{
	return (uint32_t)__builtin_popcount(x);
}

/* Tahiti's external revision from its PCI revision (si.c). */
static uint32_t
externalRevision(uint16_t device, uint8_t rev)
{
	if (device >= 0x6780 && device <= 0x679f)		/* Tahiti */
		return rev == 0 ? 1 : rev == 1 ? 5 : 6;
	if (device >= 0x6800 && device <= 0x6819)		/* Pitcairn */
		return rev + 20;
	if (device >= 0x6820 && device <= 0x683f)		/* Cape Verde */
		return rev + 40;
	if (device >= 0x6600 && device <= 0x6631)		/* Oland */
		return 60;
	return 70;						/* Hainan */
}

static int
devInfo(const struct gpu_view *v, struct drm_amdgpu_info_device *d)
{
	uint32_t cu[4][2];

	memset(d, 0, sizeof(*d));
	memset(cu, 0, sizeof(cu));
	configWords(v, "ComputeUnitMasks", &cu[0][0], 8);
	d->device_id = v->gpu.device;
	/* The chip's revision from its straps (as amdgpu's rev_id), and the
	 * external one Mesa's chip tables take from it. */
	d->chip_rev = configNumber(v, "RevisionID");
	d->external_rev = externalRevision(v->gpu.device, d->chip_rev);
	d->pci_rev = v->gpu.revision;
	d->family = AMDGPU_FAMILY_SI;
	d->num_shader_engines = configNumber(v, "ShaderEngines");
	d->num_shader_arrays_per_engine = configNumber(v, "ShaderArraysPerEngine");
	d->gpu_counter_freq = 27000;		/* kHz: the reference clock */
	/* The boot clocks (no power management yet), in kHz. */
	d->max_engine_clock = d->min_engine_clock = 450000;
	d->max_memory_clock = d->min_memory_clock = 150000;
	for (unsigned se = 0; se < 4; se++)
		for (unsigned sh = 0; sh < 2; sh++) {
			d->cu_bitmap[se][sh] = cu[se][sh];
			d->cu_active_number += popcount(cu[se][sh]);
		}
	d->cu_ao_mask = 0;
	d->enabled_rb_pipes_mask = configNumber(v, "BackendMask");
	d->num_rb_pipes = configNumber(v, "Backends");
	d->num_hw_gfx_contexts = 8;
	d->virtual_address_offset = MINKAR_VA_START;
	d->virtual_address_max = MINKAR_VA_END;
	d->virtual_address_alignment = 4096;
	d->pte_fragment_size = 4096;
	d->gart_page_size = 4096;
	d->ce_ram_size = 0x8000;
	d->vram_type = AMDGPU_VRAM_TYPE_GDDR5;
	d->vram_bit_width = configNumber(v, "TilePipes") * 32;	/* a channel a pipe */
	d->wave_front_size = 64;
	d->num_shader_visible_vgprs = 256;
	d->num_cu_per_sh = configNumber(v, "ComputeUnitsPerArray");
	d->num_tcc_blocks = configNumber(v, "TilePipes");
	d->gs_vgt_table_depth = 32;
	d->gs_prim_buffer_depth = 1792;
	d->max_gs_waves_per_vgt = 32;
	return v->config != NULL ? 0 : -ENODEV;
}

/* VRAM total, free, and the CPU's window on it, from the user client. */
static int
vram(const struct gpu_view *v, uint64_t *total, uint64_t *available, uint64_t *window)
{
	uint64_t out[3] = { 0, 0, 0 };
	uint32_t n = 3;
	if (v->conn == IO_OBJECT_NULL ||
	    IOConnectCallScalarMethod(v->conn, kMinkarAMDGPUInfo, NULL, 0, out, &n) != KERN_SUCCESS)
		return -EIO;
	*total = out[0];
	*available = out[1];
	*window = out[2];
	return 0;
}

static uint64_t
gartBytes(const struct gpu_view *v)
{
	uint32_t mb = 0;
	CFTypeRef p = IORegistryEntryCreateCFProperty(v->gpu.service, CFSTR("MinkarGARTMegabytes"),
	    kCFAllocatorDefault, 0);
	if (p != NULL && CFGetTypeID(p) == CFNumberGetTypeID())
		CFNumberGetValue(p, kCFNumberSInt32Type, &mb);
	if (p != NULL)
		CFRelease(p);
	return (uint64_t)mb << 20;
}

/* A microcode file's version, from its header (amdgpu's common header:
 * ucode_version at 16), or 0. */
static uint32_t
microcodeVersion(const char *part)
{
	char path[128];
	uint8_t h[32];
	snprintf(path, sizeof(path), "/usr/share/firmware/amdgpu/tahiti_%s.bin", part);
	FILE *f = fopen(path, "rb");
	if (f == NULL)
		return 0;
	size_t n = fread(h, 1, sizeof(h), f);
	fclose(f);
	return n == sizeof(h) ? (uint32_t)h[16] | (uint32_t)h[17] << 8 | (uint32_t)h[18] << 16 |
	    (uint32_t)h[19] << 24 : 0;
}

/* The registers clients read (ac_drm_query_gpu_info's), as set. */
static int
readRegisters(const struct gpu_view *v, uint32_t offset, uint32_t count, uint32_t instance,
    uint32_t *out)
{
	uint32_t words[32];
	unsigned se = (instance >> AMDGPU_INFO_MMR_SE_INDEX_SHIFT) & AMDGPU_INFO_MMR_SE_INDEX_MASK;

	if (instance == 0xffffffff)
		se = 0;
	for (uint32_t i = 0; i < count; i++) {
		uint32_t reg = offset + i;
		if (reg >= 0x2644 && reg < 0x2644 + 32) {		/* GB_TILE_MODE0-31 */
			memset(words, 0, sizeof(words));
			configWords(v, "TileModes", words, 32);
			out[i] = words[reg - 0x2644];
		} else if (reg == 0x263e)				/* GB_ADDR_CONFIG */
			out[i] = configNumber(v, "AddrConfig");
		else if (reg == 0x263d) {				/* CC_RB_BACKEND_DISABLE */
			memset(words, 0, sizeof(words));
			configWords(v, "BackendDisable", words, 4);
			out[i] = words[se & 3];
		} else if (reg == 0xa0d4)				/* PA_SC_RASTER_CONFIG */
			out[i] = configNumber(v, "RasterConfig");
		else if (reg == 0x9d8)					/* MC_ARB_RAMCFG */
			out[i] = configNumber(v, "MCArbRamcfg");
		else
			return -EINVAL;
	}
	return 0;
}

static int
info(int fd, struct drm_amdgpu_info *req)
{
	struct gpu_view v;
	void *ret = (void *)(uintptr_t)req->return_pointer;
	uint32_t size = req->return_size;
	int err = 0;

	if (!viewFor(fd, &v))
		return -ENODEV;
	switch (req->query) {
	case AMDGPU_INFO_DEV_INFO: {
		struct drm_amdgpu_info_device d;
		err = devInfo(&v, &d);
		memcpy(ret, &d, size < sizeof(d) ? size : sizeof(d));
		break;
	}
	case AMDGPU_INFO_MEMORY: {
		struct drm_amdgpu_memory_info m;
		uint64_t total = 0, available = 0, window = 0;
		err = vram(&v, &total, &available, &window);
		memset(&m, 0, sizeof(m));
		m.vram.total_heap_size = m.vram.usable_heap_size = total;
		m.vram.heap_usage = total - available;
		m.vram.max_allocation = window;
		m.cpu_accessible_vram.total_heap_size = m.cpu_accessible_vram.usable_heap_size = window;
		m.cpu_accessible_vram.max_allocation = window;
		m.gtt.total_heap_size = m.gtt.usable_heap_size = m.gtt.max_allocation = gartBytes(&v);
		memcpy(ret, &m, size < sizeof(m) ? size : sizeof(m));
		break;
	}
	case AMDGPU_INFO_VRAM_GTT: {
		struct drm_amdgpu_info_vram_gtt g;
		uint64_t total = 0, available = 0, window = 0;
		err = vram(&v, &total, &available, &window);
		g.vram_size = total;
		g.vram_cpu_accessible_size = window;
		g.gtt_size = gartBytes(&v);
		memcpy(ret, &g, size < sizeof(g) ? size : sizeof(g));
		break;
	}
	case AMDGPU_INFO_VRAM_USAGE:
	case AMDGPU_INFO_VIS_VRAM_USAGE: {
		uint64_t total = 0, available = 0, window = 0, used;
		err = vram(&v, &total, &available, &window);
		used = total - available;
		memcpy(ret, &used, size < sizeof(used) ? size : sizeof(used));
		break;
	}
	case AMDGPU_INFO_GTT_USAGE: {
		uint64_t used = 0;
		memcpy(ret, &used, size < sizeof(used) ? size : sizeof(used));
		break;
	}
	case AMDGPU_INFO_FW_VERSION: {
		struct drm_amdgpu_info_firmware f = { 0, 0 };
		switch (req->query_fw.fw_type) {
		case AMDGPU_INFO_FW_GFX_ME:	f.ver = microcodeVersion("me"); break;
		case AMDGPU_INFO_FW_GFX_PFP:	f.ver = microcodeVersion("pfp"); break;
		case AMDGPU_INFO_FW_GFX_CE:	f.ver = microcodeVersion("ce"); break;
		default:			break;	/* none: no MEC, UVD, VCE */
		}
		memcpy(ret, &f, size < sizeof(f) ? size : sizeof(f));
		break;
	}
	case AMDGPU_INFO_HW_IP_INFO: {
		struct drm_amdgpu_info_hw_ip ip;
		memset(&ip, 0, sizeof(ip));
		if (req->query_hw_ip.type == AMDGPU_HW_IP_GFX && req->query_hw_ip.ip_instance == 0) {
			ip.hw_ip_version_major = 6;
			ip.hw_ip_version_minor = 0;
			ip.ib_start_alignment = 32;
			ip.ib_size_alignment = 32;
			ip.available_rings = 1;
		}
		memcpy(ret, &ip, size < sizeof(ip) ? size : sizeof(ip));
		break;
	}
	case AMDGPU_INFO_HW_IP_COUNT: {
		uint32_t count = req->query_hw_ip.type == AMDGPU_HW_IP_GFX ? 1 : 0;
		memcpy(ret, &count, size < sizeof(count) ? size : sizeof(count));
		break;
	}
	case AMDGPU_INFO_READ_MMR_REG:
		if (size < req->read_mmr_reg.count * 4u)
			err = -EINVAL;
		else
			err = readRegisters(&v, req->read_mmr_reg.dword_offset, req->read_mmr_reg.count,
			    req->read_mmr_reg.instance, ret);
		break;
	case AMDGPU_INFO_MAX_IBS: {
		uint32_t ibs[AMDGPU_HW_IP_NUM];
		memset(ibs, 0, sizeof(ibs));
		/* The submission runs its IBs one after another, so any number
		 * fits; amdgpu's own limit for GFX6 (as Mesa assumes it). */
		ibs[AMDGPU_HW_IP_GFX] = 144;
		memcpy(ret, ibs, size < sizeof(ibs) ? size : sizeof(ibs));
		break;
	}
	default:
		err = -EINVAL;		/* sensors, faults, video: not here */
		break;
	}
	viewRelease(&v);
	return err;
}

int
minkar_drm_command(int fd, unsigned long index, void *data, unsigned long size)
{
	static int trace = -1;
	int err;

	if (index == DRM_AMDGPU_INFO)
		err = size >= sizeof(struct drm_amdgpu_info) ? info(fd, data) : -EINVAL;
	else
		err = minkar_drm_amdgpu_command(fd, index, data, size);
	/* MINKAR_DRM_DEBUG=1: each command, its first word (the operation or
	 * the query), and what it gave, on standard error. */
	if (trace < 0)
		trace = getenv("MINKAR_DRM_DEBUG") != NULL;
	if (trace) {
		uint32_t first = 0;
		if (index == DRM_AMDGPU_INFO && size >= sizeof(struct drm_amdgpu_info))
			first = ((struct drm_amdgpu_info *)data)->query;
		else if (data != NULL && size >= sizeof(first))
			memcpy(&first, data, sizeof(first));
		fprintf(stderr, "libdrm: fd %d command 0x%02lx (0x%x), %lu bytes: %d\n", fd, index, first,
		    size, err);
	}
	return err;
}
