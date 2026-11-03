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
 * minkar-vkdraw: Vulkan drawing through RADV, off the screen. A 64x64 color
 * image is cleared to black by its render pass, a red triangle is drawn in
 * it by a graphics pipeline (SPIR-V shaders built from shaders/), and the
 * image is copied to a buffer the CPU reads: the triangle's inside is red,
 * the corners black, and the counts match the triangle's area.
 *
 *   minkar-vkdraw [ICD]	(default /usr/lib/libvulkan_radeon.dylib)
 */

#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vulkan/vulkan.h>
/* The SPIR-V glslang makes of shaders/ (arrays it defines, not declares). */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
#include "triangle_vert.h"
#include "triangle_frag.h"
#pragma clang diagnostic pop

#define SIZE 64

typedef PFN_vkVoidFunction (VKAPI_PTR *PFN_icdGetInstanceProcAddr)(VkInstance, const char *);
typedef VkResult (VKAPI_PTR *PFN_icdNegotiate)(uint32_t *);

static int failures;

static void
check(int good, const char *what, const char *detail)
{
	printf("%s %s%s%s\n", good ? "ok  " : "FAIL", what, detail[0] ? ": " : "", detail);
	if (!good)
		failures++;
}

static void
crashed(int sig)
{
	void *frames[64];
	int n = backtrace(frames, 64);
	dprintf(2, "FAIL signal %d, at:\n", sig);
	backtrace_symbols_fd(frames, n, 2);
	_exit(128 + sig);
}

#define GET(inst, name) PFN_##name name = (PFN_##name)gipa(inst, #name)
#define DEV(name) PFN_##name name = (PFN_##name)vkGetDeviceProcAddr(dev, #name)

