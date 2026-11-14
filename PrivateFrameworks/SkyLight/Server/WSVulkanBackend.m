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
 * The Vulkan backend (GPU track G4, WindowServer plan WS6): the frame is a
 * shared buffer in VRAM, made by the display's GPU and imported into
 * Vulkan by its descriptor; one compute shader (shaders/compose.comp)
 * draws into it a rectangle at a time, reaching every buffer by its GPU
 * address. A window's surface is the server's own memory, imported once
 * (VK_EXT_external_memory_host) and read in place; so is an IOSurface the
 * app made in the same GPU's memory (G4.3), imported by its shared buffer's
 * descriptor. Other IOSurfaces go through an upload buffer. The frame is
 * presented as pixman's shadow is, by the GPU's DMA engine in bands of
 * rows, or by the CPU through the display's colour table. Each part of a
 * frame (the windows, the cursor, the cursor taken away) is one
 * submission, waited for before the frame is presented or read by display
 * streams.
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <objc/runtime.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#import "WSCompositorBackend.h"
#import "WSDisplay.h"
#import "WSSurface.h"
#import "WSVulkanShaders.h"

/* compose.comp's push constants. */
typedef struct {
	uint64_t dst, src;
	int32_t rect[4];	/* x, y, width, height in dst's pixels */
	int32_t srcOrigin[2];	/* src's first pixel, in dst's pixels */
	uint32_t dstStride, srcStride;	/* pixels a row */
	uint32_t mode, alpha;
	int32_t size[2];	/* a shadow's window; a table's address */
} WSComposeOp;
_Static_assert(sizeof(WSComposeOp) == 64, "compose.comp's push constants");

enum { kFillBlack = 0, kOver = 1, kShadow = 2, kCopy = 3, kCopyTable = 4 };

/* What the loader gives, found once by name. */
#define WS_VK_FUNCTIONS(X) \
	X(vkDestroyInstance) X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties2) \
	X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkGetPhysicalDeviceMemoryProperties) \
	X(vkEnumerateDeviceExtensionProperties) X(vkCreateDevice) X(vkGetDeviceProcAddr) \
	X(vkDestroyDevice) X(vkGetDeviceQueue) X(vkDeviceWaitIdle) \
	X(vkCreateShaderModule) X(vkDestroyShaderModule) X(vkCreatePipelineLayout) \
	X(vkCreateComputePipelines) X(vkCreateCommandPool) X(vkAllocateCommandBuffers) \
	X(vkCreateFence) X(vkWaitForFences) X(vkResetFences) X(vkBeginCommandBuffer) \
	X(vkEndCommandBuffer) X(vkResetCommandBuffer) X(vkCmdBindPipeline) X(vkCmdPushConstants) \
	X(vkCmdDispatch) X(vkCmdPipelineBarrier) X(vkQueueSubmit) X(vkCreateBuffer) \
	X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) X(vkAllocateMemory) X(vkFreeMemory) \
	X(vkBindBufferMemory) X(vkMapMemory) X(vkGetBufferDeviceAddress)

#define WS_VK_DECLARE(f) static PFN_##f f;
WS_VK_FUNCTIONS(WS_VK_DECLARE)
static VkInstance sInstance;
static int (*sSharedBufferFD)(uint64_t);

