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
 * minkar-vkinfo: Vulkan as an app uses it, through the loader
 * (/usr/lib/libvulkan.1.dylib), which finds the drivers by
 * /usr/share/vulkan/icd.d: the instance's version and surface extensions,
 * each device (name, driver, API version, VK_KHR_swapchain), and a buffer
 * filled by the first device's GPU, through the loader's dispatch.
 */

#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>

static int failures;

static void
check(int good, const char *what, const char *detail)
{
	printf("%s %s%s%s\n", good ? "ok  " : "FAIL", what, detail[0] ? ": " : "", detail);
	if (!good)
		failures++;
}

static int
hasExtension(const VkExtensionProperties *e, uint32_t n, const char *name)
{
	for (uint32_t i = 0; i < n; i++)
		if (strcmp(e[i].extensionName, name) == 0)
			return 1;
	return 0;
}

int
main(void)
{
	char detail[512];
	uint32_t version = 0;
	VkResult r;

	setvbuf(stdout, NULL, _IOLBF, 0);
	vkEnumerateInstanceVersion(&version);
	VkExtensionProperties iext[64];
	uint32_t in = 64;
	vkEnumerateInstanceExtensionProperties(NULL, &in, iext);
	snprintf(detail, sizeof(detail), "Vulkan %u.%u.%u, %u extensions", VK_VERSION_MAJOR(version),
	    VK_VERSION_MINOR(version), VK_VERSION_PATCH(version), in);
	check(version >= VK_API_VERSION_1_3 && hasExtension(iext, in, "VK_EXT_metal_surface"),
	    "the loader: its version, and VK_EXT_metal_surface from the driver", detail);

	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "minkar-vkinfo", 1, NULL, 0,
	    VK_API_VERSION_1_3 };
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app, 0, NULL, 0, NULL };
	VkInstance inst;
	r = vkCreateInstance(&ici, NULL, &inst);
	snprintf(detail, sizeof(detail), "%d", r);
	check(r == VK_SUCCESS, "vkCreateInstance through the loader", detail);
	if (r != VK_SUCCESS)
		return failures;

	VkPhysicalDevice pd[8];
	uint32_t n = 8;
	vkEnumeratePhysicalDevices(inst, &n, pd);
	snprintf(detail, sizeof(detail), "%u", n);
	check(n > 0, "physical devices", detail);
	for (uint32_t i = 0; i < n; i++) {
		VkPhysicalDeviceDriverProperties drv = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES };
		VkPhysicalDeviceProperties2 p = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &drv };
		vkGetPhysicalDeviceProperties2(pd[i], &p);
		VkExtensionProperties dext[512];
		uint32_t dn = 512;
		vkEnumerateDeviceExtensionProperties(pd[i], NULL, &dn, dext);
		snprintf(detail, sizeof(detail), "%s; %s %s; Vulkan %u.%u.%u; conformance %u.%u.%u.%u; %u extensions",
		    p.properties.deviceName, drv.driverName, drv.driverInfo, VK_VERSION_MAJOR(p.properties.apiVersion),
		    VK_VERSION_MINOR(p.properties.apiVersion), VK_VERSION_PATCH(p.properties.apiVersion),
		    drv.conformanceVersion.major, drv.conformanceVersion.minor, drv.conformanceVersion.subminor,
		    drv.conformanceVersion.patch, dn);
		check(hasExtension(dext, dn, VK_KHR_SWAPCHAIN_EXTENSION_NAME), "a device, with VK_KHR_swapchain", detail);
	}

	/* A buffer the first device fills (the command processor's DMA). */
	float prio = 1.0f;
	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0, 0, 1, &prio };
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0, 1, &qci, 0, NULL, 0, NULL, NULL };
	VkDevice dev;
	r = vkCreateDevice(pd[0], &dci, NULL, &dev);
	snprintf(detail, sizeof(detail), "%d", r);
	check(r == VK_SUCCESS, "vkCreateDevice through the loader", detail);
	if (r != VK_SUCCESS)
		return failures;
	VkQueue q;
	vkGetDeviceQueue(dev, 0, 0, &q);
	VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, 1024,
	    VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL };
	VkBuffer buf;
	vkCreateBuffer(dev, &bci, NULL, &buf);
	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(dev, buf, &req);
	VkPhysicalDeviceMemoryProperties mp;
	vkGetPhysicalDeviceMemoryProperties(pd[0], &mp);
	uint32_t type = 0;
	const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	while (type < mp.memoryTypeCount && !((req.memoryTypeBits & (1u << type)) &&
	    (mp.memoryTypes[type].propertyFlags & host) == host))
		type++;
	VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, req.size, type };
	VkDeviceMemory mem;
	void *map = NULL;
	vkAllocateMemory(dev, &mai, NULL, &mem);
	vkBindBufferMemory(dev, buf, mem, 0);
	vkMapMemory(dev, mem, 0, VK_WHOLE_SIZE, 0, &map);
	volatile uint32_t *p = map;
	for (unsigned i = 0; i < 256; i++)
		p[i] = 0;
	VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, 0, 0 };
	VkCommandPool pool;
	vkCreateCommandPool(dev, &pci, NULL, &pool);
	VkCommandBufferAllocateInfo cai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, pool,
	    VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(dev, &cai, &cmd);
	VkCommandBufferBeginInfo begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0, NULL };
	vkBeginCommandBuffer(cmd, &begin);
	vkCmdFillBuffer(cmd, buf, 0, 1024, 0x10ADE411);
	vkEndCommandBuffer(cmd);
	VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, 1, &cmd, 0, NULL };
	r = vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE);
	vkQueueWaitIdle(q);
	unsigned bad = 0;
	for (unsigned i = 0; i < 256; i++)
		bad += p[i] != 0x10ADE411;
	snprintf(detail, sizeof(detail), "%d, %u of 256 words wrong", r, bad);
	check(r == VK_SUCCESS && bad == 0, "the GPU filled a buffer, through the loader", detail);
	vkDestroyDevice(dev, NULL);
	vkDestroyInstance(inst, NULL);
	return failures;
}
