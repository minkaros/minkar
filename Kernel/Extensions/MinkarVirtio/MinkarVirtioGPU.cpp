/*
 * SPDX-License-Identifier: BSD-2-Clause
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
 * The 2D commands of the virtio specification (1.1, section 5.7), on the
 * PCI transport of MinkarVirtio: one control queue, each command waited
 * for. SHORTCUT(WS5): the whole screen is transferred and flushed at the
 * refresh rate, until the window server sends its damage with each frame;
 * taken in WS5.
 */

#include "MinkarVirtioGPU.h"
#include "MinkarVirtioPCI.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <libkern/OSByteOrder.h>

#define LOG(fmt, ...)	IOLog("MinkarVirtioGPU: " fmt "\n", ##__VA_ARGS__)

/* Command types and responses. */
enum {
	VIRTIO_GPU_CMD_GET_DISPLAY_INFO		= 0x0100,
	VIRTIO_GPU_CMD_RESOURCE_CREATE_2D	= 0x0101,
	VIRTIO_GPU_CMD_RESOURCE_UNREF		= 0x0102,
	VIRTIO_GPU_CMD_SET_SCANOUT		= 0x0103,
	VIRTIO_GPU_CMD_RESOURCE_FLUSH		= 0x0104,
	VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D	= 0x0105,
	VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING	= 0x0106,
	VIRTIO_GPU_RESP_OK_NODATA		= 0x1100,
	VIRTIO_GPU_RESP_OK_DISPLAY_INFO		= 0x1101,
};

/* Memory order B, G, R, X: the 32-bit pixels the window server writes. */
#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM	2
#define VIRTIO_GPU_MAX_SCANOUTS			16

struct virtio_gpu_ctrl_hdr {
	uint32_t type, flags;
	uint64_t fence_id;
	uint32_t ctx_id;
	uint8_t ring_idx, padding[3];
} __attribute__((packed));

struct virtio_gpu_rect {
	uint32_t x, y, width, height;
} __attribute__((packed));

struct virtio_gpu_resp_display_info {
	struct virtio_gpu_ctrl_hdr hdr;
	struct {
		struct virtio_gpu_rect r;
		uint32_t enabled, flags;
	} pmodes[VIRTIO_GPU_MAX_SCANOUTS];
} __attribute__((packed));

struct virtio_gpu_resource_create_2d {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t resource_id, format, width, height;
} __attribute__((packed));

struct virtio_gpu_resource_unref {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t resource_id, padding;
} __attribute__((packed));

struct virtio_gpu_resource_attach_backing {
	struct virtio_gpu_ctrl_hdr hdr;
	uint32_t resource_id, nr_entries;
	struct {
		uint64_t addr;
		uint32_t length, padding;
	} entry;
} __attribute__((packed));

struct virtio_gpu_set_scanout {
	struct virtio_gpu_ctrl_hdr hdr;
	struct virtio_gpu_rect r;
	uint32_t scanout_id, resource_id;
} __attribute__((packed));

struct virtio_gpu_transfer_to_host_2d {
	struct virtio_gpu_ctrl_hdr hdr;
	struct virtio_gpu_rect r;
	uint64_t offset;
	uint32_t resource_id, padding;
} __attribute__((packed));

struct virtio_gpu_resource_flush {
	struct virtio_gpu_ctrl_hdr hdr;
	struct virtio_gpu_rect r;
	uint32_t resource_id, padding;
} __attribute__((packed));

/* The request in the first half of the command page, the response in the
 * second. */
#define kCommandSize	8192
#define kResponseOffset	4096
#define kDepthIndex	0
#define kRefreshMS	16

#define super IOFramebuffer
OSDefineMetaClassAndStructors(MinkarVirtioGPU, IOFramebuffer);

static const struct { UInt32 width, height; } kStandardModes[] = {
	{ 1024, 768 }, { 1280, 800 }, { 1280, 1024 }, { 1440, 900 }, { 1600, 900 },
	{ 1680, 1050 }, { 1920, 1080 }, { 1920, 1200 }, { 2560, 1440 },
};