/* The loader, an instance and the functions; NO when there is no Vulkan. */
static BOOL
loadVulkan(void)
{
	static dispatch_once_t once;
	static BOOL loaded;

	dispatch_once(&once, ^{
		/* The driver's shader cache: the server's home is /var/empty. */
		setenv("MESA_SHADER_CACHE_DIR", "/private/var/db/WindowServer/ShaderCache", 0);
		void *vk = dlopen("/usr/lib/libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
		void *drm = dlopen("/usr/lib/libdrm_amdgpu.dylib", RTLD_NOW | RTLD_LOCAL);
		if (vk == NULL || drm == NULL) {
			fprintf(stderr, "WindowServer: Vulkan: %s\n", dlerror());
			return;
		}
		sSharedBufferFD = (int (*)(uint64_t))dlsym(drm, "minkar_drm_shared_buffer_fd");
		PFN_vkGetInstanceProcAddr gipa = (PFN_vkGetInstanceProcAddr)dlsym(vk,
		    "vkGetInstanceProcAddr");
		if (gipa == NULL || sSharedBufferFD == NULL)
			return;
		PFN_vkCreateInstance createInstance = (PFN_vkCreateInstance)gipa(NULL,
		    "vkCreateInstance");
		VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "WindowServer", 1,
		    NULL, 0, VK_API_VERSION_1_3 };
		VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app,
		    0, NULL, 0, NULL };
		VkResult r = createInstance != NULL ? createInstance(&ici, NULL, &sInstance) :
		    VK_ERROR_INITIALIZATION_FAILED;
		if (r != VK_SUCCESS) {
			fprintf(stderr, "WindowServer: Vulkan: no instance (%d)\n", r);
			return;
		}
		BOOL all = YES;
#define WS_VK_LOAD(f) \
		if ((f = (PFN_##f)gipa(sInstance, #f)) == NULL) { \
			fprintf(stderr, "WindowServer: Vulkan: no %s\n", #f); \
			all = NO; \
		}
		WS_VK_FUNCTIONS(WS_VK_LOAD)
		loaded = all;
	});
	return loaded;
}

/* A buffer and its memory, with its GPU address and, when the CPU sees
 * it, its CPU address. */
@interface WSVulkanBuffer : NSObject {
@public
	VkDevice _device;
	VkBuffer _buffer;
	VkDeviceMemory _memory;
	uint64_t _address;
	uint8_t *_cpu;
	VkDeviceSize _size;
	VkDeviceSize _used;	/* an upload buffer's: what this frame took */
	BOOL _failed;		/* a surface's: it could not be imported */
}
@end

@implementation WSVulkanBuffer

- (void)dealloc
{
	if (_buffer != VK_NULL_HANDLE)
		vkDestroyBuffer(_device, _buffer, NULL);
	if (_memory != VK_NULL_HANDLE)
		vkFreeMemory(_device, _memory, NULL);
}

@end

@implementation WSVulkanBackend {
	WSDisplay *_display;
	VkPhysicalDevice _physical;
	VkDevice _device;
	uint32_t _deviceMinor;
	char _deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
	VkQueue _queue;
	VkPipelineLayout _layout;
	VkPipeline _pipeline;
	VkCommandPool _pool;
	VkCommandBuffer _commands;
	VkFence _fence;
	BOOL _recording;
	BOOL _lost;		/* the GPU stopped answering: nothing more is drawn */
	VkPhysicalDeviceMemoryProperties _memoryTypes;
	PFN_vkGetMemoryFdPropertiesKHR _getMemoryFdProperties;
	PFN_vkGetMemoryHostPointerPropertiesEXT _getHostPointerProperties;
	VkDeviceSize _hostAlignment;	/* 0: surfaces cannot be imported */
	BOOL _importTaken;	/* the last import's handle is Vulkan's now */

	/* The frame: VRAM shared with the display's GPU, mapped for the CPU. */
	WSVulkanBuffer *_frame;		/* the one being drawn, of: */
	uint8_t *_frameCPU;
	pixman_image_t *_frameImage;
	/* Two frames shown in turn by page flips (one if flips are off):
	 * what each lacks of the picture (drawn since in the other, or under
	 * the cursor it keeps), and which is drawn now. */
	WSVulkanBuffer *_frames[2];
	uint8_t *_frameCPUs[2];
	pixman_image_t *_frameImages[2];
	pixman_region32_t _stale[2];
	unsigned _frameCount, _current;
	BOOL _flips;		/* frames shown by page flips */
	int _shown;		/* the frame shown, or -1: the framebuffer */
	BOOL _framebufferCurrent;	/* the framebuffer holds the picture */
	uint32_t _width, _height, _rowBytes;
	BOOL _gpuPresent;
	uint8_t *_vram;
	uint32_t _vramRowBytes;

	/* This frame: its damage, the surfaces it reads in place (locked
	 * until the GPU is done), the upload buffers. */
	pixman_region32_t _clip;
	BOOL _clipped;
	NSMutableArray *_locked;
	NSMutableArray<WSVulkanBuffer *> *_uploads;

	/* What the cursor covers, put back by removeCursor. */
	uint64_t _under;
	int _cx, _cy, _cw, _ch;
}

- (instancetype)init
{
	if ((self = [super init]) == nil)
		return nil;
	pixman_region32_init(&_clip);
	pixman_region32_init(&_stale[0]);
	pixman_region32_init(&_stale[1]);
	_shown = -1;
	_locked = [NSMutableArray array];
	_uploads = [NSMutableArray array];
	return self;
}

- (void)dealloc
{
	pixman_region32_fini(&_clip);
	pixman_region32_fini(&_stale[0]);
	pixman_region32_fini(&_stale[1]);
	for (unsigned i = 0; i < 2; i++)
		if (_frameImages[i] != NULL)
			pixman_image_unref(_frameImages[i]);
}

- (NSString *)backendDescription
{
	return [NSString stringWithFormat:@"Vulkan on %s (render node %u), %@", _deviceName,
	    _deviceMinor, _flips ? @"shown by page flips at the vertical blank" :
	    _gpuPresent ? @"presented by the GPU's DMA engine" : @"presented by the CPU"];
}

- (uint32_t)frameWidth
{
	return _width;
}

- (uint32_t)frameHeight
{
	return _height;
}

static BOOL
hasExtension(const VkExtensionProperties *list, uint32_t n, const char *name)
{
	for (uint32_t i = 0; i < n; i++)
		if (strcmp(list[i].extensionName, name) == 0)
			return YES;
	return NO;
}

/* The display GPU's Vulkan device, its queue and the compositing pipeline. */
- (BOOL)setUpDevice:(uint32_t)minor
{
	if (_device != VK_NULL_HANDLE)
		return _deviceMinor == minor;
	uint32_t n = 0;
	vkEnumeratePhysicalDevices(sInstance, &n, NULL);
	VkPhysicalDevice devices[n > 0 ? n : 1];
	vkEnumeratePhysicalDevices(sInstance, &n, devices);
	for (uint32_t i = 0; i < n && _physical == VK_NULL_HANDLE; i++) {
		VkPhysicalDeviceDrmPropertiesEXT drm = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT };
		VkPhysicalDeviceExternalMemoryHostPropertiesEXT host = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT,
			.pNext = &drm };
		VkPhysicalDeviceProperties2 props = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &host };
		vkGetPhysicalDeviceProperties2(devices[i], &props);
		if (drm.hasRender && (uint32_t)drm.renderMinor == minor) {
			_physical = devices[i];
			_hostAlignment = host.minImportedHostPointerAlignment;
			strlcpy(_deviceName, props.properties.deviceName, sizeof(_deviceName));
		}
	}
	if (_physical == VK_NULL_HANDLE) {
		fprintf(stderr, "WindowServer: Vulkan: no device on render node %u\n", minor);
		return NO;
	}

	uint32_t ne = 0;
	vkEnumerateDeviceExtensionProperties(_physical, NULL, &ne, NULL);
	VkExtensionProperties exts[ne > 0 ? ne : 1];
	vkEnumerateDeviceExtensionProperties(_physical, NULL, &ne, exts);
	const char *want[3] = { VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
		VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME, VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME };
	if (!hasExtension(exts, ne, want[0]) || !hasExtension(exts, ne, want[1])) {
		fprintf(stderr, "WindowServer: Vulkan: %s cannot import shared buffers\n", _deviceName);
		_physical = VK_NULL_HANDLE;
		return NO;
	}
	uint32_t nwant = hasExtension(exts, ne, want[2]) ? 3 : 2;
	if (nwant == 2)
		_hostAlignment = 0;

	/* A queue that computes: the first family that does. */
	uint32_t nq = 0, family = UINT32_MAX;
	vkGetPhysicalDeviceQueueFamilyProperties(_physical, &nq, NULL);
	VkQueueFamilyProperties families[nq > 0 ? nq : 1];
	vkGetPhysicalDeviceQueueFamilyProperties(_physical, &nq, families);
	for (uint32_t i = 0; i < nq && family == UINT32_MAX; i++)
		if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
			family = i;
	if (family == UINT32_MAX) {
		_physical = VK_NULL_HANDLE;
		return NO;
	}

	float priority = 1;
	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0, family,
	    1, &priority };
	VkPhysicalDeviceVulkan12Features v12 = { .sType =
	    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .bufferDeviceAddress = VK_TRUE };
	VkPhysicalDeviceFeatures2 features = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
	    .pNext = &v12, .features = { .shaderInt64 = VK_TRUE } };
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &features, 0, 1, &qci, 0,
	    NULL, nwant, want, NULL };
	VkResult r = vkCreateDevice(_physical, &dci, NULL, &_device);
	if (r != VK_SUCCESS) {
		fprintf(stderr, "WindowServer: Vulkan: no device on %s (%d)\n", _deviceName, r);
		_physical = VK_NULL_HANDLE;
		_device = VK_NULL_HANDLE;
		return NO;
	}
	_deviceMinor = minor;
	vkGetPhysicalDeviceMemoryProperties(_physical, &_memoryTypes);
	_getMemoryFdProperties = (PFN_vkGetMemoryFdPropertiesKHR)vkGetDeviceProcAddr(_device,
	    "vkGetMemoryFdPropertiesKHR");
	_getHostPointerProperties = (PFN_vkGetMemoryHostPointerPropertiesEXT)vkGetDeviceProcAddr(
	    _device, "vkGetMemoryHostPointerPropertiesEXT");
	if (_getHostPointerProperties == NULL)
		_hostAlignment = 0;
	vkGetDeviceQueue(_device, family, 0, &_queue);

	VkShaderModuleCreateInfo smci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0,
	    sizeof(kWSComposeShader), kWSComposeShader };
	VkShaderModule module;
	VkPushConstantRange range = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(WSComposeOp) };
	VkPipelineLayoutCreateInfo plci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL, 0,
	    0, NULL, 1, &range };
	VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
	    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, family };
	VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0 };
	if (vkCreateShaderModule(_device, &smci, NULL, &module) != VK_SUCCESS)
		return NO;
	r = vkCreatePipelineLayout(_device, &plci, NULL, &_layout);
	if (r == VK_SUCCESS) {
		VkComputePipelineCreateInfo cpci = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		    NULL, 0, { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0,
		    VK_SHADER_STAGE_COMPUTE_BIT, module, "main", NULL }, _layout, VK_NULL_HANDLE, 0 };
		r = vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &cpci, NULL, &_pipeline);
	}
	vkDestroyShaderModule(_device, module, NULL);
	if (r == VK_SUCCESS)
		r = vkCreateCommandPool(_device, &pci, NULL, &_pool);
	if (r == VK_SUCCESS) {
		VkCommandBufferAllocateInfo cai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		    NULL, _pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
		r = vkAllocateCommandBuffers(_device, &cai, &_commands);
	}
	if (r == VK_SUCCESS)
		r = vkCreateFence(_device, &fci, NULL, &_fence);
	if (r != VK_SUCCESS) {
		fprintf(stderr, "WindowServer: Vulkan: no compositing pipeline on %s (%d)\n",
		    _deviceName, r);
		return NO;
	}
	return YES;
}

