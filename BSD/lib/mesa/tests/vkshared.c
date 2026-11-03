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
 * minkar-vkshared: a buffer shared across processes, as the window server
 * uses one for its frames: made through MinkarAMDUserClient
 * (CreateSharedBuffer) in VRAM, imported into Vulkan by its descriptor
 * (minkar_drm_shared_buffer_fd, a dma-buf to VK_KHR_external_memory_fd),
 * filled by the GPU through Vulkan, and read through the maker's own CPU
 * mapping.
 */

#include <stdio.h>
#include <string.h>
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <vulkan/vulkan.h>
#include <libdrm/minkar_drm.h>
#include "MinkarAMDGPUInterface.h"

static int failures;

static void
check(int good, const char *what, const char *detail)
{
	printf("%s %s%s%s\n", good ? "ok  " : "FAIL", what, detail[0] ? ": " : "", detail);
	if (!good)
		failures++;
}

/* The MinkarAMDGPU service behind a render node. */
static io_service_t
gpuForMinor(uint32_t minor)
{
	io_iterator_t it;
	io_service_t s, found = IO_OBJECT_NULL;

	if (IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("MinkarAMDGPU"), &it) !=
	    KERN_SUCCESS)
		return IO_OBJECT_NULL;
	while ((s = IOIteratorNext(it)) != IO_OBJECT_NULL) {
		uint32_t m = 0;
		CFTypeRef v = IORegistryEntryCreateCFProperty(s, CFSTR("MinkarDRMMinor"), kCFAllocatorDefault, 0);
		if (v != NULL && CFGetTypeID(v) == CFNumberGetTypeID())
			CFNumberGetValue(v, kCFNumberSInt32Type, &m);
		if (v != NULL)
			CFRelease(v);
		if (m == minor && found == IO_OBJECT_NULL)
			found = s;
		else
			IOObjectRelease(s);
	}
	IOObjectRelease(it);
	return found;
}