bool
MinkarVirtioGPU::start(IOService *provider)
{
	fPCI = OSDynamicCast(IOPCIDevice, provider);
	if (fPCI == NULL || !super::start(provider))
		return false;
	fPCI->retain();
	fLock = IOLockAlloc();
	if (fLock == NULL || !setupDevice()) {
		LOG("cannot set up the device");
		return false;
	}
	setupForCurrentConfig();
	registerService();
	return true;
}

bool
MinkarVirtioGPU::setupDevice()
{
	uint64_t features = 0;
	UInt32 width = 0, height = 0;
	PE_Video boot;

	fVirtio = MinkarVirtioPCI::withDevice(fPCI);
	if (fVirtio == NULL || !fVirtio->begin(0, &features))
		return false;
	UInt16 qsize = fVirtio->getQueueMaxSize(0);
	if (qsize == 0)
		return false;
	if (qsize > 64)
		qsize = 64;
	fQueue = MinkarVirtQueue::withSize(0, qsize);
	if (fQueue == NULL || !fVirtio->setupQueue(fQueue))
		return false;
	fCommand = MinkarVirtioAllocDMA(kCommandSize);
	if (fCommand == NULL)
		return false;
	fVirtio->driverOK();

	/* The modes: the scanout's preferred size, then the usual ones. */
	readDisplayInfo(&width, &height);
	fModeCount = 0;
	if (width >= 640 && height >= 480) {
		fModes[fModeCount].width = width;
		fModes[fModeCount].height = height;
		fModeCount++;
	}
	for (unsigned i = 0; i < sizeof(kStandardModes) / sizeof(kStandardModes[0]) &&
	    fModeCount < kMaxModes; i++) {
		if (kStandardModes[i].width == width && kStandardModes[i].height == height)
			continue;
		fModes[fModeCount].width = kStandardModes[i].width;
		fModes[fModeCount].height = kStandardModes[i].height;
		fModeCount++;
	}
	/* The firmware's size first, so the console does not jump. */
	fCurrent = 0;
	if (getPlatform() != NULL && getPlatform()->getConsoleInfo(&boot) == kIOReturnSuccess)
		for (unsigned i = 0; i < fModeCount; i++)
			if (fModes[i].width == boot.v_width && fModes[i].height == boot.v_height)
				fCurrent = i;
	fNextResource = 1;
	if (!showMode(fCurrent))
		return false;

	fWorkLoop = IOWorkLoop::workLoop();
	fTimer = IOTimerEventSource::timerEventSource(this,
	    OSMemberFunctionCast(IOTimerEventSource::Action, this, &MinkarVirtioGPU::timerFired));
	if (fWorkLoop == NULL || fTimer == NULL ||
	    fWorkLoop->addEventSource(fTimer) != kIOReturnSuccess)
		return false;
	fTimer->setTimeoutMS(kRefreshMS);
	LOG("%ux%u, %u modes", fModes[fCurrent].width, fModes[fCurrent].height, fModeCount);
	return true;
}

/* Sends one command and waits for its response (QEMU answers at once). */
bool
MinkarVirtioGPU::command(const void *request, size_t requestLength, void *response,
    size_t responseLength)
{
	uint8_t *page = (uint8_t *)fCommand->getBytesNoCopy();
	uint64_t phys = fCommand->getPhysicalAddress();
	uint64_t addrs[2] = { phys, phys + kResponseOffset };
	uint32_t lens[2] = { (uint32_t)requestLength, (uint32_t)responseLength };
	uint32_t len;
	bool ok = false;

	if (requestLength > kResponseOffset || responseLength > kCommandSize - kResponseOffset)
		return false;
	IOLockLock(fLock);
	memcpy(page, request, requestLength);
	bzero(page + kResponseOffset, responseLength);
	if (fQueue->addChain(addrs, lens, 1, 1, page)) {
		fVirtio->notify(fQueue);
		for (int i = 0; i < 100000; i++) {
			if (fQueue->getUsed(&len) != NULL) {
				ok = true;
				break;
			}
			IODelay(10);
		}
		if (ok) {
			memcpy(response, page + kResponseOffset, responseLength);
			(void)fVirtio->readISR();
		}
	}
	IOLockUnlock(fLock);
	if (!ok)
		LOG("command 0x%x: no response", ((const struct virtio_gpu_ctrl_hdr *)request)->type);
	return ok;
}

