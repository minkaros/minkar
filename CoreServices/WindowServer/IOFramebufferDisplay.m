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

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Onyx2D/O2Context.h>
#import <Onyx2D/O2Context_builtin.h>
#import <Onyx2D/O2Surface.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <mach/mach.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#import "IOFramebufferDisplay.h"
#import "rpc.h"     /* kWSDisplay* flags, struct CGDisplayMode */

/* Memory type for IOConnectMapMemory64(): the whole scanout aperture. */
#ifndef kIOFBVRAMMemory
#define kIOFBVRAMMemory 110
#endif

/* Method indices of the IOFramebuffer server connection. */
enum {
    kFBMethodGetPixelInformation = 1,       /* in: mode, depth, aperture */
    kFBMethodGetCurrentDisplayMode = 2,     /* out: mode, depth */
};

/*
 * Captured displays share their pixels with the capturing client through
 * a SysV shared memory segment: a header of three intptr_t (width, height,
 * bitmap info) padded to 6 ints, followed by width * 4 bytes per row.
 * CGDisplayGetDrawingContext() in CoreGraphics reads the same layout.
 */
#define kCaptureHeaderSize (6 * sizeof(int))

@implementation IOFramebufferDisplay {
    io_service_t _service;
    io_connect_t _connect;

    /* Scanout memory mapped from the kernel. */
    mach_vm_address_t _vram;
    mach_vm_size_t _vramSize;
    uint32_t _vramRowBytes;

    /* Software back buffers, width * 4 bytes per row. */
    size_t _frameSize;
    O2Context *_frame;          /* composited windows */
    O2Context *_frameCursor;    /* captured frame plus cursor */
    void *_framePixels;
    void *_frameCursorPixels;
}