/* The first memory type among bits with these properties. */
- (uint32_t)memoryType:(uint32_t)bits flags:(VkMemoryPropertyFlags)flags
{
	for (uint32_t i = 0; i < _memoryTypes.memoryTypeCount; i++)
		if ((bits & (1u << i)) && (_memoryTypes.memoryTypes[i].propertyFlags & flags) == flags)
			return i;
	return UINT32_MAX;
}

/*
 * A buffer of size bytes the shaders reach by address: imported (import
 * names a VkImportMemory*Info and handleType its kind) or, without import,
 * new memory the CPU writes.
 */
- (WSVulkanBuffer *)bufferOfSize:(VkDeviceSize)size import:(const void *)import
    handleType:(VkExternalMemoryHandleTypeFlagBits)handleType typeBits:(uint32_t)typeBits
{
	WSVulkanBuffer *b = [[WSVulkanBuffer alloc] init];
	VkExternalMemoryBufferCreateInfo ext = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
	    NULL, handleType };
	VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, import != NULL ? &ext : NULL,
	    0, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
	    VK_SHARING_MODE_EXCLUSIVE, 0, NULL };

	_importTaken = NO;
	b->_device = _device;
	b->_size = size;
	if (vkCreateBuffer(_device, &bci, NULL, &b->_buffer) != VK_SUCCESS) {
		b->_buffer = VK_NULL_HANDLE;
		return nil;
	}
	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(_device, b->_buffer, &req);
	VkMemoryPropertyFlags flags = import != NULL ? 0 :
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	uint32_t type = [self memoryType:req.memoryTypeBits & typeBits flags:flags];
	if (type == UINT32_MAX)
		return nil;
	VkMemoryAllocateFlagsInfo addressable = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
	    import, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0 };
	VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &addressable,
	    import != NULL ? size : req.size, type };
	if (vkAllocateMemory(_device, &mai, NULL, &b->_memory) != VK_SUCCESS) {
		b->_memory = VK_NULL_HANDLE;
		return nil;
	}
	_importTaken = import != NULL;
	if (vkBindBufferMemory(_device, b->_buffer, b->_memory, 0) != VK_SUCCESS)
		return nil;
	if (import == NULL && vkMapMemory(_device, b->_memory, 0, VK_WHOLE_SIZE, 0,
	    (void **)&b->_cpu) != VK_SUCCESS)
		return nil;
	VkBufferDeviceAddressInfo bdai = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, NULL,
	    b->_buffer };
	b->_address = vkGetBufferDeviceAddress(_device, &bdai);
	return b;
}

