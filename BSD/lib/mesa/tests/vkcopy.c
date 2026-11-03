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
 * minkar-vkcopy: what presenting a frame costs (GPU track G4.6). A frame
 * of WIDTHxHEIGHT BGRA8 (default 3440x1440) rendered into an optimal image
 * in VRAM, then:
 *   gpu-vram   copied by the GPU into a buffer in VRAM: the WSI's present
 *              since G4.5 (the drawable's IOSurface in VRAM);
 *   gpu-system copied by the GPU into a buffer in system memory, and
 *   cpu        that buffer copied by the CPU into VRAM through the CPU's
 *              window (memcpy): the present before G4.5, both parts.
 * The GPU's times come from its timestamps, the CPU's from its clock;
 * medians of N runs (default 100).
 *
 *   minkar-vkcopy [width height [runs]]
 */

#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

static VkDevice dev;
static VkPhysicalDeviceMemoryProperties mp;

static int
cmpd(const void *a, const void *b)
{
	double x = *(const double *)a, y = *(const double *)b;
	return x < y ? -1 : x > y;
}

static double
median(double *v, unsigned n)
{
	qsort(v, n, sizeof(*v), cmpd);
	return n ? v[n / 2] : 0;
}

static uint32_t
memType(uint32_t bits, VkMemoryPropertyFlags want, VkMemoryPropertyFlags deny)
{
	for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
		if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want &&
		    !(mp.memoryTypes[i].propertyFlags & deny))
			return i;
	return UINT32_MAX;
}

static VkDeviceMemory
allocate(VkMemoryRequirements req, VkMemoryPropertyFlags want, VkMemoryPropertyFlags deny)
{
	uint32_t type = memType(req.memoryTypeBits, want, deny);
	if (type == UINT32_MAX)
		return VK_NULL_HANDLE;
	VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, req.size, type };
	VkDeviceMemory m = VK_NULL_HANDLE;
	vkAllocateMemory(dev, &mai, NULL, &m);
	return m;
}

static VkBuffer
buffer(VkDeviceSize size, VkMemoryPropertyFlags want, VkMemoryPropertyFlags deny, VkDeviceMemory *mem)
{
	VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, size,
	    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	    VK_SHARING_MODE_EXCLUSIVE, 0, NULL };
	VkBuffer b;
	VkMemoryRequirements req;
	vkCreateBuffer(dev, &bci, NULL, &b);
	vkGetBufferMemoryRequirements(dev, b, &req);
	*mem = allocate(req, want, deny);
	if (*mem == VK_NULL_HANDLE)
		return VK_NULL_HANDLE;
	vkBindBufferMemory(dev, b, *mem, 0);
	return b;
}

