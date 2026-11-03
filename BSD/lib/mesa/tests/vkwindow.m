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
 * minkar-vkwindow: Vulkan presented in a window, as apps on the reference
 * system do it: an AppKit window whose view has a CAMetalLayer, a surface
 * for that layer (VK_EXT_metal_surface), a swapchain, and frames drawn by
 * RADV (a clear that changes color, then the red triangle) and presented.
 * The last frame is checked where the window server got it (the window's
 * back buffer, SkyLight's copy of what was committed).
 *
 *   minkar-vkwindow [FRAMES [ICD]]	(default 120, /usr/lib/libvulkan_radeon.dylib)
 *   minkar-vkwindow -layer		no Vulkan: a drawable of the layer
 *					filled by the CPU and presented (any GPU)
 *   MINKAR_VKWINDOW_HOLD=SECONDS	keep the window up that long after
 */

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CAMetalLayerPrivate.h>
#import <SkyLight/SkyLight.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define VK_USE_PLATFORM_METAL_EXT 1
#include <vulkan/vulkan.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
#include "triangle_vert.h"
#include "triangle_frag.h"
#pragma clang diagnostic pop

#define WIDTH 400
#define HEIGHT 300
#define MAX_IMAGES 4

typedef PFN_vkVoidFunction (VKAPI_PTR *PFN_icdGetInstanceProcAddr)(VkInstance, const char *);
typedef VkResult (VKAPI_PTR *PFN_icdNegotiate)(uint32_t *);

@interface NSWindow (MinkarSkyLight)
- (uint32_t)_skyLightWindowID;
@end

/* A view whose layer is a CAMetalLayer. */
@interface VKView : NSView
@end

@implementation VKView
- (CALayer *)makeBackingLayer
{
	return [CAMetalLayer layer];
}
- (BOOL)isOpaque
{
	return YES;
}
@end

static int failures;

static void
check(int good, const char *what, const char *detail)
{
	printf("%s %s%s%s\n", good ? "ok  " : "FAIL", what, detail[0] ? ": " : "", detail);
	if (!good)
		failures++;
}

static void
crashed(int sig, siginfo_t *info, void *context)
{
	void *frames[64];
	Dl_info dl;
	int n = backtrace(frames, 64);
	(void)context;
	dprintf(2, "FAIL signal %d at address %p", sig, info->si_addr);
	if (dladdr(info->si_addr, &dl) != 0 && dl.dli_fname != NULL)
		dprintf(2, " (%s, %s)", dl.dli_fname, dl.dli_sname ? dl.dli_sname : "?");
	dprintf(2, ", from:\n");
	backtrace_symbols_fd(frames, n, 2);
	_exit(128 + sig);
}

static double
now(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* Lets AppKit handle what is queued (the window coming up, its drawing). */
static void
spin(double seconds)
{
	NSDate *until = [NSDate dateWithTimeIntervalSinceNow:seconds];
	NSEvent *e;
	while ((e = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:until inMode:NSDefaultRunLoopMode
	    dequeue:YES]) != nil)
		[NSApp sendEvent:e];
}

#define GET(inst, name) PFN_##name name = (PFN_##name)gipa(inst, #name)
#define DEV(name) PFN_##name name = (PFN_##name)vkGetDeviceProcAddr(dev, #name)
#define TRY(what, call) do { VkResult r_ = (call); char d_[32]; snprintf(d_, sizeof(d_), "%d", r_); \
	if (r_ != VK_SUCCESS) { check(0, what, d_); return failures; } } while (0)

/* The window's pixels where the server has them, at (x, y) of the view
 * (which fills the content, below the title bar). */
static int
windowPixel(NSWindow *window, uint32_t x, uint32_t y, uint32_t *out, char *detail, size_t size)
{
	void *pixels = NULL;
	uint32_t ww = 0, wh = 0, rowBytes = 0;
	if (SLSWindowBackBuffer([window _skyLightWindowID], &pixels, &ww, &wh, &rowBytes) != kSLSErrorSuccess ||
	    pixels == NULL) {
		snprintf(detail, size, "no back buffer");
		return 0;
	}
	NSRect content = [window contentRectForFrameRect:[window frame]];
	uint32_t top = wh - (uint32_t)NSHeight(content);	/* the title bar */
	*out = ((const uint32_t *)pixels)[(size_t)(top + y) * (rowBytes / 4) + x];
	snprintf(detail, size, "window %ux%u, title %u", ww, wh, top);
	return 1;
}