static bool
okNoData(const struct virtio_gpu_ctrl_hdr *response)
{
	return OSSwapLittleToHostInt32(response->type) == VIRTIO_GPU_RESP_OK_NODATA;
}

bool
MinkarVirtioGPU::readDisplayInfo(UInt32 *width, UInt32 *height)
{
	struct virtio_gpu_ctrl_hdr request;
	struct virtio_gpu_resp_display_info info;

	bzero(&request, sizeof(request));
	request.type = OSSwapHostToLittleInt32(VIRTIO_GPU_CMD_GET_DISPLAY_INFO);
	if (!command(&request, sizeof(request), &info, sizeof(info)) ||
	    OSSwapLittleToHostInt32(info.hdr.type) != VIRTIO_GPU_RESP_OK_DISPLAY_INFO)
		return false;
	if (!info.pmodes[0].enabled)
		return false;
	*width = OSSwapLittleToHostInt32(info.pmodes[0].r.width);
	*height = OSSwapLittleToHostInt32(info.pmodes[0].r.height);
	return true;
}

/* A new resource of the mode's size, its pixels attached and scanned out;
 * the previous one goes. The console draws into it. */
bool
MinkarVirtioGPU::showMode(unsigned index)
{
	const Mode *m = &fModes[index];
	size_t length = (size_t)m->width * m->height * 4;
	UInt32 resource = fNextResource++;
	struct virtio_gpu_ctrl_hdr response;

	IOBufferMemoryDescriptor *backing = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
	    kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
	    length, 0xFFFFFFFFFFFFF000ULL);
	if (backing == NULL)
		return false;
	bzero(backing->getBytesNoCopy(), length);

	struct virtio_gpu_resource_create_2d create;
	bzero(&create, sizeof(create));
	create.hdr.type = OSSwapHostToLittleInt32(VIRTIO_GPU_CMD_RESOURCE_CREATE_2D);
	create.resource_id = OSSwapHostToLittleInt32(resource);
	create.format = OSSwapHostToLittleInt32(VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM);
	create.width = OSSwapHostToLittleInt32(m->width);
	create.height = OSSwapHostToLittleInt32(m->height);
	struct virtio_gpu_resource_attach_backing attach;
	bzero(&attach, sizeof(attach));
	attach.hdr.type = OSSwapHostToLittleInt32(VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING);
	attach.resource_id = OSSwapHostToLittleInt32(resource);
	attach.nr_entries = OSSwapHostToLittleInt32(1);
	attach.entry.addr = OSSwapHostToLittleInt64(backing->getPhysicalAddress());
	attach.entry.length = OSSwapHostToLittleInt32((uint32_t)length);
	struct virtio_gpu_set_scanout scanout;
	bzero(&scanout, sizeof(scanout));
	scanout.hdr.type = OSSwapHostToLittleInt32(VIRTIO_GPU_CMD_SET_SCANOUT);
	scanout.r.width = OSSwapHostToLittleInt32(m->width);
	scanout.r.height = OSSwapHostToLittleInt32(m->height);
	scanout.resource_id = OSSwapHostToLittleInt32(resource);
	if (!command(&create, sizeof(create), &response, sizeof(response)) || !okNoData(&response) ||
	    !command(&attach, sizeof(attach), &response, sizeof(response)) || !okNoData(&response) ||
	    !command(&scanout, sizeof(scanout), &response, sizeof(response)) || !okNoData(&response)) {
		LOG("cannot show %ux%u", m->width, m->height);
		backing->release();
		return false;
	}
	if (fResource != 0) {
		struct virtio_gpu_resource_unref unref;
		bzero(&unref, sizeof(unref));
		unref.hdr.type = OSSwapHostToLittleInt32(VIRTIO_GPU_CMD_RESOURCE_UNREF);
		unref.resource_id = OSSwapHostToLittleInt32(fResource);
		command(&unref, sizeof(unref), &response, sizeof(response));
	}
	IOLockLock(fLock);
	OSSafeReleaseNULL(fBacking);
	fBacking = backing;
	fResource = resource;
	fCurrent = index;
	IOLockUnlock(fLock);

	/* The geometry, as IOGOPFramebuffer publishes it for hardware checks. */
	setProperty("MinkarFramebufferAddress", (unsigned long long)backing->getPhysicalAddress(), 64);
	setProperty("MinkarFramebufferWidth", (unsigned long long)m->width, 32);
	setProperty("MinkarFramebufferHeight", (unsigned long long)m->height, 32);
	setProperty("MinkarFramebufferRowBytes", (unsigned long long)m->width * 4, 32);

	/* The kernel's console draws here now. */
	PE_Video console;
	bzero(&console, sizeof(console));
	console.v_baseAddr = (unsigned long)backing->getPhysicalAddress() | 1;
	console.v_width = m->width;
	console.v_height = m->height;
	console.v_depth = 32;
	console.v_rowBytes = m->width * 4;
	console.v_display = GRAPHICS_MODE;
	console.v_scale = kPEScaleFactor1x;
	if (getPlatform() != NULL) {
		getPlatform()->setConsoleInfo(&console, kPEAcquireScreen);
		getPlatform()->setConsoleInfo(&console, kPEGraphicsMode);
		getPlatform()->setConsoleInfo(&console, kPEEnableScreen);
	}
	flushAll();
	return true;
}