int
main(int argc, char **argv)
{
	uint32_t w = argc > 2 ? (uint32_t)atoi(argv[1]) : 3440, h = argc > 2 ? (uint32_t)atoi(argv[2]) : 1440;
	unsigned runs = argc > 3 ? (unsigned)atoi(argv[3]) : 100;
	VkDeviceSize bytes = (VkDeviceSize)w * h * 4;

	setvbuf(stdout, NULL, _IOLBF, 0);
	if (runs < 1 || runs > 1000 || w == 0 || h == 0) {
		fprintf(stderr, "usage: minkar-vkcopy [width height [runs]]\n");
		return 2;
	}
	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "minkar-vkcopy", 1, NULL, 0,
	    VK_API_VERSION_1_3 };
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app, 0, NULL, 0, NULL };
	VkInstance inst;
	if (vkCreateInstance(&ici, NULL, &inst) != VK_SUCCESS) {
		fprintf(stderr, "minkar-vkcopy: no Vulkan instance\n");
		return 1;
	}
	VkPhysicalDevice pd;
	uint32_t n = 1;
	vkEnumeratePhysicalDevices(inst, &n, &pd);
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(pd, &props);
	vkGetPhysicalDeviceMemoryProperties(pd, &mp);
	float prio = 1;
	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0, 0, 1, &prio };
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0, 1, &qci, 0, NULL, 0, NULL, NULL };
	if (vkCreateDevice(pd, &dci, NULL, &dev) != VK_SUCCESS) {
		fprintf(stderr, "minkar-vkcopy: no device\n");
		return 1;
	}
	VkQueue q;
	vkGetDeviceQueue(dev, 0, 0, &q);
	printf("device %s, %ux%u BGRA8 (%.1f MB), %u runs\n", props.deviceName, w, h, bytes / 1e6, runs);

	/* The rendered frame: an optimal image in VRAM, filled once. */
	VkImageCreateInfo ii = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0, VK_IMAGE_TYPE_2D,
	    VK_FORMAT_B8G8R8A8_UNORM, { w, h, 1 }, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
	    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
	    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
	    VK_IMAGE_LAYOUT_UNDEFINED };
	VkImage image;
	VkMemoryRequirements req;
	vkCreateImage(dev, &ii, NULL, &image);
	vkGetImageMemoryRequirements(dev, image, &req);
	VkDeviceMemory imageMem = allocate(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
	vkBindImageMemory(dev, image, imageMem, 0);

	VkDeviceMemory vramMem, sysMem, windowMem;
	VkBuffer vram = buffer(bytes, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, &vramMem);
	VkBuffer sys = buffer(bytes, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
	    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &sysMem);
	/* VRAM the CPU writes through its window, as an IOSurface in VRAM. */
	VkBuffer window = buffer(bytes, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0, &windowMem);
	if (vram == VK_NULL_HANDLE || sys == VK_NULL_HANDLE || window == VK_NULL_HANDLE) {
		fprintf(stderr, "minkar-vkcopy: no memory for the buffers\n");
		return 1;
	}

	VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
	    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, 0 };
	VkCommandPool pool;
	vkCreateCommandPool(dev, &pci, NULL, &pool);
	VkCommandBufferAllocateInfo cai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, pool,
	    VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(dev, &cai, &cmd);
	VkQueryPoolCreateInfo qpi = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, NULL, 0,
	    VK_QUERY_TYPE_TIMESTAMP, 2, 0 };
	VkQueryPool qp;
	vkCreateQueryPool(dev, &qpi, NULL, &qp);
	VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0 };
	VkFence fence;
	vkCreateFence(dev, &fci, NULL, &fence);

	/* The image filled and left as the WSI finds it: transfer source. */
	VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
	    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, NULL };
	VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	VkImageMemoryBarrier toDst = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, 0,
	    VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, range };
	VkImageMemoryBarrier toSrc = toDst;
	toSrc.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	toSrc.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	VkClearColorValue color = { .float32 = { 0.2f, 0.4f, 0.8f, 1.0f } };
	vkBeginCommandBuffer(cmd, &bi);
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
	    0, NULL, 0, NULL, 1, &toDst);
	vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
	    0, NULL, 0, NULL, 1, &toSrc);
	vkEndCommandBuffer(cmd);
	VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, 1, &cmd, 0, NULL };
	vkQueueSubmit(q, 1, &si, fence);
	vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
	vkResetFences(dev, 1, &fence);

	double *gpuVram = calloc(runs, sizeof(double)), *gpuSys = calloc(runs, sizeof(double));
	double *cpu = calloc(runs, sizeof(double));
	VkBuffer dsts[2] = { vram, sys };
	double *out[2] = { gpuVram, gpuSys };
	VkBufferImageCopy region = { 0, w, h, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 },
	    { w, h, 1 } };
	for (int d = 0; d < 2; d++)
		for (unsigned r = 0; r < runs; r++) {
			vkResetCommandBuffer(cmd, 0);
			vkBeginCommandBuffer(cmd, &bi);
			vkCmdResetQueryPool(cmd, qp, 0, 2);
			vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, qp, 0);
			vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dsts[d], 1,
			    &region);
			vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, qp, 1);
			vkEndCommandBuffer(cmd);
			vkQueueSubmit(q, 1, &si, fence);
			vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
			vkResetFences(dev, 1, &fence);
			uint64_t ts[2] = { 0, 0 };
			vkGetQueryPoolResults(dev, qp, 0, 2, sizeof(ts), ts, sizeof(ts[0]),
			    VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
			out[d][r] = (double)(ts[1] - ts[0]) * props.limits.timestampPeriod / 1e6;
		}

	/* The CPU's part of the old present: system memory into VRAM. */
	void *from = NULL, *to = NULL;
	vkMapMemory(dev, sysMem, 0, VK_WHOLE_SIZE, 0, &from);
	vkMapMemory(dev, windowMem, 0, VK_WHOLE_SIZE, 0, &to);
	mach_timebase_info_data_t tb;
	mach_timebase_info(&tb);
	for (unsigned r = 0; r < runs; r++) {
		uint64_t t0 = mach_absolute_time();
		memcpy(to, from, (size_t)bytes);
		cpu[r] = (double)(mach_absolute_time() - t0) * tb.numer / tb.denom / 1e6;
	}

	double mv = median(gpuVram, runs), ms = median(gpuSys, runs), mc = median(cpu, runs);
	printf("gpu-vram   %.3f ms  (%.1f GB/s)  image to a buffer in VRAM: the present since G4.5\n",
	    mv, bytes / mv / 1e6);
	printf("gpu-system %.3f ms  (%.1f GB/s)  image to a buffer in system memory\n", ms,
	    bytes / ms / 1e6);
	printf("cpu        %.3f ms  (%.1f GB/s)  that buffer into VRAM by the CPU\n", mc,
	    bytes / mc / 1e6);
	printf("before G4.5 %.3f ms a frame, since %.3f ms\n", ms + mc, mv);
	return 0;
}