/* A shared VRAM buffer of the display's GPU, by its name, as the GPU
 * sees it (size bytes from its start); nil if refused. */
- (WSVulkanBuffer *)importShared:(uint64_t)name size:(VkDeviceSize)size
{
	int fd = sSharedBufferFD(name);
	VkMemoryFdPropertiesKHR fdProps = { .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR };

	if (fd < 0)
		return nil;
	if (_getMemoryFdProperties(_device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd,
	    &fdProps) != VK_SUCCESS) {
		close(fd);
		return nil;
	}
	VkImportMemoryFdInfoKHR import = { VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, NULL,
	    VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd };
	WSVulkanBuffer *b = [self bufferOfSize:size import:&import
	    handleType:VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT typeBits:fdProps.memoryTypeBits];
	if (!_importTaken)
		close(fd);	/* Vulkan owns it once its memory is made */
	return b;
}

- (void)releaseFrame
{
	if (_device != VK_NULL_HANDLE)
		vkDeviceWaitIdle(_device);
	for (unsigned i = 0; i < 2; i++) {
		if (_frameImages[i] != NULL)
			pixman_image_unref(_frameImages[i]);
		_frameImages[i] = NULL;
		_frames[i] = nil;
		_frameCPUs[i] = NULL;
	}
	_frameImage = NULL;
	_frame = nil;
	_frameCPU = NULL;
	_frameCount = 0;
	_flips = NO;
	_shown = -1;
	[_uploads removeAllObjects];
	[_display tearDownGPUPresent];
}