int
main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "/usr/lib/libvulkan_radeon.dylib";
	char detail[512];
	VkResult r;

	setvbuf(stdout, NULL, _IOLBF, 0);
	signal(SIGSEGV, crashed);
	signal(SIGBUS, crashed);
	signal(SIGABRT, crashed);

	/* The driver, an instance, the first device and its graphics queue. */
	void *icd = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
	PFN_icdNegotiate negotiate = icd ? (PFN_icdNegotiate)dlsym(icd, "vk_icdNegotiateLoaderICDInterfaceVersion") : NULL;
	PFN_icdGetInstanceProcAddr gipa = icd ? (PFN_icdGetInstanceProcAddr)dlsym(icd, "vk_icdGetInstanceProcAddr") : NULL;
	uint32_t version = 5;
	if (negotiate == NULL || gipa == NULL || negotiate(&version) != VK_SUCCESS) {
		check(0, "the driver loads", icd ? "no loader interface" : dlerror());
		return failures;
	}
	GET(NULL, vkCreateInstance);
	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "minkar-vkdraw", 1, NULL, 0,
	    VK_API_VERSION_1_1 };
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app, 0, NULL, 0, NULL };
	VkInstance inst;
	r = vkCreateInstance(&ici, NULL, &inst);
	if (r != VK_SUCCESS) {
		snprintf(detail, sizeof(detail), "%d", r);
		check(0, "vkCreateInstance", detail);
		return failures;
	}
	GET(inst, vkEnumeratePhysicalDevices);
	GET(inst, vkGetPhysicalDeviceMemoryProperties);
	GET(inst, vkGetPhysicalDeviceQueueFamilyProperties);
	GET(inst, vkCreateDevice);
	GET(inst, vkGetDeviceProcAddr);
	VkPhysicalDevice pd;
	uint32_t n = 1;
	r = vkEnumeratePhysicalDevices(inst, &n, &pd);
	if ((r != VK_SUCCESS && r != VK_INCOMPLETE) || n == 0) {
		check(0, "a physical device", "none");
		return failures;
	}
	uint32_t qn = 8, family = UINT32_MAX;
	VkQueueFamilyProperties qf[8];
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, qf);
	for (uint32_t i = 0; i < qn && family == UINT32_MAX; i++)
		if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			family = i;
	float prio = 1.0f;
	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0, family, 1, &prio };
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0, 1, &qci, 0, NULL, 0, NULL, NULL };
	VkDevice dev;
	r = vkCreateDevice(pd, &dci, NULL, &dev);
	snprintf(detail, sizeof(detail), "%d", r);
	check(r == VK_SUCCESS, "a device with a graphics queue", detail);
	if (r != VK_SUCCESS)
		return failures;
	DEV(vkGetDeviceQueue);
	DEV(vkCreateImage);
	DEV(vkGetImageMemoryRequirements);
	DEV(vkBindImageMemory);
	DEV(vkCreateImageView);
	DEV(vkCreateBuffer);
	DEV(vkGetBufferMemoryRequirements);
	DEV(vkBindBufferMemory);
	DEV(vkAllocateMemory);
	DEV(vkMapMemory);
	DEV(vkCreateRenderPass);
	DEV(vkCreateFramebuffer);
	DEV(vkCreateShaderModule);
	DEV(vkCreatePipelineLayout);
	DEV(vkCreateGraphicsPipelines);
	DEV(vkCreateCommandPool);
	DEV(vkAllocateCommandBuffers);
	DEV(vkBeginCommandBuffer);
	DEV(vkCmdBeginRenderPass);
	DEV(vkCmdBindPipeline);
	DEV(vkCmdPushConstants);
	DEV(vkCmdDraw);
	DEV(vkCmdEndRenderPass);
	DEV(vkCmdCopyImageToBuffer);
	DEV(vkCmdPipelineBarrier);
	DEV(vkEndCommandBuffer);
	DEV(vkCreateFence);
	DEV(vkQueueSubmit);
	DEV(vkWaitForFences);
	DEV(vkDestroyDevice);
	VkQueue q;
	vkGetDeviceQueue(dev, family, 0, &q);
	VkPhysicalDeviceMemoryProperties mp;
	vkGetPhysicalDeviceMemoryProperties(pd, &mp);

	/* The color image (the driver's own tiling) in device-local memory. */
	const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
	VkImageCreateInfo imci = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0, VK_IMAGE_TYPE_2D, format,
	    { SIZE, SIZE, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
	    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, 0,
	    NULL, VK_IMAGE_LAYOUT_UNDEFINED };
	VkImage image;
	VkMemoryRequirements req;
	r = vkCreateImage(dev, &imci, NULL, &image);
	vkGetImageMemoryRequirements(dev, image, &req);
	uint32_t itype = UINT32_MAX;
	for (uint32_t i = 0; i < mp.memoryTypeCount && itype == UINT32_MAX; i++)
		if ((req.memoryTypeBits & (1u << i)) &&
		    (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			itype = i;
	VkMemoryAllocateInfo iai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, req.size, itype };
	VkDeviceMemory imem;
	if (r == VK_SUCCESS)
		r = vkAllocateMemory(dev, &iai, NULL, &imem);
	if (r == VK_SUCCESS)
		r = vkBindImageMemory(dev, image, imem, 0);
	VkImageViewCreateInfo ivci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, image,
	    VK_IMAGE_VIEW_TYPE_2D, format, { 0, 0, 0, 0 }, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
	VkImageView view;
	if (r == VK_SUCCESS)
		r = vkCreateImageView(dev, &ivci, NULL, &view);
	snprintf(detail, sizeof(detail), "%d, %llu bytes, memory type %u", r, (unsigned long long)req.size, itype);
	check(r == VK_SUCCESS, "a 64x64 color image and its view", detail);
	if (r != VK_SUCCESS)
		return failures;

	/* The buffer the image is copied to, which the CPU reads. */
	VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, SIZE * SIZE * 4,
	    VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL };
	VkBuffer buf;
	vkCreateBuffer(dev, &bci, NULL, &buf);
	vkGetBufferMemoryRequirements(dev, buf, &req);
	const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	uint32_t btype = UINT32_MAX;
	for (uint32_t i = 0; i < mp.memoryTypeCount && btype == UINT32_MAX; i++)
		if ((req.memoryTypeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & host) == host)
			btype = i;
	VkMemoryAllocateInfo bai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, req.size, btype };
	VkDeviceMemory bmem;
	void *map = NULL;
	r = vkAllocateMemory(dev, &bai, NULL, &bmem);
	if (r == VK_SUCCESS)
		r = vkBindBufferMemory(dev, buf, bmem, 0);
	if (r == VK_SUCCESS)
		r = vkMapMemory(dev, bmem, 0, VK_WHOLE_SIZE, 0, &map);
	check(r == VK_SUCCESS, "a host-visible buffer for the pixels", "");
	if (r != VK_SUCCESS)
		return failures;
	volatile uint32_t *px = map;
	for (unsigned i = 0; i < SIZE * SIZE; i++)
		px[i] = 0x12345678;

	/* A render pass that clears the image and leaves it for the copy. */
	VkAttachmentDescription att = { 0, format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
	    VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
	    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL };
	VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkSubpassDescription sub = { 0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, NULL, 1, &ref, NULL, NULL, 0, NULL };
	VkSubpassDependency dep = { 0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
	    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, 0 };
	VkRenderPassCreateInfo rpci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 1, &att, 1, &sub, 1,
	    &dep };
	VkRenderPass pass;
	r = vkCreateRenderPass(dev, &rpci, NULL, &pass);
	VkFramebufferCreateInfo fbci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, pass, 1, &view, SIZE,
	    SIZE, 1 };
	VkFramebuffer fb;
	if (r == VK_SUCCESS)
		r = vkCreateFramebuffer(dev, &fbci, NULL, &fb);
	snprintf(detail, sizeof(detail), "%d", r);
	check(r == VK_SUCCESS, "a render pass and its framebuffer", detail);
	if (r != VK_SUCCESS)
		return failures;

	/* The pipeline: the triangle's shaders, the whole image as viewport. */
	VkShaderModuleCreateInfo vsci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0,
	    sizeof(triangle_vert), triangle_vert };
	VkShaderModuleCreateInfo fsci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0,
	    sizeof(triangle_frag), triangle_frag };
	VkShaderModule vs, fs;
	vkCreateShaderModule(dev, &vsci, NULL, &vs);
	vkCreateShaderModule(dev, &fsci, NULL, &fs);
	VkPushConstantRange pcr = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16 };
	VkPipelineLayoutCreateInfo plci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL, 0, 0, NULL, 1,
	    &pcr };
	VkPipelineLayout layout;
	vkCreatePipelineLayout(dev, &plci, NULL, &layout);
	VkPipelineShaderStageCreateInfo stages[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main",
		    NULL },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs,
		    "main", NULL },
	};
	VkPipelineVertexInputStateCreateInfo vi = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
	    NULL, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE };
	VkViewport vp = { 0, 0, SIZE, SIZE, 0, 1 };
	VkRect2D scissor = { { 0, 0 }, { SIZE, SIZE } };
	VkPipelineViewportStateCreateInfo vps = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0, 1,
	    &vp, 1, &scissor };
	VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
	    NULL, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE,
	    VK_FALSE, 0, 0, 0, 1.0f };
	VkPipelineMultisampleStateCreateInfo ms = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
	    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
	VkPipelineColorBlendAttachmentState cba = { VK_FALSE, 0, 0, 0, 0, 0, 0, 0xf };
	VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL, 0,
	    VK_FALSE, VK_LOGIC_OP_COPY, 1, &cba, { 0, 0, 0, 0 } };
	VkGraphicsPipelineCreateInfo gpci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, NULL, 0, 2, stages,
	    &vi, &ia, NULL, &vps, &rs, &ms, NULL, &cb, NULL, layout, pass, 0, VK_NULL_HANDLE, -1 };
	VkPipeline pipeline;
	r = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpci, NULL, &pipeline);
	snprintf(detail, sizeof(detail), "%d", r);
	check(r == VK_SUCCESS, "a graphics pipeline (SPIR-V vertex and fragment shaders)", detail);
	if (r != VK_SUCCESS)
		return failures;

	/* Clear, draw, copy out. */
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
	VkClearValue black = { .color = { .float32 = { 0, 0, 0, 1 } } };
	VkRenderPassBeginInfo rpbi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL, pass, fb, scissor, 1, &black };
	vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	const float red[4] = { 1, 0, 0, 1 };
	vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(red), red);
	vkCmdDraw(cmd, 3, 1, 0, 0);
	vkCmdEndRenderPass(cmd);
	VkBufferImageCopy copy = { 0, 0, 0, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 }, { SIZE, SIZE, 1 } };
	vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1, &copy);
	VkMemoryBarrier mb = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL, VK_ACCESS_TRANSFER_WRITE_BIT,
	    VK_ACCESS_HOST_READ_BIT };
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0,
	    NULL);
	r = vkEndCommandBuffer(cmd);
	VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0 };
	VkFence fence;
	vkCreateFence(dev, &fci, NULL, &fence);
	VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, 1, &cmd, 0, NULL };
	if (r == VK_SUCCESS)
		r = vkQueueSubmit(q, 1, &si, fence);
	VkResult rw = r == VK_SUCCESS ? vkWaitForFences(dev, 1, &fence, VK_TRUE, 2000000000ull) : r;
	snprintf(detail, sizeof(detail), "%d %d", r, rw);
	check(r == VK_SUCCESS && rw == VK_SUCCESS, "drawn and copied; the fence signals", detail);

	/* The pixels: red inside, black outside, the counts near the area. */
	const uint32_t RED = 0xff0000ff, BLACK = 0xff000000;	/* R G B A in memory, little-endian */
	unsigned reds = 0, blacks = 0, others = 0;
	for (unsigned i = 0; i < SIZE * SIZE; i++)
		px[i] == RED ? reds++ : px[i] == BLACK ? blacks++ : others++;
	snprintf(detail, sizeof(detail), "center 0x%08x", px[(SIZE / 2) * SIZE + SIZE / 2]);
	check(px[(SIZE / 2) * SIZE + SIZE / 2] == RED, "the triangle's inside is red", detail);
	snprintf(detail, sizeof(detail), "0x%08x 0x%08x 0x%08x 0x%08x", px[0], px[SIZE - 1],
	    px[(SIZE - 1) * SIZE], px[SIZE * SIZE - 1]);
	check(px[0] == BLACK && px[SIZE - 1] == BLACK && px[(SIZE - 1) * SIZE] == BLACK &&
	    px[SIZE * SIZE - 1] == BLACK, "the corners are black (the clear)", detail);
	/* Corners at (32, 8), (56, 56), (8, 56): half of 48 by 48 = 1152. */
	snprintf(detail, sizeof(detail), "%u red, %u black, %u other", reds, blacks, others);
	check(others == 0 && reds > 1100 && reds < 1210, "every pixel red or black, the red ones the triangle's area",
	    detail);
	if (getenv("MINKAR_VKDRAW_SHOW") != NULL)
		for (unsigned y = 0; y < SIZE; y += 2) {
			for (unsigned x = 0; x < SIZE; x++)
				putchar(px[y * SIZE + x] == RED ? '#' : px[y * SIZE + x] == BLACK ? '.' : '?');
			putchar('\n');
		}
	vkDestroyDevice(dev, NULL);
	return failures;
}
