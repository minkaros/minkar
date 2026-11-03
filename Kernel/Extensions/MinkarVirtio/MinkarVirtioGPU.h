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
 * MinkarVirtioGPU: a virtio-gpu display in 2D mode, as an IOFramebuffer
 * (roadmap step 3; WindowServer plan, WS5). QEMU's virtio-vga and
 * virtio-gpu-pci. The framebuffer is guest memory attached to a host
 * resource that is scanned out; what is drawn reaches the screen when it
 * is transferred to the host and flushed.
 */

#ifndef _MINKARVIRTIOGPU_H
#define _MINKARVIRTIOGPU_H

#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/pci/IOPCIDevice.h>

class MinkarVirtioPCI;
class MinkarVirtQueue;

class MinkarVirtioGPU : public IOFramebuffer
{
	OSDeclareDefaultStructors(MinkarVirtioGPU);

public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void free() APPLE_KEXT_OVERRIDE;

	virtual IOReturn enableController() APPLE_KEXT_OVERRIDE;
	virtual const char *getPixelFormats() APPLE_KEXT_OVERRIDE;
	virtual IOItemCount getDisplayModeCount(void) APPLE_KEXT_OVERRIDE;
	virtual IOReturn getDisplayModes(IODisplayModeID *allDisplayModes) APPLE_KEXT_OVERRIDE;
	virtual IOReturn getInformationForDisplayMode(IODisplayModeID displayMode,
	    IODisplayModeInformation *info) APPLE_KEXT_OVERRIDE;
	virtual UInt64 getPixelFormatsForDisplayMode(IODisplayModeID displayMode,
	    IOIndex depth) APPLE_KEXT_OVERRIDE;
	virtual IOReturn getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
	    IOPixelAperture aperture, IOPixelInformation *info) APPLE_KEXT_OVERRIDE;
	virtual IOReturn getCurrentDisplayMode(IODisplayModeID *displayMode,
	    IOIndex *depth) APPLE_KEXT_OVERRIDE;
	virtual IOReturn setDisplayMode(IODisplayModeID displayMode, IOIndex depth) APPLE_KEXT_OVERRIDE;
	virtual IODeviceMemory *getApertureRange(IOPixelAperture aperture) APPLE_KEXT_OVERRIDE;
	virtual bool isConsoleDevice(void) APPLE_KEXT_OVERRIDE;
	virtual IOReturn setAttribute(IOSelect attribute, uintptr_t value) APPLE_KEXT_OVERRIDE;
	virtual void close(void) APPLE_KEXT_OVERRIDE;

private:
	struct Mode {
		UInt32 width, height;
	};
	enum { kMaxModes = 16 };

	bool setupDevice();
	bool command(const void *request, size_t requestLength, void *response,
	    size_t responseLength);
	bool readDisplayInfo(UInt32 *width, UInt32 *height);
	bool showMode(unsigned index);
	void flushAll();
	void flushRect(UInt32 x, UInt32 y, UInt32 width, UInt32 height);
	void timerFired(IOTimerEventSource *sender);
	const Mode *modeFor(IODisplayModeID id);

	IOPCIDevice *			fPCI;
	MinkarVirtioPCI *		fVirtio;
	MinkarVirtQueue *		fQueue;
	IOBufferMemoryDescriptor *	fCommand;	/* request and response */
	IOLock *			fLock;
	IOWorkLoop *			fWorkLoop;
	IOTimerEventSource *		fTimer;

	Mode				fModes[kMaxModes];
	unsigned			fModeCount;
	unsigned			fCurrent;	/* index in fModes */
	IOBufferMemoryDescriptor *	fBacking;	/* the current mode's pixels */
	UInt32				fResource;	/* its host resource, or 0 */
	UInt32				fNextResource;
	UInt32				fFlushOrigin;	/* x << 16 | y */
	bool				fServerFlushes;	/* else the timer does */
};

#endif