- (BOOL)setUpForDisplay:(WSDisplay *)display width:(uint32_t)width height:(uint32_t)height
{
	uint8_t *vram = NULL;
	uint32_t rowBytes = 0;
	uint64_t names[2] = { 0, 0 };

	[self releaseFrame];
	_display = display;
	if (!loadVulkan() || ![display mapFramebuffer:&vram rowBytes:&rowBytes])
		return NO;
	uint32_t minor = display.gpuRenderMinor;
	if (minor == 0 || ![self setUpDevice:minor])
		return NO;

	/* The frame, rows as long as the framebuffer's (damaged rows are one
	 * range in both), in whole pages. */
	if (rowBytes < width * 4)
		rowBytes = width * 4;
	size_t bytes = ((size_t)rowBytes * height + 4095) & ~(size_t)4095;
	/* Two, shown in turn by page flips; one if the GPU has room for no
	 * more, or WSNoFlip asks (it is then copied into the framebuffer). */
	uint8_t *cpu[2] = { NULL, NULL };
	unsigned count = getenv("WSNoFlip") != NULL ? 1 : 2;
	while (count > 0 && ![display setUpGPUFrames:count bytes:bytes names:names cpu:cpu])
		count--;
	if (count == 0) {
		fprintf(stderr, "WindowServer: Vulkan: the GPU made no frame of %zu bytes\n", bytes);
		return NO;
	}
	for (unsigned i = 0; i < count; i++) {
		_frames[i] = [self importShared:names[i] size:bytes];
		if (_frames[i] == nil) {
			fprintf(stderr, "WindowServer: Vulkan: the frame cannot be imported\n");
			[self releaseFrame];
			return NO;
		}
		memset(cpu[i], 0, bytes);
		_frameCPUs[i] = cpu[i];
		_frameImages[i] = pixman_image_create_bits(PIXMAN_x8r8g8b8, width, height,
		    (uint32_t *)cpu[i], rowBytes);
		/* Nothing is drawn in it yet. */
		pixman_region32_fini(&_stale[i]);
		pixman_region32_init_rect(&_stale[i], 0, 0, width, height);
	}
	_frameCount = count;
	_flips = count == 2;
	_shown = -1;
	_framebufferCurrent = NO;
	[self drawInto:0];
	_width = width;
	_height = height;
	_rowBytes = rowBytes;
	_vram = vram;
	_vramRowBytes = rowBytes;
	_gpuPresent = YES;
	_lost = NO;
	return YES;
}

/* Commands from here on, until -submit. */
- (void)begin
{
	if (_recording)
		return;
	VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
	    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, NULL };
	vkResetCommandBuffer(_commands, 0);
	vkBeginCommandBuffer(_commands, &bi);
	vkCmdBindPipeline(_commands, VK_PIPELINE_BIND_POINT_COMPUTE, _pipeline);
	_recording = YES;
}

/* What was recorded, run and waited for; then the surfaces read in place
 * are let go. */
- (void)submit
{
	if (!_recording)
		return;
	_recording = NO;
	VkMemoryBarrier toHost = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL,
	    VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT };
	vkCmdPipelineBarrier(_commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	    VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &toHost, 0, NULL, 0,
	    NULL);
	vkEndCommandBuffer(_commands);
	VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, 1, &_commands, 0, NULL };
	VkResult r = _lost ? VK_ERROR_DEVICE_LOST : vkQueueSubmit(_queue, 1, &si, _fence);
	if (r == VK_SUCCESS) {
		r = vkWaitForFences(_device, 1, &_fence, VK_TRUE, 2000000000ull);
		vkResetFences(_device, 1, &_fence);
	}
	if (r != VK_SUCCESS && !_lost) {
		fprintf(stderr, "WindowServer: display 0x%x: the GPU did not finish a frame (%d); "
		    "nothing more is drawn\n", _display.displayID, r);
		_lost = YES;
	}
	for (id surface in _locked)
		[surface unlock];
	[_locked removeAllObjects];
}

/* One operation, over its rectangle clipped to the frame and, between
 * beginFrame and endWindows, to the damage; then a barrier, for the next
 * one reads what this one wrote. */
- (void)run:(WSComposeOp)op
{
	int32_t x1 = MAX(op.rect[0], 0), y1 = MAX(op.rect[1], 0);
	int32_t x2 = op.rect[0] + op.rect[2], y2 = op.rect[1] + op.rect[3];
	pixman_box32_t whole = { 0, 0, (int32_t)_width, (int32_t)_height };
	pixman_box32_t *boxes = &whole;
	int n = 1;

	if (op.dst == _frame->_address) {
		x2 = MIN(x2, (int32_t)_width);
		y2 = MIN(y2, (int32_t)_height);
		if (_clipped)
			boxes = pixman_region32_rectangles(&_clip, &n);
	} else
		boxes = NULL, n = 1;
	if (x1 >= x2 || y1 >= y2)
		return;
	[self begin];
	BOOL ran = NO;
	for (int i = 0; i < n; i++) {
		WSComposeOp o = op;
		int32_t bx1 = x1, by1 = y1, bx2 = x2, by2 = y2;
		if (boxes != NULL) {
			bx1 = MAX(bx1, boxes[i].x1);
			by1 = MAX(by1, boxes[i].y1);
			bx2 = MIN(bx2, boxes[i].x2);
			by2 = MIN(by2, boxes[i].y2);
		}
		if (bx1 >= bx2 || by1 >= by2)
			continue;
		o.rect[0] = bx1;
		o.rect[1] = by1;
		o.rect[2] = bx2 - bx1;
		o.rect[3] = by2 - by1;
		vkCmdPushConstants(_commands, _layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(o), &o);
		vkCmdDispatch(_commands, ((uint32_t)o.rect[2] + 7) / 8, ((uint32_t)o.rect[3] + 7) / 8, 1);
		ran = YES;
	}
	if (!ran)
		return;
	VkMemoryBarrier mb = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL, VK_ACCESS_SHADER_WRITE_BIT,
	    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT };
	vkCmdPipelineBarrier(_commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
}