/* The layer alone: a drawable the CPU fills (BGRA: blue, a red square in
 * the middle), presented, then found in the window. */
static int
layerOnly(NSWindow *window, CAMetalLayer *layer)
{
	char detail[512], where[128];
	id<CAMetalDrawable> drawable = [layer nextDrawable];
	IOSurfaceRef surface = drawable ? [(id)drawable _minkarIOSurface] : NULL;
	CGSize size = [layer drawableSize];
	snprintf(detail, sizeof(detail), "%.0fx%.0f, surface %u", size.width, size.height,
	    surface ? IOSurfaceGetID(surface) : 0);
	check(surface != NULL, "the layer gives a drawable with an IOSurface", detail);
	if (surface == NULL)
		return failures;
	IOSurfaceLock(surface, 0, NULL);
	uint32_t *base = IOSurfaceGetBaseAddress(surface);	/* rows are 4-byte aligned */
	size_t stride = IOSurfaceGetBytesPerRow(surface) / 4, w = IOSurfaceGetWidth(surface), h = IOSurfaceGetHeight(surface);
	for (size_t y = 0; y < h; y++)
		for (size_t x = 0; x < w; x++)
			base[y * stride + x] =
			    (x >= w / 4 && x < 3 * w / 4 && y >= h / 4 && y < 3 * h / 4) ? 0xffff0000 : 0xff0000ff;
	IOSurfaceUnlock(surface, 0, NULL);
	[drawable present];
	uint32_t center = 0, corner = 0;
	if (windowPixel(window, (uint32_t)w / 2, (uint32_t)h / 2, &center, where, sizeof(where)) &&
	    windowPixel(window, 2, 2, &corner, where, sizeof(where))) {
		snprintf(detail, sizeof(detail), "%s; center 0x%08x, corner 0x%08x", where, center, corner);
		check(center == 0xffff0000 && corner == 0xff0000ff,
		    "the window shows it: the red square on its blue", detail);
	} else
		check(0, "the window shows it", where);
	/* The window drawn again by AppKit: the layer's frame comes back. */
	[[window contentView] setNeedsDisplay:YES];
	[window display];
	spin(0.2);
	if (windowPixel(window, (uint32_t)w / 2, (uint32_t)h / 2, &center, where, sizeof(where))) {
		snprintf(detail, sizeof(detail), "center 0x%08x", center);
		check(center == 0xffff0000, "after AppKit draws the window, the frame is still there", detail);
	}
	return failures;
}

