/*
 * IOGOPFramebuffer.cpp: minimal boot framebuffer using EFI GOP
 *
 * Copyright (C) 2025-2026 Zoe Knox. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "IOGOPFramebuffer.h"
#include <IOKit/pci/IOPCIDevice.h>

#define kDisplayModeID 1
/* Depths are indexes into a mode's list; GOP has one, 32 bits per pixel. */
#define kDepthIndex    0

#define DEBUG(x, ...) kprintf("IOGOPFramebuffer: " x, ##__VA_ARGS__)

#define super IOFramebuffer
OSDefineMetaClassAndStructors(IOGOPFramebuffer, IOFramebuffer);

/*
 * Minkar: only the card whose memory window holds the firmware's
 * framebuffer drives it. A machine with two GPUs (PC 2, two FirePro D500) matched
 * one IOGOPFramebuffer on each, both claiming the same framebuffer.
 * A window's base is read from the card's BAR; its size from "reg", which
 * PCI configuration publishes even when it leaves the firmware's
 * assignment in place.
 */
IOService *
IOGOPFramebuffer::probe(IOService *provider, SInt32 *score)
{
    PE_Video boot;
    IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
    OSData *reg;

    if (pci == NULL)
        return this;
    if (getPlatform() == NULL || getPlatform()->getConsoleInfo(&boot) != kIOReturnSuccess ||
        boot.v_baseAddr == 0)
        return NULL;
    uint64_t fb = boot.v_baseAddr & ~(uint64_t)0xFFF;
    reg = OSDynamicCast(OSData, pci->getProperty("reg"));
    if (reg == NULL)
        return this;
    const IOPCIPhysicalAddress *e = (const IOPCIPhysicalAddress *)reg->getBytesNoCopy();
    unsigned n = reg->getLength() / sizeof(IOPCIPhysicalAddress);
    for (unsigned i = 0; i < n; i++) {
        unsigned bar = e[i].physHi.s.registerNum;
        if (bar < kIOPCIConfigBaseAddress0 || bar > kIOPCIConfigBaseAddress5 ||
            e[i].physHi.s.space < kIOPCI32BitMemorySpace)
            continue;
        uint32_t lo = pci->configRead32(bar);
        uint64_t base = lo & ~(uint64_t)0xF;
        if ((lo & 0x6) == 0x4 && bar < kIOPCIConfigBaseAddress5)
            base |= (uint64_t)pci->configRead32(bar + 4) << 32;
        uint64_t size = ((uint64_t)e[i].lengthHi << 32) | e[i].lengthLo;
        if (base != 0 && fb >= base && fb < base + size) {
            setProperty("MinkarFramebufferWindowBase", (unsigned long long)base, 64);
            setProperty("MinkarFramebufferWindowSize", (unsigned long long)size, 64);
            return this;
        }
    }
    IOLog("IOGOPFramebuffer: %s does not decode the framebuffer at 0x%llx\n",
        pci->getName(), (unsigned long long)fb);
    return NULL;
}

bool
IOGOPFramebuffer::start(IOService *provider)
{
    if (!super::start(provider)) {
        DEBUG("super::start failed\n");
        return false;
    }

    PE_Video    bootDisplay;
    IOPlatformExpert *pe = getPlatform();
    if (!pe) {
        DEBUG("getPlatform() returned null\n");
        return false;
    }
    IOReturn err = pe->getConsoleInfo( &bootDisplay);
    if (err != kIOReturnSuccess) {
        DEBUG("getConsoleInfo failed: %d\n", err);
        return false;
    }
    if (bootDisplay.v_baseAddr == 0) {
        DEBUG("no framebuffer base address\n");
        return false;
    }
    fbBase = (void *)bootDisplay.v_baseAddr;
    width  = bootDisplay.v_width;
    height = bootDisplay.v_height;
    pitch  = bootDisplay.v_rowBytes;
    bpp    = 32;

    IOLog("IOFB: GOP framebuffer %dx%d @ %p\n", width, height, fbBase);
    // Minkar: the geometry the firmware gave and the console uses, for
    // hardware checks (minkar-hwcheck --display): the address must lie in
    // a window the GPU decodes.
    setProperty("MinkarFramebufferAddress", (unsigned long long)(uintptr_t)fbBase, 64);
    setProperty("MinkarFramebufferWidth", (unsigned long long)width, 32);
    setProperty("MinkarFramebufferHeight", (unsigned long long)height, 32);
    setProperty("MinkarFramebufferRowBytes", (unsigned long long)pitch, 32);

    // Register the framebuffer with the kernel console system
    PE_Video consoleInfo;
    consoleInfo.v_baseAddr   = bootDisplay.v_baseAddr | 1;  // Set low bit to force mapping
    consoleInfo.v_width      = bootDisplay.v_width;
    consoleInfo.v_height     = bootDisplay.v_height;
    consoleInfo.v_depth      = 32;  // GOP is always 32-bit
    consoleInfo.v_rowBytes   = bootDisplay.v_rowBytes;
    consoleInfo.v_display    = GRAPHICS_MODE;
    consoleInfo.v_offset     = 0;
    consoleInfo.v_length     = 0;  // Let kernel calculate from height * rowBytes
    consoleInfo.v_rotate     = 0;
    consoleInfo.v_scale      = kPEScaleFactor1x;

    // Initialize graphics console with this framebuffer
    // Use the public IOPlatformExpert::setConsoleInfo() method
    IOReturn ret = pe->setConsoleInfo(&consoleInfo, kPEAcquireScreen)
                | pe->setConsoleInfo(&consoleInfo, kPEGraphicsMode)
                | pe->setConsoleInfo(&consoleInfo, kPEEnableScreen);
    if (ret != kIOReturnSuccess) {
        DEBUG("setConsoleInfo failed: %d\n", ret);
        // Don't fail - we can still register the service even if console init fails
    } else {
        DEBUG("Kernel graphics console initialized\n");
    }

    setupForCurrentConfig(); // full setup
    registerService();
    return true;
}

