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
 * minkar-vktest: Vulkan on Minkar through Mesa's RADV, loaded as an ICD
 * without a loader (opened lazily, as the Vulkan loader does): an instance, the physical devices, a logical device,
 * then a buffer the GPU fills (vkCmdFillBuffer, submitted with a fence)
 * and the CPU reads back.
 *
 *   minkar-vktest [ICD]	(default /usr/lib/libvulkan_radeon.dylib)
 */

#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>

typedef PFN_vkVoidFunction (VKAPI_PTR *PFN_icdGetInstanceProcAddr)(VkInstance, const char *);
typedef VkResult (VKAPI_PTR *PFN_icdNegotiate)(uint32_t *);

static int failures;

/* A crash inside the driver: where, before the process goes. */
static void
crashed(int sig)
{
	void *frames[64];
	int n = backtrace(frames, 64);
	dprintf(2, "FAIL signal %d, at:\n", sig);
	backtrace_symbols_fd(frames, n, 2);
	_exit(128 + sig);
}

static void
check(int good, const char *what, const char *detail)
{
	printf("%s %s%s%s\n", good ? "ok  " : "FAIL", what, detail[0] ? ": " : "", detail);
	if (!good)
		failures++;
}

#define GET(inst, name) PFN_##name name = (PFN_##name)gipa(inst, #name)
#define DEV(dev, name) PFN_##name name = (PFN_##name)vkGetDeviceProcAddr(dev, #name)