int
main(int argc, char **argv)
{
	BOOL layerTest = argc > 1 && strcmp(argv[1], "-layer") == 0;
	unsigned frames = argc > 1 && !layerTest ? (unsigned)atoi(argv[1]) : 120;
	const char *path = argc > 2 ? argv[2] : "/usr/lib/libvulkan_radeon.dylib";
	char detail[512];
	VkResult r;

	setvbuf(stdout, NULL, _IOLBF, 0);
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = crashed;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGBUS, &sa, NULL);
	sigaction(SIGABRT, &sa, NULL);
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	/* The window and its view's layer. */
	[NSApplication sharedApplication];
	NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(200, 200, WIDTH, HEIGHT)
	    styleMask:NSTitledWindowMask | NSClosableWindowMask backing:NSBackingStoreBuffered defer:NO];
	[window setTitle:@"minkar-vkwindow"];
	VKView *view = [[VKView alloc] initWithFrame:NSMakeRect(0, 0, WIDTH, HEIGHT)];
	[view setWantsLayer:YES];
	[window setContentView:view];
	[window makeKeyAndOrderFront:nil];
	[NSApp activateIgnoringOtherApps:YES];
	spin(0.5);
	CAMetalLayer *layer = (CAMetalLayer *)[view layer];
	snprintf(detail, sizeof(detail), "%s, window %u", layer ? [NSStringFromClass([layer class]) UTF8String] : "none",
	    [window _skyLightWindowID]);
	check([layer isKindOfClass:[CAMetalLayer class]] && [window _skyLightWindowID] != 0,
	    "a window whose view has a CAMetalLayer", detail);
	if (![layer isKindOfClass:[CAMetalLayer class]])
		return failures;
	if (layerTest) {
		layerOnly(window, layer);
		const char *hold = getenv("MINKAR_VKWINDOW_HOLD");
		if (hold != NULL)
			spin(atof(hold));
		[pool release];
		return failures;
	}

	/* The driver, an instance with the surface extensions, the surface. */
	void *icd = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
	PFN_icdNegotiate negotiate = icd ? (PFN_icdNegotiate)dlsym(icd, "vk_icdNegotiateLoaderICDInterfaceVersion") : NULL;
	PFN_icdGetInstanceProcAddr gipa = icd ? (PFN_icdGetInstanceProcAddr)dlsym(icd, "vk_icdGetInstanceProcAddr") : NULL;
	uint32_t version = 5;
	if (negotiate == NULL || gipa == NULL || negotiate(&version) != VK_SUCCESS) {
		check(0, "the driver loads", icd ? "no loader interface" : dlerror());
		return failures;
	}
	GET(NULL, vkCreateInstance);
	const char *iext[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_METAL_SURFACE_EXTENSION_NAME };
	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "minkar-vkwindow", 1, NULL, 0,
	    VK_API_VERSION_1_1 };
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app, 0, NULL, 2, iext };
	VkInstance inst;
	r = vkCreateInstance(&ici, NULL, &inst);
	if (r != VK_SUCCESS) {
		/* What the driver offers instead. */
		GET(NULL, vkEnumerateInstanceExtensionProperties);
		VkExtensionProperties props[64];
		uint32_t pn = 64;
		size_t len = 0;
		vkEnumerateInstanceExtensionProperties(NULL, &pn, props);
		len = (size_t)snprintf(detail, sizeof(detail), "%d; offered:", r);
		for (uint32_t i = 0; i < pn && len < sizeof(detail); i++)
			if (strstr(props[i].extensionName, "surface") != NULL)
				len += (size_t)snprintf(detail + len, sizeof(detail) - len, " %s", props[i].extensionName);
		check(0, "an instance with VK_EXT_metal_surface", detail);
		return failures;
	}
	GET(inst, vkCreateMetalSurfaceEXT);
	GET(inst, vkEnumeratePhysicalDevices);
	GET(inst, vkGetPhysicalDeviceQueueFamilyProperties);
	GET(inst, vkGetPhysicalDeviceSurfaceSupportKHR);
	GET(inst, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
	GET(inst, vkGetPhysicalDeviceSurfaceFormatsKHR);
	GET(inst, vkCreateDevice);
	GET(inst, vkGetDeviceProcAddr);
	VkMetalSurfaceCreateInfoEXT msci = { VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT, NULL, 0, layer };
	VkSurfaceKHR surface;
	TRY("vkCreateMetalSurfaceEXT on the layer", vkCreateMetalSurfaceEXT(inst, &msci, NULL, &surface));
	check(1, "vkCreateMetalSurfaceEXT on the layer", "");

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
	for (uint32_t i = 0; i < qn && family == UINT32_MAX; i++) {
		VkBool32 present = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface, &present);
		if ((qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present)
			family = i;
	}
	VkSurfaceCapabilitiesKHR caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &caps);
	VkSurfaceFormatKHR formats[16];
	uint32_t fn = 16;
	vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &fn, formats);
	VkFormat format = VK_FORMAT_UNDEFINED;
	for (uint32_t i = 0; i < fn; i++)
		if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
			format = formats[i].format;
	snprintf(detail, sizeof(detail), "family %u, extent %ux%u, images %u-%u, %u formats", family,
	    caps.currentExtent.width, caps.currentExtent.height, caps.minImageCount, caps.maxImageCount, fn);
	check(family != UINT32_MAX && format != VK_FORMAT_UNDEFINED, "the surface: a queue presents to it, BGRA8",
	    detail);
	if (family == UINT32_MAX || format == VK_FORMAT_UNDEFINED)
		return failures;
	VkExtent2D extent = caps.currentExtent;
	if (extent.width == UINT32_MAX || extent.width == 0)
		extent = (VkExtent2D){ WIDTH, HEIGHT };

	float prio = 1.0f;
	const char *dext[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0, family, 1, &prio };
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0, 1, &qci, 0, NULL, 1, dext, NULL };
	VkDevice dev;
	TRY("a device with VK_KHR_swapchain", vkCreateDevice(pd, &dci, NULL, &dev));
	DEV(vkGetDeviceQueue);
	DEV(vkCreateSwapchainKHR);
	DEV(vkGetSwapchainImagesKHR);
	DEV(vkAcquireNextImageKHR);
	DEV(vkQueuePresentKHR);
	DEV(vkCreateImageView);
	DEV(vkCreateRenderPass);
	DEV(vkCreateFramebuffer);
	DEV(vkCreateShaderModule);
	DEV(vkCreatePipelineLayout);
	DEV(vkCreateGraphicsPipelines);
	DEV(vkCreateCommandPool);
	DEV(vkAllocateCommandBuffers);
	DEV(vkBeginCommandBuffer);
	DEV(vkResetCommandBuffer);
	DEV(vkCmdBeginRenderPass);
	DEV(vkCmdBindPipeline);
	DEV(vkCmdPushConstants);
	DEV(vkCmdDraw);
	DEV(vkCmdEndRenderPass);
	DEV(vkEndCommandBuffer);
	DEV(vkCreateFence);
	DEV(vkWaitForFences);
	DEV(vkResetFences);
	DEV(vkCreateSemaphore);
	DEV(vkQueueSubmit);
	DEV(vkDeviceWaitIdle);
	VkQueue q;
	vkGetDeviceQueue(dev, family, 0, &q);

	/* The swapchain and a framebuffer per image. */
	uint32_t want = caps.minImageCount < 2 ? 2 : caps.minImageCount;
	if (caps.maxImageCount != 0 && want > caps.maxImageCount)
		want = caps.maxImageCount;
	VkSwapchainCreateInfoKHR sci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, NULL, 0, surface, want, format,
	    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, extent, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
	    VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
	    VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_PRESENT_MODE_FIFO_KHR, VK_TRUE, VK_NULL_HANDLE };
	VkSwapchainKHR swapchain;
	TRY("a swapchain", vkCreateSwapchainKHR(dev, &sci, NULL, &swapchain));
	VkImage images[MAX_IMAGES];
	uint32_t count = MAX_IMAGES;
	vkGetSwapchainImagesKHR(dev, swapchain, &count, images);
	snprintf(detail, sizeof(detail), "%u images of %ux%u", count, extent.width, extent.height);
	check(count >= 2, "a swapchain", detail);

	VkAttachmentDescription att = { 0, format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
	    VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
	    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
	VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkSubpassDescription sub = { 0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, NULL, 1, &ref, NULL, NULL, 0, NULL };
	VkRenderPassCreateInfo rpci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 1, &att, 1, &sub, 0, NULL };
	VkRenderPass pass;
	TRY("a render pass", vkCreateRenderPass(dev, &rpci, NULL, &pass));
	VkFramebuffer fbs[MAX_IMAGES];
	for (uint32_t i = 0; i < count; i++) {
		VkImageViewCreateInfo ivci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, images[i],
		    VK_IMAGE_VIEW_TYPE_2D, format, { 0, 0, 0, 0 }, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
		VkImageView v;
		TRY("an image view", vkCreateImageView(dev, &ivci, NULL, &v));
		VkFramebufferCreateInfo fbci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, pass, 1, &v,
		    extent.width, extent.height, 1 };
		TRY("a framebuffer", vkCreateFramebuffer(dev, &fbci, NULL, &fbs[i]));
	}

	/* The triangle's pipeline (minkar-vkdraw's shaders). */
	VkShaderModuleCreateInfo vsci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0, sizeof(triangle_vert),
	    triangle_vert };
	VkShaderModuleCreateInfo fsci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0, sizeof(triangle_frag),
	    triangle_frag };
	VkShaderModule vs, fs;
	vkCreateShaderModule(dev, &vsci, NULL, &vs);
	vkCreateShaderModule(dev, &fsci, NULL, &fs);
	VkPushConstantRange pcr = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16 };
	VkPipelineLayoutCreateInfo plci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL, 0, 0, NULL, 1, &pcr };
	VkPipelineLayout playout;
	vkCreatePipelineLayout(dev, &plci, NULL, &playout);
	VkPipelineShaderStageCreateInfo stages[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main", NULL },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", NULL },
	};
	VkPipelineVertexInputStateCreateInfo vi = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL,
	    0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE };
	VkViewport vp = { 0, 0, extent.width, extent.height, 0, 1 };
	VkRect2D scissor = { { 0, 0 }, extent };
	VkPipelineViewportStateCreateInfo vps = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0, 1,
	    &vp, 1, &scissor };
	VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, NULL,
	    0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_FALSE, 0, 0, 0,
	    1.0f };
	VkPipelineMultisampleStateCreateInfo ms = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
	    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
	VkPipelineColorBlendAttachmentState cba = { VK_FALSE, 0, 0, 0, 0, 0, 0, 0xf };
	VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL, 0,
	    VK_FALSE, VK_LOGIC_OP_COPY, 1, &cba, { 0, 0, 0, 0 } };
	VkGraphicsPipelineCreateInfo gpci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, NULL, 0, 2, stages, &vi,
	    &ia, NULL, &vps, &rs, &ms, NULL, &cb, NULL, playout, pass, 0, VK_NULL_HANDLE, -1 };
	VkPipeline pipeline;
	TRY("a graphics pipeline", vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpci, NULL, &pipeline));

	VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
	    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, family };
	VkCommandPool cpool;
	vkCreateCommandPool(dev, &pci, NULL, &cpool);
	VkCommandBufferAllocateInfo cai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, cpool,
	    VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(dev, &cai, &cmd);
	VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0 };
	VkFence fence;
	vkCreateFence(dev, &fci, NULL, &fence);
	VkSemaphoreCreateInfo semci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, 0 };
	VkSemaphore acquired, rendered;
	vkCreateSemaphore(dev, &semci, NULL, &acquired);
	vkCreateSemaphore(dev, &semci, NULL, &rendered);

	/* The frames: a blue that brightens, the red triangle over it. */
	const float red[4] = { 1, 0, 0, 1 };
	unsigned presented = 0, last = 0;
	VkResult bad = VK_SUCCESS;
	double start = now();
	for (unsigned f = 0; f < frames; f++) {
		uint32_t index;
		r = vkAcquireNextImageKHR(dev, swapchain, 2000000000ull, acquired, VK_NULL_HANDLE, &index);
		if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
			bad = r;
			break;
		}
		vkResetCommandBuffer(cmd, 0);
		VkCommandBufferBeginInfo begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
		    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, NULL };
		vkBeginCommandBuffer(cmd, &begin);
		last = 64 + (f * 191) / (frames > 1 ? frames - 1 : 1);	/* blue 64..255 */
		VkClearValue clear = { .color = { .float32 = { 0, 0, last / 255.0f, 1 } } };
		VkRenderPassBeginInfo rpbi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL, pass, fbs[index], scissor,
		    1, &clear };
		vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdPushConstants(cmd, playout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(red), red);
		vkCmdDraw(cmd, 3, 1, 0, 0);
		vkCmdEndRenderPass(cmd);
		vkEndCommandBuffer(cmd);
		VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 1, &acquired, &stage, 1, &cmd, 1, &rendered };
		r = vkQueueSubmit(q, 1, &si, fence);
		if (r != VK_SUCCESS) {
			bad = r;
			break;
		}
		VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, NULL, 1, &rendered, 1, &swapchain, &index,
		    NULL };
		r = vkQueuePresentKHR(q, &pi);
		if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
			bad = r;
			break;
		}
		vkWaitForFences(dev, 1, &fence, VK_TRUE, 2000000000ull);
		vkResetFences(dev, 1, &fence);
		presented++;
		if ((f & 15) == 0)
			spin(0);
	}
	double seconds = now() - start;
	vkDeviceWaitIdle(dev);
	snprintf(detail, sizeof(detail), "%u of %u (last result %d), %.1f frames a second", presented, frames, bad,
	    seconds > 0 ? presented / seconds : 0);
	check(presented == frames, "frames drawn and presented", detail);

	/* The last frame where the window server has it. */
	uint32_t center = 0, corner = 0;
	char where[128];
	if (windowPixel(window, extent.width / 2, extent.height / 2, &center, where, sizeof(where)) &&
	    windowPixel(window, 2, 2, &corner, where, sizeof(where))) {
		/* BGRA in memory: blue in the low byte. */
		snprintf(detail, sizeof(detail), "%s; center 0x%08x, corner 0x%08x (blue %u)", where, center, corner,
		    last);
		check((center & 0x00ffffff) == 0x00ff0000 && (corner & 0x00ffffff) == last,
		    "the window shows the last frame: the red triangle on its blue", detail);
	} else
		check(0, "the window's pixels", where);
	const char *hold = getenv("MINKAR_VKWINDOW_HOLD");
	if (hold != NULL)
		spin(atof(hold));
	[pool release];
	return failures;
}