void
IOGOPFramebuffer::stop(IOService *provider)
{
    DEBUG("stop %p\n", provider);
    super::stop(provider);
}

void * IOGOPFramebuffer::getBaseAddress(void) { return fbBase; }
uint32_t IOGOPFramebuffer::getWidth(void) { return width; }
uint32_t IOGOPFramebuffer::getHeight(void) { return height; }
uint32_t IOGOPFramebuffer::getPitch(void) { return pitch; }
uint32_t IOGOPFramebuffer::getDepth(void) { return bpp; }


IOReturn
IOGOPFramebuffer::enableController()
{
    return kIOReturnSuccess;
}

const char *
IOGOPFramebuffer::getPixelFormats()
{
    const char *        ret;
    PE_Video            bootDisplay;

    getPlatform()->getConsoleInfo( &bootDisplay);

    switch (bootDisplay.v_depth)
    {
        case 8:
        default:
            ret = IO8BitIndexedPixels;
            break;
        case 15:
        case 16:
            ret = IO16BitDirectPixels;
            break;
        case 24:
        case 32:
            ret = IO32BitDirectPixels;
            break;
    }

    return (ret);
}

IOReturn
IOGOPFramebuffer::getCurrentDisplayMode(IODisplayModeID * displayMode,
                                        IOIndex         * depth)
{
    *displayMode = kDisplayModeID;
    *depth = kDepthIndex;
    return kIOReturnSuccess;
}

IOReturn
IOGOPFramebuffer::setDisplayMode(IODisplayModeID displayMode,
                                 IOIndex depth)
{
    return kIOReturnSuccess;
}

IODeviceMemory *
IOGOPFramebuffer::getApertureRange(IOPixelAperture)
{
    return IODeviceMemory::withRange(
        (mach_vm_address_t)fbBase,
        pitch * height
    );
}

IOReturn
IOGOPFramebuffer::getInformationForDisplayMode(IODisplayModeID,
                                               IODisplayModeInformation * info)
{
    bzero(info, sizeof(*info));

    info->nominalWidth  = width;
    info->nominalHeight = height;
    info->refreshRate   = 60 << 16;

    info->maxDepthIndex = kDepthIndex;

    return kIOReturnSuccess;
}

UInt64
IOGOPFramebuffer::getPixelFormatsForDisplayMode(IODisplayModeID,
                                                IOIndex)
{
    return (UInt64)(uintptr_t)getPixelFormats();
}

IOItemCount
IOGOPFramebuffer::getDisplayModeCount(void)
{
    return 1;
}

IOReturn
IOGOPFramebuffer::getDisplayModes(IODisplayModeID * allDisplayModes)
{
    *allDisplayModes = kDisplayModeID;
    return kIOReturnSuccess;
}

IOReturn
IOGOPFramebuffer::getPixelInformation(IODisplayModeID      displayMode,
                                      IOIndex              depth,
                                      IOPixelAperture      aperture,
                                      IOPixelInformation * info)
{
    PE_Video    bootDisplay;

    if (aperture || (depth != kDepthIndex) || (displayMode != kDisplayModeID))
    {
        return (kIOReturnUnsupportedMode);
    }

    getPlatform()->getConsoleInfo( &bootDisplay);

    bzero( info, sizeof(*info));

    info->activeWidth           = static_cast<UInt32>(bootDisplay.v_width);
    info->activeHeight          = static_cast<UInt32>(bootDisplay.v_height);
    info->bytesPerRow           = bootDisplay.v_rowBytes & 0x7fff;
    info->bytesPerPlane         = 0;

    switch (bootDisplay.v_depth)
    {
        case 8:
        default:
            strlcpy(info->pixelFormat, IO8BitIndexedPixels, sizeof(info->pixelFormat));
            info->pixelType             = kIOCLUTPixels;
            info->componentMasks[0]     = 0xff;
            info->bitsPerPixel          = 8;
            info->componentCount        = 1;
            info->bitsPerComponent      = 8;
            break;
        case 15:
        case 16:
            strlcpy(info->pixelFormat, IO16BitDirectPixels, sizeof(info->pixelFormat));
            info->pixelType     = kIORGBDirectPixels;
            info->componentMasks[0] = 0x7c00;
            info->componentMasks[1] = 0x03e0;
            info->componentMasks[2] = 0x001f;
            info->bitsPerPixel  = 16;
            info->componentCount        = 3;
            info->bitsPerComponent      = 5;
            break;
        case 24:
        case 32:
            strlcpy(info->pixelFormat, IO32BitDirectPixels, sizeof(info->pixelFormat));
            info->pixelType     = kIORGBDirectPixels;
            info->componentMasks[0] = 0x00ff0000;
            info->componentMasks[1] = 0x0000ff00;
            info->componentMasks[2] = 0x000000ff;
            info->bitsPerPixel  = 32;
            info->componentCount        = 3;
            info->bitsPerComponent      = 8;
            break;
    }

    return (kIOReturnSuccess);
}

bool IOGOPFramebuffer::isConsoleDevice( void )
{
    return true;
}