/* Room for bytes in this frame's upload buffers: its GPU address, and
 * where the CPU writes it. */
- (uint64_t)upload:(VkDeviceSize)bytes cpu:(uint8_t **)cpu
{
	bytes = (bytes + 255) & ~(VkDeviceSize)255;
	for (WSVulkanBuffer *b in _uploads)
		if (b->_size - b->_used >= bytes) {
			*cpu = b->_cpu + b->_used;
			b->_used += bytes;
			return b->_address + b->_used - bytes;
		}
	WSVulkanBuffer *b = [self bufferOfSize:MAX(bytes, (VkDeviceSize)16 << 20) import:NULL
	    handleType:0 typeBits:~0u];
	if (b == nil)
		return 0;
	[_uploads addObject:b];
	*cpu = b->_cpu;
	b->_used = bytes;
	return b->_address;
}

/* Pixels the GPU reads from an upload buffer, packed: their address. */
- (uint64_t)uploadPixels:(const uint8_t *)pixels rowBytes:(uint32_t)rowBytes width:(uint32_t)w
    height:(uint32_t)h
{
	uint8_t *cpu = NULL;
	uint64_t address = [self upload:(VkDeviceSize)w * h * 4 cpu:&cpu];

	if (address == 0)
		return 0;
	if (rowBytes == w * 4)
		memcpy(cpu, pixels, (size_t)rowBytes * h);
	else
		for (uint32_t y = 0; y < h; y++)
			memcpy(cpu + (size_t)y * w * 4, pixels + (size_t)y * rowBytes, (size_t)w * 4);
	return address;
}

/* A server surface's memory as the GPU sees it, imported at its first
 * frame and kept as long as the surface; nil when it cannot be. */
- (WSVulkanBuffer *)importSurface:(WSSurface *)surface
{
	const void *key = (__bridge const void *)self;
	WSVulkanBuffer *b = objc_getAssociatedObject(surface, key);

	if (b != nil)
		return b->_failed ? nil : b;
	mach_vm_address_t base = surface.memoryBase;
	mach_vm_size_t size = surface.memorySize;
	VkMemoryHostPointerPropertiesEXT hostProps = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT };
	if (_hostAlignment != 0 && base % _hostAlignment == 0 && size % _hostAlignment == 0 &&
	    _getHostPointerProperties(_device,
	    VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, (void *)(uintptr_t)base,
	    &hostProps) == VK_SUCCESS) {
		VkImportMemoryHostPointerInfoEXT import = {
			VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT, NULL,
			VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, (void *)(uintptr_t)base };
		b = [self bufferOfSize:size import:&import
		    handleType:VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT
		    typeBits:hostProps.memoryTypeBits];
	}
	if (b == nil) {
		b = [[WSVulkanBuffer alloc] init];
		b->_failed = YES;
	}
	objc_setAssociatedObject(surface, key, b, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
	return b->_failed ? nil : b;
}

/* An app's IOSurface in this GPU's memory, imported at its first frame
 * and kept as long as the surface: the compositor reads it where the app
 * draws (G4.3). nil for other surfaces. */