+ (NSArray<IOFramebufferDisplay *> *)availableDisplays {
    NSMutableArray *displays = [NSMutableArray array];
    io_iterator_t iter = IO_OBJECT_NULL;

    kern_return_t kr = IOServiceGetMatchingServices(kIOMasterPortDefault,
        IOServiceMatching("IOFramebuffer"), &iter);
    if (kr != KERN_SUCCESS) {
        NSLog(@"IOFramebuffer lookup failed: 0x%x", kr);
        return displays;
    }

    io_service_t service;
    while ((service = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
        IOFramebufferDisplay *d = [[IOFramebufferDisplay alloc] initWithService:service];
        IOObjectRelease(service);   /* the display holds its own reference */
        if ([d open])
            [displays addObject:d];
    }
    IOObjectRelease(iter);

    /* The first framebuffer is the main display, as on macOS. */
    [[displays firstObject] markAsMain];
    return displays;
}

- (instancetype)initWithService:(io_service_t)service {
    self = [super init];
    if (self == nil)
        return nil;
    IOObjectRetain(service);
    _service = service;
    _connect = IO_OBJECT_NULL;
    _openGLMask = 0;
    return self;
}

/* A stable, non-zero CGDirectDisplayID derived from the registry entry. */
static uint32_t displayIDForService(io_service_t service) {
    uint64_t entryID = 0;
    if (IORegistryEntryGetRegistryEntryID(service, &entryID) != KERN_SUCCESS)
        return 1;
    uint32_t ID = (uint32_t)(entryID ^ (entryID >> 32));
    return ID ? ID : 1;
}

- (void)markAsMain {
    _flags |= kWSDisplayPrimary | kWSDisplayMain;
}

- (BOOL)open {
    kern_return_t kr;

    kr = IOServiceOpen(_service, mach_task_self(), kIOFBServerConnectType, &_connect);
    if (kr != KERN_SUCCESS) {
        NSLog(@"IOFramebuffer: cannot open server connection: 0x%x", kr);
        return NO;
    }

    uint64_t current[2] = { 0, 0 };
    uint32_t count = 2;
    kr = IOConnectCallScalarMethod(_connect, kFBMethodGetCurrentDisplayMode,
        NULL, 0, current, &count);
    if (kr != KERN_SUCCESS) {
        NSLog(@"IOFramebuffer: cannot get the current mode: 0x%x", kr);
        return NO;
    }

    uint64_t query[3] = { current[0], current[1], kIOFBSystemAperture };
    IOPixelInformation info;
    size_t infoSize = sizeof(info);
    memset(&info, 0, sizeof(info));
    kr = IOConnectCallMethod(_connect, kFBMethodGetPixelInformation,
        query, 3, NULL, 0, NULL, NULL, &info, &infoSize);
    if (kr != KERN_SUCCESS) {
        NSLog(@"IOFramebuffer: cannot get pixel information: 0x%x", kr);
        return NO;
    }
    if (info.bitsPerPixel != 32 || info.activeWidth == 0 || info.activeHeight == 0) {
        NSLog(@"IOFramebuffer: unsupported mode %ux%u, %u bpp",
            info.activeWidth, info.activeHeight, info.bitsPerPixel);
        return NO;
    }

    width = info.activeWidth;
    height = info.activeHeight;
    depth = 32;
    _vramRowBytes = info.bytesPerRow;

    /* Map the scanout memory, write-combined since we only write to it. */
    IOOptionBits mapOptions = kIOMapAnywhere | kIOMapWriteCombineCache;
    kr = IOConnectMapMemory64(_connect, kIOFBVRAMMemory, mach_task_self(),
        &_vram, &_vramSize, mapOptions);
    if (kr != KERN_SUCCESS)
        kr = IOConnectMapMemory64(_connect, kIOFBSystemAperture, mach_task_self(),
            &_vram, &_vramSize, mapOptions);
    if (kr != KERN_SUCCESS) {
        NSLog(@"IOFramebuffer: cannot map the screen: 0x%x", kr);
        _vram = 0;
        return NO;
    }
    if (_vramSize < (mach_vm_size_t)_vramRowBytes * height) {
        NSLog(@"IOFramebuffer: mapping too small (%llu bytes)", (unsigned long long)_vramSize);
        return NO;
    }

    _ID = displayIDForService(_service);
    _flags |= kWSDisplayActive | kWSDisplayOnline;

    _currentMode->width = width;
    _currentMode->height = height;
    _currentMode->depth = depth;
    _currentMode->refresh = 0;
    _currentMode->flags = 0;
    CFArrayAppendValue(_allModes, CGDisplayModeRetain(_currentMode));

    cs = CGColorSpaceCreateDeviceRGB();
    size_t rowBytes = (size_t)width * 4;
    _frameSize = rowBytes * height;
    _frame = [O2Context createWithBytes:NULL width:width height:height
        bitsPerComponent:8 bytesPerRow:rowBytes colorSpace:(__bridge O2ColorSpaceRef)cs
        bitmapInfo:[self format] releaseCallback:NULL releaseInfo:NULL];
    _frameCursor = [O2Context createWithBytes:NULL width:width height:height
        bitsPerComponent:8 bytesPerRow:rowBytes colorSpace:(__bridge O2ColorSpaceRef)cs
        bitmapInfo:[self format] releaseCallback:NULL releaseInfo:NULL];
    if (_frame == nil || _frameCursor == nil)
        return NO;
    _framePixels = [[_frame surface] pixelBytes];
    _frameCursorPixels = [[_frameCursor surface] pixelBytes];
    activeCtx = _frame;

    NSLog(@"IOFramebuffer: display 0x%08x %dx%d, %u bytes per row", _ID, width, height,
        _vramRowBytes);
    return YES;
}

- (void)dealloc {
    [self releaseCapture];
    if (_vram != 0)
        IOConnectUnmapMemory64(_connect, kIOFBVRAMMemory, mach_task_self(), _vram);
    if (_connect != IO_OBJECT_NULL)
        IOServiceClose(_connect);
    if (_service != IO_OBJECT_NULL)
        IOObjectRelease(_service);
    if (cs)
        CGColorSpaceRelease(cs);
}

/* Copy a width * 4 stride frame to the screen. */
- (void)present:(const void *)pixels {
    const size_t rowBytes = (size_t)width * 4;
    uint8_t *dst = (uint8_t *)(uintptr_t)_vram;
    const uint8_t *src = pixels;

    if (_vramRowBytes == rowBytes) {
        memcpy(dst, src, _frameSize);
        return;
    }
    for (int y = 0; y < height; ++y) {
        memcpy(dst, src, rowBytes);
        dst += _vramRowBytes;
        src += rowBytes;
    }
}

- (void *)visiblePixels {
    return _captured ? [[captureCtx surface] pixelBytes] : _framePixels;
}

/* Clear the drawing context to black and show it. */
- (void)clear {
    O2ContextSetRGBFillColor(activeCtx, 0, 0, 0, 1);
    O2ContextFillRect(activeCtx, O2RectMake(0, 0, width, height));
    [self present:[self visiblePixels]];
}

- (void)draw {
    [self present:[self visiblePixels]];
}

/* Used while captured: the client's frame with our cursor on top. */
- (void)drawWithCursor:(O2Image *)cursor inRect:(O2Rect)rect {
    O2ContextDrawImage(_frameCursor, O2RectMake(0, 0, width, height), [captureCtx surface]);
    O2ContextSetBlendMode(_frameCursor, kCGBlendModeNormal);
    O2ContextDrawImage(_frameCursor, rect, cursor);
    [self present:_frameCursorPixels];
}

- (O2Context *)context {
    return _captured ? captureCtx : _frame;
}

- (BOOL)capture:(pid_t)pid withOptions:(uint32_t)options {
    if (_captured != 0)
        return NO;

    pthread_mutex_lock(&renderLock);
    size_t size = kCaptureHeaderSize + _frameSize;
    int segment = shmget(_ID ^ (key_t)random(), size, IPC_CREAT | 0666);
    if (segment < 0) {
        pthread_mutex_unlock(&renderLock);
        return NO;
    }
    uint8_t *p = shmat(segment, NULL, 0);
    if (p == (void *)-1) {
        shmctl(segment, IPC_RMID, NULL);
        pthread_mutex_unlock(&renderLock);
        return NO;
    }

    intptr_t *header = (intptr_t *)p;
    header[0] = width;
    header[1] = height;
    header[2] = [self format];

    captureCtx = [[O2Context_builtin alloc] initWithBytes:p + kCaptureHeaderSize
        width:width height:height bitsPerComponent:8 bytesPerRow:(size_t)width * 4
        colorSpace:(__bridge O2ColorSpaceRef)cs bitmapInfo:[self format]
        releaseCallback:NULL releaseInfo:NULL];
    shmid = segment;
    shmSize = size;
    _captured = pid;
    activeCtx = captureCtx;
    pthread_mutex_unlock(&renderLock);

    /* Capture options are deprecated on macOS; always start from black. */
    [self clear];
    return YES;
}

- (void)releaseCapture {
    pthread_mutex_lock(&renderLock);
    if (captureCtx != nil) {
        uint8_t *pixels = [[captureCtx surface] pixelBytes];
        shmctl(shmid, IPC_RMID, NULL);
        shmdt(pixels - kCaptureHeaderSize);
        captureCtx = nil;
    }
    shmid = 0;
    shmSize = 0;
    _captured = 0;
    activeCtx = _frame;
    pthread_mutex_unlock(&renderLock);
}

/* Read a CFNumber from the attached display's EDID-derived properties. */
- (uint32_t)displayInfoNumber:(const char *)key {
    io_service_t display = IODisplayForFramebuffer(_service, 0);
    if (display == IO_OBJECT_NULL)
        return 0;
    CFDictionaryRef info = IODisplayCreateInfoDictionary(display, kIODisplayOnlyPreferredName);
    IOObjectRelease(display);
    if (info == NULL)
        return 0;

    uint32_t value = 0;
    CFStringRef name = CFStringCreateWithCString(NULL, key, kCFStringEncodingASCII);
    CFNumberRef number = CFDictionaryGetValue(info, name);
    if (number != NULL && CFGetTypeID(number) == CFNumberGetTypeID())
        CFNumberGetValue(number, kCFNumberSInt32Type, &value);
    CFRelease(name);
    CFRelease(info);
    return value;
}

- (uint32_t)vendorNumber {
    return [self displayInfoNumber:kDisplayVendorID];
}

- (uint32_t)modelNumber {
    return [self displayInfoNumber:kDisplayProductID];
}

- (uint32_t)serialNumber {
    return [self displayInfoNumber:kDisplaySerialNumber];
}

/* Mode switching needs a driver with more than the boot mode. */
- (BOOL)setMode:(struct CGDisplayMode *)mode {
    return NO;
}

@end