void
MinkarVirtioGPU::flushAll()
{
	struct virtio_gpu_ctrl_hdr response;
	const Mode *m = &fModes[fCurrent];

	if (fResource == 0)
		return;
	struct virtio_gpu_transfer_to_host_2d transfer;
	bzero(&transfer, sizeof(transfer));
	transfer.hdr.type = OSSwapHostToLittleInt32(VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D);
	transfer.r.width = OSSwapHostToLittleInt32(m->width);
	transfer.r.height = OSSwapHostToLittleInt32(m->height);
	transfer.resource_id = OSSwapHostToLittleInt32(fResource);
	struct virtio_gpu_resource_flush flush;
	bzero(&flush, sizeof(flush));
	flush.hdr.type = OSSwapHostToLittleInt32(VIRTIO_GPU_CMD_RESOURCE_FLUSH);
	flush.r.width = OSSwapHostToLittleInt32(m->width);
	flush.r.height = OSSwapHostToLittleInt32(m->height);
	flush.resource_id = OSSwapHostToLittleInt32(fResource);
	command(&transfer, sizeof(transfer), &response, sizeof(response));
	command(&flush, sizeof(flush), &response, sizeof(response));
}

void
MinkarVirtioGPU::timerFired(IOTimerEventSource *)
{
	flushAll();
	fTimer->setTimeoutMS(kRefreshMS);
}

void
MinkarVirtioGPU::stop(IOService *provider)
{
	if (fTimer != NULL) {
		fTimer->cancelTimeout();
		if (fWorkLoop != NULL)
			fWorkLoop->removeEventSource(fTimer);
	}
	super::stop(provider);
}

void
MinkarVirtioGPU::free()
{
	OSSafeReleaseNULL(fTimer);
	OSSafeReleaseNULL(fWorkLoop);
	OSSafeReleaseNULL(fBacking);
	OSSafeReleaseNULL(fCommand);
	OSSafeReleaseNULL(fQueue);
	OSSafeReleaseNULL(fVirtio);
	OSSafeReleaseNULL(fPCI);
	if (fLock != NULL)
		IOLockFree(fLock);
	super::free();
}