int
main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IOLBF, 0);
	signal(SIGSEGV, crashed);
	signal(SIGBUS, crashed);
	signal(SIGILL, crashed);
	signal(SIGABRT, crashed);
	const char *path = argc > 1 ? argv[1] : "/usr/lib/libvulkan_radeon.dylib";
	char detail[512];

	void *icd = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
	snprintf(detail, sizeof(detail), "%s%s%s", path, icd ? "" : ": ", icd ? "" : dlerror());
	check(icd != NULL, "the ICD loads", detail);
	if (icd == NULL)
		return failures;
	PFN_icdNegotiate negotiate = (PFN_icdNegotiate)dlsym(icd, "vk_icdNegotiateLoaderICDInterfaceVersion");
	PFN_icdGetInstanceProcAddr gipa = (PFN_icdGetInstanceProcAddr)dlsym(icd, "vk_icdGetInstanceProcAddr");
	uint32_t version = 5;
	VkResult r = negotiate != NULL ? negotiate(&version) : VK_ERROR_INITIALIZATION_FAILED;
	snprintf(detail, sizeof(detail), "%d, interface %u", r, version);
	check(r == VK_SUCCESS && gipa != NULL, "the loader interface", detail);
	if (gipa == NULL)
		return failures;

	GET(NULL, vkCreateInstance);
	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "minkar-vktest", 1, NULL, 0,
	    VK_API_VERSION_1_1 };
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app, 0, NULL, 0, NULL };
	VkInstance inst = VK_NULL_HANDLE;
	r = vkCreateInstance(&ici, NULL, &inst);
	snprintf(detail, sizeof(detail), "%d", r);
	check(r == VK_SUCCESS, "vkCreateInstance", detail);
	if (r != VK_SUCCESS)
		return failures;
	GET(inst, vkEnumeratePhysicalDevices);
	GET(inst, vkGetPhysicalDeviceProperties);
	GET(inst, vkGetPhysicalDeviceMemoryProperties);
	GET(inst, vkGetPhysicalDeviceQueueFamilyProperties);
	GET(inst, vkCreateDevice);
	GET(inst, vkGetDeviceProcAddr);
	GET(inst, vkDestroyInstance);

	VkPhysicalDevice pd[8];
	uint32_t n = 8;
	r = vkEnumeratePhysicalDevices(inst, &n, pd);
	snprintf(detail, sizeof(detail), "%d, %u", r, n);
	check(r == VK_SUCCESS && n > 0, "vkEnumeratePhysicalDevices", detail);
	for (uint32_t i = 0; i < n; i++) {
		VkPhysicalDeviceProperties p;
		VkPhysicalDeviceMemoryProperties m;
		vkGetPhysicalDeviceProperties(pd[i], &p);
		vkGetPhysicalDeviceMemoryProperties(pd[i], &m);
		unsigned long long heap = m.memoryHeapCount > 0 ? m.memoryHeaps[0].size >> 20 : 0;
		snprintf(detail, sizeof(detail), "%s, Vulkan %u.%u.%u, driver 0x%x, %04x:%04x, type %d, "
		    "%u heaps (first %llu MB)", p.deviceName, VK_VERSION_MAJOR(p.apiVersion),
		    VK_VERSION_MINOR(p.apiVersion), VK_VERSION_PATCH(p.apiVersion), p.driverVersion,
		    p.vendorID, p.deviceID, p.deviceType, m.memoryHeapCount, heap);
		check(p.vendorID == 0x1002, "a physical device", detail);
	}
	if (n == 0)
		return failures;

	/* The first device: a graphics queue, a host-visible buffer. */
	uint32_t qn = 8, family = UINT32_MAX;
	VkQueueFamilyProperties qf[8];
	vkGetPhysicalDeviceQueueFamilyProperties(pd[0], &qn, qf);
	for (uint32_t i = 0; i < qn && family == UINT32_MAX; i++)
		if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			family = i;
	float prio = 1.0f;
	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0, family, 1, &prio };
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0, 1, &qci, 0, NULL, 0, NULL,
	    NULL };
	VkDevice dev = VK_NULL_HANDLE;
	r = family != UINT32_MAX ? vkCreateDevice(pd[0], &dci, NULL, &dev) : VK_ERROR_FEATURE_NOT_PRESENT;
	snprintf(detail, sizeof(detail), "%d, graphics queue family %u", r, family);
	check(r == VK_SUCCESS, "vkCreateDevice", detail);
	if (r != VK_SUCCESS)
		return failures;
	DEV(dev, vkGetDeviceQueue);
	DEV(dev, vkCreateBuffer);
	DEV(dev, vkGetBufferMemoryRequirements);
	DEV(dev, vkAllocateMemory);
	DEV(dev, vkBindBufferMemory);
	DEV(dev, vkMapMemory);
	DEV(dev, vkCreateCommandPool);
	DEV(dev, vkAllocateCommandBuffers);
	DEV(dev, vkBeginCommandBuffer);
	DEV(dev, vkCmdFillBuffer);
	DEV(dev, vkCmdUpdateBuffer);
	DEV(dev, vkCmdCopyBuffer);
	DEV(dev, vkCmdPipelineBarrier);
	DEV(dev, vkEndCommandBuffer);
	DEV(dev, vkCreateFence);
	DEV(dev, vkQueueSubmit);
	DEV(dev, vkWaitForFences);
	DEV(dev, vkDestroyDevice);
	VkQueue q;
	vkGetDeviceQueue(dev, family, 0, &q);

	VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, 65536,
	    VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL };
	VkBuffer buf;
	r = vkCreateBuffer(dev, &bci, NULL, &buf);
	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(dev, buf, &req);
	VkPhysicalDeviceMemoryProperties mp;
	vkGetPhysicalDeviceMemoryProperties(pd[0], &mp);
	uint32_t type = UINT32_MAX;
	for (uint32_t i = 0; i < mp.memoryTypeCount && type == UINT32_MAX; i++)
		if ((req.memoryTypeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags &
		    (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
		    (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			type = i;
	VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, req.size, type };
	VkDeviceMemory mem;
	VkResult ra = type != UINT32_MAX ? vkAllocateMemory(dev, &mai, NULL, &mem) : VK_ERROR_OUT_OF_DEVICE_MEMORY;
	VkResult rb = ra == VK_SUCCESS ? vkBindBufferMemory(dev, buf, mem, 0) : ra;
	volatile uint32_t *p = NULL;
	VkResult rm = rb == VK_SUCCESS ? vkMapMemory(dev, mem, 0, VK_WHOLE_SIZE, 0, (void **)&p) : rb;
	snprintf(detail, sizeof(detail), "%d %d %d %d, memory type %u", r, ra, rb, rm, type);
	check(r == VK_SUCCESS && rm == VK_SUCCESS, "a 64 KB host-visible buffer, mapped", detail);
	if (rm != VK_SUCCESS)
		return failures;
	for (unsigned i = 0; i < 16384; i++)
		p[i] = 0;

	/* A device-local buffer the CPU can write (visible VRAM), as RADV keeps
	 * its shaders: written here, copied by the GPU into the first. */
	VkBufferCreateInfo vci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, 4096,
	    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL };
	VkBuffer vbuf;
	vkCreateBuffer(dev, &vci, NULL, &vbuf);
	vkGetBufferMemoryRequirements(dev, vbuf, &req);
	uint32_t vtype = UINT32_MAX;
	const VkMemoryPropertyFlags vflags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	for (uint32_t i = 0; i < mp.memoryTypeCount && vtype == UINT32_MAX; i++)
		if ((req.memoryTypeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & vflags) == vflags)
			vtype = i;
	VkMemoryAllocateInfo vai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, req.size, vtype };
	VkDeviceMemory vmem;
	volatile uint32_t *vp = NULL;
	ra = vtype != UINT32_MAX ? vkAllocateMemory(dev, &vai, NULL, &vmem) : VK_ERROR_OUT_OF_DEVICE_MEMORY;
	rb = ra == VK_SUCCESS ? vkBindBufferMemory(dev, vbuf, vmem, 0) : ra;
	rm = rb == VK_SUCCESS ? vkMapMemory(dev, vmem, 0, VK_WHOLE_SIZE, 0, (void **)&vp) : rb;
	snprintf(detail, sizeof(detail), "%d %d %d, memory type %u", ra, rb, rm, vtype);
	check(rm == VK_SUCCESS, "a 4 KB device-local buffer, mapped", detail);
	if (rm != VK_SUCCESS)
		return failures;
	for (unsigned i = 0; i < 1024; i++)
		vp[i] = 0x7E570000 + i;

	/* A second device-local buffer the compute shader fills, copied back
	 * by DMA: whether the shader runs at all, apart from where it writes. */
	VkBufferCreateInfo fci2 = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, 4096,
	    VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0,
	    NULL };
	VkBuffer fbuf;
	VkDeviceMemory fmem;
	vkCreateBuffer(dev, &fci2, NULL, &fbuf);
	vkGetBufferMemoryRequirements(dev, fbuf, &req);
	vai.allocationSize = req.size;
	ra = vkAllocateMemory(dev, &vai, NULL, &fmem);
	rb = ra == VK_SUCCESS ? vkBindBufferMemory(dev, fbuf, fmem, 0) : ra;
	check(rb == VK_SUCCESS, "a second 4 KB device-local buffer", "");

	VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, 0, family };
	VkCommandPool pool;
	vkCreateCommandPool(dev, &pci, NULL, &pool);
	VkCommandBufferAllocateInfo cai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, pool,
	    VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(dev, &cai, &cmd);
	VkCommandBufferBeginInfo begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
	    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, NULL };
	vkBeginCommandBuffer(cmd, &begin);
	/* Three ways RADV writes a buffer: packets with the data
	 * (vkCmdUpdateBuffer), the command processor's DMA (a small fill), and
	 * a compute shader (a large fill). */
	uint32_t words[16];
	for (unsigned i = 0; i < 16; i++)
		words[i] = 0xDA7A0000 + i;
	vkCmdUpdateBuffer(cmd, buf, 0, sizeof(words), words);
	vkCmdFillBuffer(cmd, buf, 1024, 1024, 0xD3A11111);
	VkBufferCopy region = { 0, 2048, 512 };
	vkCmdCopyBuffer(cmd, vbuf, buf, 1, &region);
	vkCmdFillBuffer(cmd, buf, 4096, 65536 - 4096, 0xC0FFEE11);
	vkCmdFillBuffer(cmd, fbuf, 0, 4096, 0x5EADBEEF);
	VkMemoryBarrier mb = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL,
	    VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT,
	    VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT };
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
	    1, &mb, 0, NULL, 0, NULL);
	VkBufferCopy back = { 0, 3072, 512 };
	vkCmdCopyBuffer(cmd, fbuf, buf, 1, &back);
	r = vkEndCommandBuffer(cmd);
	check(r == VK_SUCCESS, "a command buffer recorded (update, small and large fills)", "");

	VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0 };
	VkFence fence;
	vkCreateFence(dev, &fci, NULL, &fence);
	VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, 1, &cmd, 0, NULL };
	r = vkQueueSubmit(q, 1, &si, fence);
	VkResult rw = r == VK_SUCCESS ? vkWaitForFences(dev, 1, &fence, VK_TRUE, 2000000000ull) : r;
	snprintf(detail, sizeof(detail), "%d %d", r, rw);
	check(r == VK_SUCCESS && rw == VK_SUCCESS, "submitted, and its fence signals", detail);
	unsigned bad = 0;
	for (unsigned i = 0; i < 16; i++)
		bad += p[i] != words[i];
	snprintf(detail, sizeof(detail), "%u of 16 dwords wrong; first 0x%08x", bad, p[0]);
	check(bad == 0, "vkCmdUpdateBuffer wrote its data", detail);
	bad = 0;
	for (unsigned i = 256; i < 512; i++)
		bad += p[i] != 0xD3A11111;
	snprintf(detail, sizeof(detail), "%u of 256 dwords wrong; first 0x%08x", bad, p[256]);
	check(bad == 0, "a 1 KB vkCmdFillBuffer (DMA)", detail);
	bad = 0;
	for (unsigned i = 0; i < 128; i++)
		bad += p[512 + i] != 0x7E570000 + i;
	snprintf(detail, sizeof(detail), "%u of 128 dwords wrong; first 0x%08x", bad, p[512]);
	check(bad == 0, "the GPU read what the CPU wrote to device-local memory", detail);
	bad = 0;
	for (unsigned i = 768; i < 896; i++)
		bad += p[i] != 0x5EADBEEF;
	snprintf(detail, sizeof(detail), "%u of 128 dwords wrong; first 0x%08x", bad, p[768]);
	check(bad == 0, "a 4 KB compute fill of device-local memory, copied back", detail);
	bad = 0;
	for (unsigned i = 1024; i < 16384; i++)
		bad += p[i] != 0xC0FFEE11;
	snprintf(detail, sizeof(detail), "%u of 15360 dwords wrong; first 0x%08x, last 0x%08x", bad,
	    p[1024], p[16383]);
	check(bad == 0, "a 60 KB vkCmdFillBuffer (compute shader)", detail);
	vkDestroyDevice(dev, NULL);
	vkDestroyInstance(inst, NULL);
	return failures;
}
