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
 * minkar-drmtest: Minkar's libdrm as Mesa's RADV starts on it: the DRM
 * devices, each render node opened, the amdgpu device on it, and what
 * ac_query_gpu_info asks (the device's information, its memory, the
 * microcode's versions, the graphics block, the registers it reads).
 * Prints "ok"/"FAIL" lines and the values; exit status: the failures.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>
#include <amdgpu.h>
#include <amdgpu_drm.h>

static int failures;

static void
check(int good, const char *what, const char *detail)
{
	printf("%s %s%s%s\n", good ? "ok  " : "FAIL", what, detail[0] ? ": " : "", detail);
	if (!good)
		failures++;
}

static int
query(int fd, uint32_t q, void *out, uint32_t size, struct drm_amdgpu_info *extra)
{
	struct drm_amdgpu_info r;
	if (extra != NULL)
		r = *extra;
	else
		memset(&r, 0, sizeof(r));
	r.return_pointer = (uint64_t)(uintptr_t)out;
	r.return_size = size;
	r.query = q;
	return drmCommandWrite(fd, DRM_AMDGPU_INFO, &r, sizeof(r));
}

int
main(void)
{
	drmDevicePtr devices[8];
	char detail[256];
	int n = drmGetDevices2(0, devices, 8);

	snprintf(detail, sizeof(detail), "%d", n);
	check(n > 0, "drmGetDevices2 lists the GPUs", detail);
	for (int i = 0; i < n; i++) {
		drmDevicePtr d = devices[i];
		const char *node = d->nodes[DRM_NODE_RENDER];
		snprintf(detail, sizeof(detail), "%s, PCI %02x:%02x.%x, %04x:%04x rev %02x", node,
		    d->businfo.pci->bus, d->businfo.pci->dev, d->businfo.pci->func,
		    d->deviceinfo.pci->vendor_id, d->deviceinfo.pci->device_id,
		    d->deviceinfo.pci->revision_id);
		check(d->available_nodes & (1 << DRM_NODE_RENDER), "a render node", detail);
		int fd = open(node, O_RDWR | O_CLOEXEC);
		snprintf(detail, sizeof(detail), "%s: %s", node, fd < 0 ? strerror(errno) : "");
		check(fd >= 0, "it opens", fd < 0 ? detail : "");
		if (fd < 0)
			continue;
		drmDevicePtr back = NULL;
		check(drmGetDevice2(fd, 0, &back) == 0 && back != NULL && drmDevicesEqual(back, d),
		    "the descriptor finds its device", "");
		drmFreeDevice(&back);
		drmVersionPtr v = drmGetVersion(fd);
		snprintf(detail, sizeof(detail), "%s %d.%d", v ? v->name : "?", v ? v->version_major : 0,
		    v ? v->version_minor : 0);
		check(v != NULL && v->version_major == 3 && v->version_minor >= 54,
		    "the driver's version, as RADV wants it", detail);
		drmFreeVersion(v);
		uint32_t major = 0, minor = 0;
		amdgpu_device_handle dev = NULL;
		int r = amdgpu_device_initialize(fd, &major, &minor, &dev);
		snprintf(detail, sizeof(detail), "%d", r);
		check(r == 0, "amdgpu_device_initialize", r ? detail : "");

		struct drm_amdgpu_info_device info;
		memset(&info, 0, sizeof(info));
		r = query(fd, AMDGPU_INFO_DEV_INFO, &info, sizeof(info), NULL);
		snprintf(detail, sizeof(detail), "%d: device %04x rev %u/%u family %u, %u SE x %u SH, "
		    "%u CUs (%u a SH), %u RBs (0x%x), VRAM %u bits, %llu-%llu MHz",
		    r, info.device_id, info.chip_rev, info.external_rev, info.family,
		    info.num_shader_engines, info.num_shader_arrays_per_engine, info.cu_active_number,
		    info.num_cu_per_sh, info.num_rb_pipes, info.enabled_rb_pipes_mask,
		    info.vram_bit_width, (unsigned long long)info.max_engine_clock / 1000,
		    (unsigned long long)info.max_memory_clock / 1000);
		check(r == 0 && info.family == AMDGPU_FAMILY_SI && info.cu_active_number > 0 &&
		    info.num_rb_pipes > 0, "DEV_INFO", detail);

		struct drm_amdgpu_memory_info mem;
		r = query(fd, AMDGPU_INFO_MEMORY, &mem, sizeof(mem), NULL);
		snprintf(detail, sizeof(detail), "%d: VRAM %llu MB (%llu MB used), window %llu MB, "
		    "GTT %llu MB", r, (unsigned long long)mem.vram.total_heap_size >> 20,
		    (unsigned long long)mem.vram.heap_usage >> 20,
		    (unsigned long long)mem.cpu_accessible_vram.total_heap_size >> 20,
		    (unsigned long long)mem.gtt.total_heap_size >> 20);
		check(r == 0 && mem.vram.total_heap_size > 0, "MEMORY", detail);

		struct drm_amdgpu_info_firmware fw[2];
		struct drm_amdgpu_info x;
		memset(&x, 0, sizeof(x));
		x.query_fw.fw_type = AMDGPU_INFO_FW_GFX_ME;
		int r1 = query(fd, AMDGPU_INFO_FW_VERSION, &fw[0], sizeof(fw[0]), &x);
		x.query_fw.fw_type = AMDGPU_INFO_FW_GFX_PFP;
		int r2 = query(fd, AMDGPU_INFO_FW_VERSION, &fw[1], sizeof(fw[1]), &x);
		snprintf(detail, sizeof(detail), "ME %u, PFP %u", fw[0].ver, fw[1].ver);
		check(r1 == 0 && r2 == 0 && fw[0].ver != 0 && fw[1].ver != 0, "the microcode's versions",
		    detail);

		struct drm_amdgpu_info_hw_ip ip;
		memset(&x, 0, sizeof(x));
		x.query_hw_ip.type = AMDGPU_HW_IP_GFX;
		r = query(fd, AMDGPU_INFO_HW_IP_INFO, &ip, sizeof(ip), &x);
		snprintf(detail, sizeof(detail), "%d: GFX %u.%u, %u ring(s)", r, ip.hw_ip_version_major,
		    ip.hw_ip_version_minor, ip.available_rings);
		check(r == 0 && ip.available_rings == 1, "HW_IP_INFO", detail);

		uint32_t regs[32], addr = 0, ramcfg = 0, rb = 0, raster = 0;
		memset(&x, 0, sizeof(x));
		x.read_mmr_reg.dword_offset = 0x263e;
		x.read_mmr_reg.count = 1;
		x.read_mmr_reg.instance = 0xffffffff;
		int a = query(fd, AMDGPU_INFO_READ_MMR_REG, &addr, 4, &x);
		x.read_mmr_reg.dword_offset = 0x2644;
		x.read_mmr_reg.count = 32;
		int b = query(fd, AMDGPU_INFO_READ_MMR_REG, regs, sizeof(regs), &x);
		x.read_mmr_reg.dword_offset = 0x9d8;
		x.read_mmr_reg.count = 1;
		int c = query(fd, AMDGPU_INFO_READ_MMR_REG, &ramcfg, 4, &x);
		x.read_mmr_reg.dword_offset = 0x263d;
		x.read_mmr_reg.instance = (0u << AMDGPU_INFO_MMR_SE_INDEX_SHIFT) |
		    (AMDGPU_INFO_MMR_SH_INDEX_MASK << AMDGPU_INFO_MMR_SH_INDEX_SHIFT);
		int e = query(fd, AMDGPU_INFO_READ_MMR_REG, &rb, 4, &x);
		x.read_mmr_reg.dword_offset = 0xa0d4;
		int f = query(fd, AMDGPU_INFO_READ_MMR_REG, &raster, 4, &x);
		snprintf(detail, sizeof(detail), "GB_ADDR_CONFIG 0x%08x, tile 0 0x%08x 8 0x%08x, "
		    "MC_ARB_RAMCFG 0x%08x, RB disable 0x%08x, raster 0x%08x", addr, regs[0], regs[8],
		    ramcfg, rb, raster);
		check(a == 0 && b == 0 && c == 0 && e == 0 && f == 0 && addr != 0 && regs[8] == 4,
		    "the registers RADV reads", detail);

		/* A buffer through libdrm_amdgpu, written by the CPU. */
		struct amdgpu_bo_alloc_request req = { 1 << 20, 4096, AMDGPU_GEM_DOMAIN_VRAM, 0 };
		amdgpu_bo_handle bo = NULL;
		void *cpu = NULL;
		r = dev != NULL ? amdgpu_bo_alloc(dev, &req, &bo) : -ENODEV;
		int m = r == 0 ? amdgpu_bo_cpu_map(bo, &cpu) : r;
		if (m == 0)
			((volatile uint32_t *)cpu)[0] = 0x12345678;
		snprintf(detail, sizeof(detail), "%d %d", r, m);
		check(r == 0 && m == 0, "a 1 MB VRAM buffer, mapped", detail);
		if (bo != NULL) {
			amdgpu_bo_cpu_unmap(bo);
			amdgpu_bo_free(bo);
		}
		uint64_t va = 0;
		amdgpu_va_handle vah = NULL;
		r = dev != NULL ? amdgpu_va_range_alloc(dev, amdgpu_gpu_va_range_general, 1 << 20, 4096, 0,
		    &va, &vah, 0) : -ENODEV;
		snprintf(detail, sizeof(detail), "%d: 0x%llx", r, (unsigned long long)va);
		check(r == 0 && va >= (4ull << 30), "a GPU virtual address range", detail);
		if (vah != NULL)
			amdgpu_va_range_free(vah);
		if (dev != NULL)
			amdgpu_device_deinitialize(dev);
		close(fd);
	}
	drmFreeDevices(devices, n);
	return failures;
}