int
main(void)
{
	char detail[512];
	VkResult r;

	setvbuf(stdout, NULL, _IOLBF, 0);
	const char *iext[] = { VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME };
	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "minkar-vkshared", 1, NULL, 0,
	    VK_API_VERSION_1_3 };
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app, 0, NULL, 1, iext };
	VkInstance inst;
	r = vkCreateInstance(&ici, NULL, &inst);
	if (r != VK_SUCCESS) {
		snprintf(detail, sizeof(detail), "%d", r);
		check(0, "an instance", detail);
		return failures;
	}
	VkPhysicalDevice pd;
	uint32_t n = 1;
	vkEnumeratePhysicalDevices(inst, &n, &pd);

	/* Its render node, to find the same GPU's user client. */
	VkPhysicalDeviceDrmPropertiesEXT drm = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT };
	VkPhysicalDeviceProperties2 props = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
	    .pNext = &drm };
	vkGetPhysicalDeviceProperties2(pd, &props);
	snprintf(detail, sizeof(detail), "render node %lld", (long long)drm.renderMinor);
	check(drm.hasRender, "the device's render node (VK_EXT_physical_device_drm)", detail);
	io_service_t service = gpuForMinor((uint32_t)drm.renderMinor);
	io_connect_t conn = IO_OBJECT_NULL;
	if (service != IO_OBJECT_NULL)
		IOServiceOpen(service, mach_task_self(), 0, &conn);
	check(conn != IO_OBJECT_NULL, "its GPU's user client", "");
	if (conn == IO_OBJECT_NULL)
		return failures;

	/* A shared VRAM buffer, and the maker's CPU view of it. */
	const uint64_t bytes = 1 << 20;
	uint64_t in = bytes, out[3] = { 0, 0, 0 };
	uint32_t on = 3;
	kern_return_t kr = IOConnectCallScalarMethod(conn, kMinkarAMDGPUCreateSharedBuffer, &in, 1, out, &on);
	mach_vm_address_t cpu = 0;
	mach_vm_size_t cpuSize = 0;
	if (kr == KERN_SUCCESS)
		kr = IOConnectMapMemory64(conn, (uint32_t)out[0], mach_task_self(), &cpu, &cpuSize, kIOMapAnywhere);
	snprintf(detail, sizeof(detail), "0x%x, name 0x%016llx", kr, (unsigned long long)out[2]);
	check(kr == KERN_SUCCESS, "a shared VRAM buffer, mapped for the CPU", detail);
	if (kr != KERN_SUCCESS)
		return failures;
	volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)cpu;
	for (unsigned i = 0; i < bytes / 4; i++)
		p[i] = 0;

	/* Vulkan: a device, the buffer imported by its descriptor. */
	float prio = 1;
	const char *dext[] = { VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME };
	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0, 0, 1, &prio };
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0, 1, &qci, 0, NULL, 2, dext, NULL };
	VkDevice dev;
	r = vkCreateDevice(pd, &dci, NULL, &dev);
	snprintf(detail, sizeof(detail), "%d", r);
	check(r == VK_SUCCESS, "a device with VK_KHR_external_memory_fd and dma-bufs", detail);
	if (r != VK_SUCCESS)
		return failures;
	PFN_vkGetMemoryFdPropertiesKHR getFdProps =
	    (PFN_vkGetMemoryFdPropertiesKHR)vkGetDeviceProcAddr(dev, "vkGetMemoryFdPropertiesKHR");
	/* Its name cannot be guessed: no small number (the IDs that were
	 * handed out in turn before) names a buffer, and a second buffer's
	 * name is nothing like the first's. */
	unsigned held = 0;
	for (uint64_t guess = 1; guess <= 4096; guess++) {
		uint64_t gout[3];
		uint32_t gn = 3;
		if (IOConnectCallScalarMethod(conn, kMinkarAMDGPURetainSharedBuffer, &guess, 1, gout, &gn) ==
		    KERN_SUCCESS) {
			held++;
			IOConnectCallScalarMethod(conn, kMinkarAMDGPUDestroyBuffer, &gout[0], 1, NULL, NULL);
		}
	}
	snprintf(detail, sizeof(detail), "%u of 4096 held", held);
	check(held == 0, "no shared buffer is held by guessing names 1 to 4096", detail);
	uint64_t in2 = 4096, out2[3] = { 0, 0, 0 };
	uint32_t on2 = 3;
	kr = IOConnectCallScalarMethod(conn, kMinkarAMDGPUCreateSharedBuffer, &in2, 1, out2, &on2);
	uint64_t apart = out2[2] > out[2] ? out2[2] - out[2] : out[2] - out2[2];
	snprintf(detail, sizeof(detail), "0x%016llx and 0x%016llx", (unsigned long long)out[2],
	    (unsigned long long)out2[2]);
	check(kr == KERN_SUCCESS && apart > 0xffffffffull, "two buffers' names are random, not in turn",
	    detail);
	if (kr == KERN_SUCCESS)
		IOConnectCallScalarMethod(conn, kMinkarAMDGPUDestroyBuffer, &out2[0], 1, NULL, NULL);
	uint64_t rin = out[2], rout[3];
	uint32_t rn = 3;
	kr = IOConnectCallScalarMethod(conn, kMinkarAMDGPURetainSharedBuffer, &rin, 1, rout, &rn);
	check(kr == KERN_SUCCESS, "the name its maker gives out holds it", "");
	if (kr == KERN_SUCCESS)
		IOConnectCallScalarMethod(conn, kMinkarAMDGPUDestroyBuffer, &rout[0], 1, NULL, NULL);

	int fd = minkar_drm_shared_buffer_fd(out[2]);
	VkMemoryFdPropertiesKHR fdProps = { .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR };
	r = getFdProps(dev, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd, &fdProps);
	snprintf(detail, sizeof(detail), "descriptor %d, %d, memory types 0x%x", fd, r, fdProps.memoryTypeBits);
	check(fd >= 0 && r == VK_SUCCESS && fdProps.memoryTypeBits != 0, "the buffer's descriptor and its memory types",
	    detail);

	VkExternalMemoryBufferCreateInfo ext = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, NULL,
	    VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT };
	VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &ext, 0, bytes,
	    VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL };
	VkBuffer buf;
	vkCreateBuffer(dev, &bci, NULL, &buf);
	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(dev, buf, &req);
	uint32_t type = 0;
	while (type < 32 && !((fdProps.memoryTypeBits & req.memoryTypeBits) & (1u << type)))
		type++;
	VkImportMemoryFdInfoKHR import = { VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, NULL,
	    VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd };
	VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &import, bytes, type };
	VkDeviceMemory mem;
	r = vkAllocateMemory(dev, &mai, NULL, &mem);
	VkResult rb = r == VK_SUCCESS ? vkBindBufferMemory(dev, buf, mem, 0) : r;
	snprintf(detail, sizeof(detail), "%d %d, memory type %u", r, rb, type);
	check(rb == VK_SUCCESS, "Vulkan memory imported from the descriptor, a buffer bound to it", detail);
	if (rb != VK_SUCCESS)
		return failures;

	VkQueue q;
	vkGetDeviceQueue(dev, 0, 0, &q);
	VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, 0, 0 };
	VkCommandPool pool;
	vkCreateCommandPool(dev, &pci, NULL, &pool);
	VkCommandBufferAllocateInfo cai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, pool,
	    VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(dev, &cai, &cmd);
	VkCommandBufferBeginInfo begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0, NULL };
	vkBeginCommandBuffer(cmd, &begin);
	vkCmdFillBuffer(cmd, buf, 0, bytes, 0x5AA5F00D);
	vkEndCommandBuffer(cmd);
	VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, 1, &cmd, 0, NULL };
	r = vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE);
	vkQueueWaitIdle(q);
	unsigned bad = 0;
	for (unsigned i = 0; i < bytes / 4; i++)
		bad += p[i] != 0x5AA5F00D;
	snprintf(detail, sizeof(detail), "%d, %u of %u words wrong; first 0x%08x", r, bad, (unsigned)(bytes / 4),
	    p[0]);
	check(r == VK_SUCCESS && bad == 0, "the GPU filled it through Vulkan; the maker reads it", detail);
	vkDestroyBuffer(dev, buf, NULL);
	vkFreeMemory(dev, mem, NULL);
	vkDestroyDevice(dev, NULL);
	vkDestroyInstance(inst, NULL);
	return failures;
}