- (WSVulkanBuffer *)importIOSurface:(WSIOSurfaceBacking *)surface
{
	const void *key = (__bridge const void *)self;
	WSVulkanBuffer *b = objc_getAssociatedObject(surface, key);

	if (b != nil)
		return b->_failed ? nil : b;
	if (surface.gpuBuffer == 0 || surface.gpuEntryID != _display.gpuEntryID)
		return nil;
	VkDeviceSize size = ((VkDeviceSize)surface.allocSize + 4095) & ~(VkDeviceSize)4095;
	if ((VkDeviceSize)surface.rowBytes * surface.height > size)
		return nil;
	b = [self importShared:surface.gpuBuffer size:size];
	if (b == nil) {
		fprintf(stderr, "WindowServer: Vulkan: IOSurface %u's VRAM cannot be imported; "
		    "it is uploaded\n", surface.surfaceID);
		b = [[WSVulkanBuffer alloc] init];
		b->_failed = YES;
	} else
		fprintf(stderr, "WindowServer: Vulkan: IOSurface %u is read in place, in VRAM\n",
		    surface.surfaceID);
	objc_setAssociatedObject(surface, key, b, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
	return b->_failed ? nil : b;
}

- (void)drawInto:(unsigned)index
{
	_current = index;
	_frame = _frames[index];
	_frameCPU = _frameCPUs[index];
	_frameImage = _frameImages[index];
}

- (void)beginFrame:(pixman_region32_t *)damage
{
	for (WSVulkanBuffer *b in _uploads)
		b->_used = 0;
	/* With flips, the frame not shown; it lacks what changed since it
	 * was drawn, as well as this frame's damage. */
	if (_flips && _shown >= 0)
		[self drawInto:_shown == 0 ? 1 : 0];
	for (unsigned i = 0; i < _frameCount; i++)
		pixman_region32_union(&_stale[i], &_stale[i], damage);
	pixman_region32_intersect_rect(&_clip, &_stale[_current], 0, 0, _width, _height);
	pixman_region32_clear(&_stale[_current]);
	_clipped = NO;
	int n = 0;
	pixman_box32_t *boxes = pixman_region32_rectangles(&_clip, &n);
	for (int i = 0; i < n; i++)
		[self fillBlackX:boxes[i].x1 y:boxes[i].y1 width:boxes[i].x2 - boxes[i].x1
		    height:boxes[i].y2 - boxes[i].y1];
	_clipped = YES;
}

- (void)fillBlackX:(int)x y:(int)y width:(int)width height:(int)height
{
	WSComposeOp op = { .dst = _frame->_address, .rect = { x, y, width, height },
	    .dstStride = _rowBytes / 4, .mode = kFillBlack };
	[self run:op];
}

static uint32_t
alpha8(double alpha)
{
	return alpha >= 1 ? 255 : alpha <= 0 ? 0 : (uint32_t)(alpha * 255 + 0.5);
}

- (void)drawShadowX:(int)x y:(int)y width:(int)w height:(int)h alpha:(double)alpha
{
	WSComposeOp op = { .dst = _frame->_address,
	    .rect = { x - kWSShadowRadius, y - kWSShadowRadius + kWSShadowOffsetY,
		w + 2 * kWSShadowRadius, h + 2 * kWSShadowRadius },
	    .srcOrigin = { x, y }, .dstStride = _rowBytes / 4, .mode = kShadow,
	    .alpha = alpha8(alpha), .size = { w, h } };
	[self run:op];
}

- (void)drawSurface:(WSSurface *)surface x:(int)x y:(int)y alpha:(double)alpha
{
	[self drawSurface:surface x:x y:y clip:(SLSIntRect){ x, y, (int32_t)surface.width,
	    (int32_t)surface.height } alpha:alpha];
}

- (void)drawSurface:(WSSurface *)surface x:(int)x y:(int)y clip:(SLSIntRect)clip alpha:(double)alpha
{
	/* The clip within the surface: the rect drawn; the surface's first
	 * pixel stays at (x, y). */
	int x0 = MAX(clip.x, x), y0 = MAX(clip.y, y);
	int x1 = MIN(clip.x + clip.width, x + (int)surface.width);
	int y1 = MIN(clip.y + clip.height, y + (int)surface.height);
	if (x1 <= x0 || y1 <= y0)
		return;
	WSComposeOp op = { .dst = _frame->_address, .rect = { x0, y0, x1 - x0, y1 - y0 },
	    .srcOrigin = { x, y }, .dstStride = _rowBytes / 4, .srcStride = surface.rowBytes / 4,
	    .mode = kOver, .alpha = alpha8(alpha) };
	WSVulkanBuffer *memory = [surface isKindOfClass:[WSSurface class]] ?
	    [self importSurface:surface] : [surface isKindOfClass:[WSIOSurfaceBacking class]] ?
	    [self importIOSurface:(WSIOSurfaceBacking *)surface] : nil;
	/* Shown twice in one frame: what is recorded runs first, for its lock
	 * is not taken twice. */
	if ([_locked indexOfObjectIdenticalTo:surface] != NSNotFound)
		[self submit];
	const uint8_t *pixels = [surface lockFront];

	if (memory != nil) {
		/* Read in place: locked until the GPU is done. */
		uint64_t start = [surface isKindOfClass:[WSSurface class]] ? surface.memoryBase :
		    (uintptr_t)pixels;	/* an IOSurface's pixels start its buffer */
		op.src = memory->_address + (uint64_t)((uintptr_t)pixels - start);
		[_locked addObject:surface];
	} else {
		op.src = [self uploadPixels:pixels rowBytes:surface.rowBytes width:surface.width
		    height:surface.height];
		op.srcStride = surface.width;
		[surface unlock];
	}
	if (op.src != 0)
		[self run:op];
}

- (void)endWindows
{
	_clipped = NO;
	[self submit];
}

- (pixman_image_t *)frameImage
{
	return _frameImage;
}

- (void)drawCursor:(pixman_image_t *)cursor x:(int)x y:(int)y width:(int)w height:(int)h
{
	[self removeCursor];
	uint8_t *saved = NULL;
	uint64_t under = [self upload:(VkDeviceSize)w * h * 4 cpu:&saved];
	uint64_t pixels = [self uploadPixels:(const uint8_t *)pixman_image_get_data(cursor)
	    rowBytes:(uint32_t)pixman_image_get_stride(cursor) width:w height:h];
	if (under == 0 || pixels == 0)
		return;
	/* What it covers, into a buffer as large as the cursor; then the
	 * cursor over the frame. */
	WSComposeOp save = { .dst = under, .src = _frame->_address, .rect = { 0, 0, w, h },
	    .srcOrigin = { -x, -y }, .dstStride = w, .srcStride = _rowBytes / 4, .mode = kCopy };
	save.rect[0] = MAX(0, -x);
	save.rect[1] = MAX(0, -y);
	save.rect[2] = MIN(w, (int)_width - x) - save.rect[0];
	save.rect[3] = MIN(h, (int)_height - y) - save.rect[1];
	if (save.rect[2] <= 0 || save.rect[3] <= 0)
		return;
	[self run:save];
	WSComposeOp over = { .dst = _frame->_address, .src = pixels, .rect = { x, y, w, h },
	    .srcOrigin = { x, y }, .dstStride = _rowBytes / 4, .srcStride = w, .mode = kOver,
	    .alpha = 255 };
	[self run:over];
	[self submit];
	/* A frame shown keeps its cursor: the next time it is drawn, there
	 * is to be drawn again. */
	if (_flips)
		pixman_region32_union_rect(&_stale[_current], &_stale[_current], x, y, w, h);
	_under = under;
	_cx = x;
	_cy = y;
	_cw = w;
	_ch = h;
}

- (void)removeCursor
{
	if (_under == 0)
		return;
	if (_shown == (int)_current) {	/* on the screen: it stays */
		_under = 0;
		return;
	}
	WSComposeOp put = { .dst = _frame->_address, .src = _under, .rect = { _cx, _cy, _cw, _ch },
	    .srcOrigin = { _cx, _cy }, .dstStride = _rowBytes / 4, .srcStride = _cw, .mode = kCopy };
	[self run:put];
	[self submit];
	_under = 0;
}

/*
 * The frame drawn, shown: by a page flip at the next vertical blank,
 * waited for; or, through a colour table or without flips, copied into the
 * framebuffer (all of it if the framebuffer was not current), by the GPU's
 * DMA engine in bands of rows or by the CPU, and the framebuffer shown
 * again.
 */
- (uint64_t)present:(pixman_region32_t *)damage colors:(NSData *)colors
{
	int nboxes = 0;
	pixman_box32_t *boxes = pixman_region32_rectangles(damage, &nboxes);
	const uint8_t *lut = colors.bytes;	/* red, green, blue */
	uint64_t area = 0;

	if (_lost)
		return 0;
	for (int i = 0; i < nboxes; i++)
		area += (uint64_t)(boxes[i].x2 - boxes[i].x1) * (boxes[i].y2 - boxes[i].y1);
	if (_flips && lut == NULL) {
		if ([_display flipToGPUFrame:(int)_current]) {
			_shown = (int)_current;
			_framebufferCurrent = NO;
			return area;
		}
		fprintf(stderr, "WindowServer: display 0x%x: the display took no page flip; "
		    "frames are copied from now on\n", _display.displayID);
		_flips = NO;
	}

	pixman_box32_t whole = { 0, 0, (int32_t)_width, (int32_t)_height };
	if (!_framebufferCurrent) {
		boxes = &whole;
		nboxes = 1;
	}
	BOOL presented = NO;
	if (_gpuPresent && lut == NULL && _rowBytes == _vramRowBytes) {
		int32_t bands[2 * kWSMaxRowBands];
		int nbands = WSRowBands(boxes, nboxes, bands);
		presented = YES;
		for (int j = 0; j < nbands && presented; j++)
			presented = [_display gpuPresentFrame:_current
			    offset:(size_t)bands[2 * j] * _rowBytes
			    bytes:(size_t)(bands[2 * j + 1] - bands[2 * j]) * _rowBytes];
		if (!presented) {
			fprintf(stderr, "WindowServer: display 0x%x: the GPU did not take a frame; "
			    "the CPU presents from now on\n", _display.displayID);
			_gpuPresent = NO;
		}
	}
	/* The CPU: from the frame's mapping, through the colour table. */
	for (int i = 0; i < nboxes && !presented; i++) {
		size_t bytes = (size_t)(boxes[i].x2 - boxes[i].x1) * 4;
		for (int y = boxes[i].y1; y < boxes[i].y2; y++) {
			uint8_t *to = _vram + (size_t)y * _vramRowBytes + (size_t)boxes[i].x1 * 4;
			const uint8_t *from = _frameCPU + (size_t)y * _rowBytes + (size_t)boxes[i].x1 * 4;
			if (lut == NULL) {
				memcpy(to, from, bytes);
				continue;
			}
			for (size_t x = 0; x < bytes; x += 4) {	/* B, G, R, X */
				to[x] = lut[512 + from[x]];
				to[x + 1] = lut[256 + from[x + 1]];
				to[x + 2] = lut[from[x + 2]];
				to[x + 3] = from[x + 3];
			}
		}
	}
	_framebufferCurrent = YES;
	if (_shown >= 0 && [_display flipToGPUFrame:-1])
		_shown = -1;
	return area;
}

@end