/* ---- IOFramebuffer ----------------------------------------------------- */

IOReturn
MinkarVirtioGPU::enableController()
{
	return kIOReturnSuccess;
}

const char *
MinkarVirtioGPU::getPixelFormats()
{
	return IO32BitDirectPixels "\0";
}

IOItemCount
MinkarVirtioGPU::getDisplayModeCount(void)
{
	return fModeCount;
}

/* Mode IDs are 1 and up, in fModes order. */
IOReturn
MinkarVirtioGPU::getDisplayModes(IODisplayModeID *allDisplayModes)
{
	for (unsigned i = 0; i < fModeCount; i++)
		allDisplayModes[i] = (IODisplayModeID)(i + 1);
	return kIOReturnSuccess;
}

const MinkarVirtioGPU::Mode *
MinkarVirtioGPU::modeFor(IODisplayModeID id)
{
	if (id < 1 || (unsigned)id > fModeCount)
		return NULL;
	return &fModes[id - 1];
}

IOReturn
MinkarVirtioGPU::getInformationForDisplayMode(IODisplayModeID displayMode,
    IODisplayModeInformation *info)
{
	const Mode *m = modeFor(displayMode);

	if (m == NULL)
		return kIOReturnUnsupportedMode;
	bzero(info, sizeof(*info));
	info->nominalWidth = m->width;
	info->nominalHeight = m->height;
	info->refreshRate = 60 << 16;
	info->maxDepthIndex = kDepthIndex;
	return kIOReturnSuccess;
}

UInt64
MinkarVirtioGPU::getPixelFormatsForDisplayMode(IODisplayModeID, IOIndex)
{
	return 0;
}

IOReturn
MinkarVirtioGPU::getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
    IOPixelAperture aperture, IOPixelInformation *info)
{
	const Mode *m = modeFor(displayMode);

	if (m == NULL || aperture != kIOFBSystemAperture || depth != kDepthIndex)
		return kIOReturnUnsupportedMode;
	bzero(info, sizeof(*info));
	info->activeWidth = m->width;
	info->activeHeight = m->height;
	info->bytesPerRow = m->width * 4;
	strlcpy(info->pixelFormat, IO32BitDirectPixels, sizeof(info->pixelFormat));
	info->pixelType = kIORGBDirectPixels;
	info->componentMasks[0] = 0x00ff0000;
	info->componentMasks[1] = 0x0000ff00;
	info->componentMasks[2] = 0x000000ff;
	info->bitsPerPixel = 32;
	info->componentCount = 3;
	info->bitsPerComponent = 8;
	return kIOReturnSuccess;
}

IOReturn
MinkarVirtioGPU::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth)
{
	if (displayMode)
		*displayMode = (IODisplayModeID)(fCurrent + 1);
	if (depth)
		*depth = kDepthIndex;
	return kIOReturnSuccess;
}

IOReturn
MinkarVirtioGPU::setDisplayMode(IODisplayModeID displayMode, IOIndex depth)
{
	if (modeFor(displayMode) == NULL || depth != kDepthIndex)
		return kIOReturnUnsupportedMode;
	if ((unsigned)displayMode - 1 == fCurrent)
		return kIOReturnSuccess;
	return showMode((unsigned)displayMode - 1) ? kIOReturnSuccess : kIOReturnNoMemory;
}

IODeviceMemory *
MinkarVirtioGPU::getApertureRange(IOPixelAperture)
{
	IODeviceMemory *range = NULL;

	IOLockLock(fLock);
	if (fBacking != NULL)
		range = IODeviceMemory::withRange(fBacking->getPhysicalAddress(),
		    fBacking->getLength());
	IOLockUnlock(fLock);
	return range;
}

bool
MinkarVirtioGPU::isConsoleDevice(void)
{
	return true;
}
