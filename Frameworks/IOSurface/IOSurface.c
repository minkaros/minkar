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
 * IOSurface (roadmap step 4): a surface is a Mach memory entry mapped in
 * each process that holds it. Its global ID, its properties and its values
 * live in the registry, iosurfaced (IOSurfaceRegistry.defs); its seed and
 * use count live in a page after the pixels, shared by every mapping.
 * Without the registry a surface still works in its own process, with ID 0.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <servers/bootstrap.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "IOSurfaceRef.h"
#include "IOSurfaceRegistryUser.h"
#include <IOKit/IOKitLib.h>
#include "MinkarAMDGPUInterface.h"

#define kRegistryService	"org.minkar.iosurfaced"
#define kMaxPlanes		4
#define kTrailerSize		4096
#define kTrailerMagic		0x494f5346	/* 'IOSF' */

const CFStringRef kIOSurfaceAllocSize = CFSTR("IOSurfaceAllocSize");
const CFStringRef kIOSurfaceWidth = CFSTR("IOSurfaceWidth");
const CFStringRef kIOSurfaceHeight = CFSTR("IOSurfaceHeight");
const CFStringRef kIOSurfaceBytesPerRow = CFSTR("IOSurfaceBytesPerRow");
const CFStringRef kIOSurfaceBytesPerElement = CFSTR("IOSurfaceBytesPerElement");
const CFStringRef kIOSurfaceElementWidth = CFSTR("IOSurfaceElementWidth");
const CFStringRef kIOSurfaceElementHeight = CFSTR("IOSurfaceElementHeight");
const CFStringRef kIOSurfaceOffset = CFSTR("IOSurfaceOffset");
const CFStringRef kIOSurfacePlaneInfo = CFSTR("IOSurfacePlaneInfo");
const CFStringRef kIOSurfacePlaneWidth = CFSTR("IOSurfacePlaneWidth");
const CFStringRef kIOSurfacePlaneHeight = CFSTR("IOSurfacePlaneHeight");
const CFStringRef kIOSurfacePlaneBytesPerRow = CFSTR("IOSurfacePlaneBytesPerRow");
const CFStringRef kIOSurfacePlaneOffset = CFSTR("IOSurfacePlaneOffset");
const CFStringRef kIOSurfacePlaneSize = CFSTR("IOSurfacePlaneSize");
const CFStringRef kIOSurfacePlaneBase = CFSTR("IOSurfacePlaneBase");
const CFStringRef kIOSurfacePlaneBitsPerElement = CFSTR("IOSurfacePlaneBitsPerElement");
const CFStringRef kIOSurfacePlaneBytesPerElement = CFSTR("IOSurfacePlaneBytesPerElement");
const CFStringRef kIOSurfacePlaneElementWidth = CFSTR("IOSurfacePlaneElementWidth");
const CFStringRef kIOSurfacePlaneElementHeight = CFSTR("IOSurfacePlaneElementHeight");
const CFStringRef kIOSurfaceCacheMode = CFSTR("IOSurfaceCacheMode");
const CFStringRef kIOSurfaceIsGlobal = CFSTR("IOSurfaceIsGlobal");
const CFStringRef kIOSurfacePixelFormat = CFSTR("IOSurfacePixelFormat");
const CFStringRef kIOSurfacePixelSizeCastingAllowed = CFSTR("IOSurfacePixelSizeCastingAllowed");
const CFStringRef kIOSurfacePlaneComponentBitDepths = CFSTR("IOSurfacePlaneComponentBitDepths");
const CFStringRef kIOSurfacePlaneComponentBitOffsets = CFSTR("IOSurfacePlaneComponentBitOffsets");
const CFStringRef kIOSurfaceName = CFSTR("IOSurfaceName");

/* Shared by every process's mapping, after the pixels. */
typedef struct {
	_Atomic uint32_t	seed;
	_Atomic int32_t		useCount;
	uint32_t		magic;
	uint32_t		purgeable;
} SurfaceTrailer;

typedef struct {
	size_t	width, height, bytesPerRow, bytesPerElement;
	size_t	elementWidth, elementHeight, offset, size;
} SurfacePlane;

struct __IOSurface {
	CFRuntimeBase		base;
	IOSurfaceID		surfaceID;
	mach_port_t		memory;		/* the memory entry, a send right */
	mach_vm_address_t	address;
	mach_vm_size_t		mapSize;	/* pixels, rounded, and the trailer */
	size_t			allocSize;
	size_t			width, height, bytesPerRow, bytesPerElement;
	size_t			elementWidth, elementHeight;
	OSType			pixelFormat;
	Boolean			castingAllowed;
	size_t			planeCount;
	SurfacePlane		planes[kMaxPlanes];
	CFMutableDictionaryRef	localValues;	/* without the registry */
	_Atomic int		locks;
	/* In GPU memory, in the process that made it: the GPU's buffer,
	 * mapped from the GPU's user client (the others map the entry). */
	uint64_t		gpuHandle;
	uint64_t		gpuBuffer;	/* its name */
	uint64_t		gpuEntryID;	/* its GPU's registry entry */
	/* Elsewhere: whether the registry was asked for the two above. */
	_Atomic Boolean		gpuAsked;
};

static CFTypeID surfaceTypeID;
static mach_port_t registry = MACH_PORT_NULL;
static mach_port_t session = MACH_PORT_NULL;	/* this process, to the registry */
static pthread_once_t once = PTHREAD_ONCE_INIT;

static void surfaceFinalize(CFTypeRef cf);

static const CFRuntimeClass surfaceClass = {
	0, "IOSurface", NULL, NULL, surfaceFinalize, NULL, NULL, NULL, NULL
};

static void setUp(void) {
	surfaceTypeID = _CFRuntimeRegisterClass(&surfaceClass);
	if (bootstrap_look_up(bootstrap_port, kRegistryService, &registry) != KERN_SUCCESS)
		registry = MACH_PORT_NULL;
	if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &session) != KERN_SUCCESS)
		session = MACH_PORT_NULL;
}

CFTypeID IOSurfaceGetTypeID(void) {
	pthread_once(&once, setUp);
	return surfaceTypeID;
}

static SurfaceTrailer *trailer(IOSurfaceRef s) {
	return (SurfaceTrailer *)(uintptr_t)(s->address + s->mapSize - kTrailerSize);
}

/* ---- Properties --------------------------------------------------------- */

static size_t number(CFDictionaryRef d, CFStringRef key, size_t fallback) {
	CFNumberRef n = d ? CFDictionaryGetValue(d, key) : NULL;
	long long v;

	if (n == NULL || CFGetTypeID(n) != CFNumberGetTypeID() ||
	    !CFNumberGetValue(n, kCFNumberLongLongType, &v) || v < 0)
		return fallback;
	return (size_t)v;
}

static size_t alignUp(size_t value, size_t to) {
	return (value + to - 1) / to * to;
}

/* Fills the geometry from properties (as created, or as the registry
 * keeps them); false when they make no surface. */
static Boolean readGeometry(IOSurfaceRef s, CFDictionaryRef p) {
	s->width = number(p, kIOSurfaceWidth, 0);
	s->height = number(p, kIOSurfaceHeight, 0);
	s->bytesPerElement = number(p, kIOSurfaceBytesPerElement, 1);
	s->elementWidth = number(p, kIOSurfaceElementWidth, 1);
	s->elementHeight = number(p, kIOSurfaceElementHeight, 1);
	if (s->elementWidth == 0 || s->elementHeight == 0)
		return false;
	s->bytesPerRow = number(p, kIOSurfaceBytesPerRow,
	    IOSurfaceAlignProperty(kIOSurfaceBytesPerRow,
	    (s->width + s->elementWidth - 1) / s->elementWidth * s->bytesPerElement));
	s->pixelFormat = (OSType)number(p, kIOSurfacePixelFormat, 0);
	CFBooleanRef cast = p ? CFDictionaryGetValue(p, kIOSurfacePixelSizeCastingAllowed) : NULL;
	s->castingAllowed = cast == NULL || cast == kCFBooleanTrue;

	size_t end = number(p, kIOSurfaceOffset, 0) +
	    s->bytesPerRow * ((s->height + s->elementHeight - 1) / s->elementHeight);
	CFArrayRef info = p ? CFDictionaryGetValue(p, kIOSurfacePlaneInfo) : NULL;
	s->planeCount = 0;
	if (info != NULL && CFGetTypeID(info) == CFArrayGetTypeID()) {
		size_t offset = 0;
		CFIndex n = CFArrayGetCount(info);
		if (n > kMaxPlanes)
			return false;
		for (CFIndex i = 0; i < n; i++) {
			CFDictionaryRef d = CFArrayGetValueAtIndex(info, i);
			SurfacePlane *pl = &s->planes[i];
			if (CFGetTypeID(d) != CFDictionaryGetTypeID())
				return false;
			pl->width = number(d, kIOSurfacePlaneWidth, 0);
			pl->height = number(d, kIOSurfacePlaneHeight, 0);
			pl->bytesPerElement = number(d, kIOSurfacePlaneBytesPerElement, 1);
			pl->elementWidth = number(d, kIOSurfacePlaneElementWidth, 1);
			pl->elementHeight = number(d, kIOSurfacePlaneElementHeight, 1);
			if (pl->elementWidth == 0 || pl->elementHeight == 0)
				return false;
			pl->bytesPerRow = number(d, kIOSurfacePlaneBytesPerRow,
			    IOSurfaceAlignProperty(kIOSurfaceBytesPerRow,
			    (pl->width + pl->elementWidth - 1) / pl->elementWidth * pl->bytesPerElement));
			pl->offset = number(d, kIOSurfacePlaneOffset, offset);
			pl->size = number(d, kIOSurfacePlaneSize,
			    pl->bytesPerRow * ((pl->height + pl->elementHeight - 1) / pl->elementHeight));
			offset = alignUp(pl->offset + pl->size, 16);
			if (pl->offset + pl->size > end)
				end = pl->offset + pl->size;
		}
		s->planeCount = (size_t)n;
	}
	s->allocSize = number(p, kIOSurfaceAllocSize, end);
	if (s->allocSize < end || s->allocSize == 0)
		return false;
	s->mapSize = alignUp(s->allocSize, (size_t)vm_page_size) + kTrailerSize;
	return true;
}

static CFDataRef serialize(CFPropertyListRef p) {
	return p ? CFPropertyListCreateData(NULL, p, kCFPropertyListBinaryFormat_v1_0, 0, NULL) : NULL;
}

static CFPropertyListRef deserialize(const void *bytes, size_t len) {
	if (bytes == NULL || len == 0)
		return NULL;
	CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, bytes, (CFIndex)len, kCFAllocatorNull);
	CFPropertyListRef p = CFPropertyListCreateWithData(NULL, data,
	    kCFPropertyListMutableContainers, NULL, NULL);
	CFRelease(data);
	return p;
}

/* ---- Surfaces in GPU memory (G2) -------------------------------------- */

/*
 * Asked for with IOSurfaceMinkarGPUMemory (a private property, true): the
 * pixels in VRAM of the GPU that lights the display, a shared buffer of
 * its user client (for administrators and the gpu-client entitlement, so
 * for the window server and GPU daemons). The registry holds the buffer
 * while the surface lives anywhere; others map it through the memory
 * entry as any surface. The CPU reads VRAM slowly: for what the GPU draws.
 */
static const CFStringRef kIOSurfaceMinkarGPUMemory = CFSTR("IOSurfaceMinkarGPUMemory");
static pthread_once_t gpuOnce = PTHREAD_ONCE_INIT;
static io_connect_t gpu = IO_OBJECT_NULL;
static uint64_t gpuEntryID;

static void openGPU(void) {
	io_iterator_t it = IO_OBJECT_NULL;
	io_service_t service, chosen = IO_OBJECT_NULL;

	if (IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("MinkarAMDGPU"),
	    &it) != KERN_SUCCESS)
		return;
	while ((service = IOIteratorNext(it)) != IO_OBJECT_NULL) {
		CFTypeRef lights = IORegistryEntryCreateCFProperty(service, CFSTR("MinkarLightsDisplay"),
		    kCFAllocatorDefault, 0);
		Boolean display = lights == kCFBooleanTrue;
		if (lights != NULL)
			CFRelease(lights);
		if (chosen == IO_OBJECT_NULL || display) {
			if (chosen != IO_OBJECT_NULL)
				IOObjectRelease(chosen);
			chosen = service;
			if (display)
				break;
		} else
			IOObjectRelease(service);
	}
	IOObjectRelease(it);
	if (chosen == IO_OBJECT_NULL)
		return;
	if (IOServiceOpen(chosen, mach_task_self(), 0, &gpu) != KERN_SUCCESS)
		gpu = IO_OBJECT_NULL;
	else
		IORegistryEntryGetRegistryEntryID(chosen, &gpuEntryID);
	IOObjectRelease(chosen);
}

static Boolean wantsGPUMemory(CFDictionaryRef properties) {
	CFTypeRef v = properties ? CFDictionaryGetValue(properties, kIOSurfaceMinkarGPUMemory) : NULL;
	return v == kCFBooleanTrue;
}

/* The pixels and the trailer in a shared VRAM buffer, mapped here. */
static Boolean allocateInGPU(IOSurfaceRef s) {
	uint64_t in = s->mapSize, out[3];
	uint32_t n = 3;
	mach_vm_size_t size = 0;

	pthread_once(&gpuOnce, openGPU);
	if (gpu == IO_OBJECT_NULL ||
	    IOConnectCallScalarMethod(gpu, kMinkarAMDGPUCreateSharedBuffer, &in, 1, out, &n) != KERN_SUCCESS)
		return false;
	s->gpuHandle = out[0];
	s->gpuBuffer = out[2];
	s->gpuEntryID = gpuEntryID;
	s->gpuAsked = true;
	if (IOConnectMapMemory64(gpu, (uint32_t)s->gpuHandle, mach_task_self(), &s->address, &size,
	    kIOMapAnywhere) != KERN_SUCCESS || size < s->mapSize) {
		s->address = 0;
		return false;
	}
	return true;
}

/* A surface looked up: the registry knows its GPU buffer, once its maker
 * attached it. */
static void askGPUBuffer(IOSurfaceRef s) {
	uint64_t entryID = 0, name = 0;
	int32_t result = -1;

	if (s->gpuAsked || s->surfaceID == 0 || registry == MACH_PORT_NULL)
		return;
	if (iosr_gpu_buffer(registry, s->surfaceID, &entryID, &name, &result) != KERN_SUCCESS ||
	    result != 0)
		return;
	s->gpuEntryID = entryID;
	s->gpuBuffer = name;
	s->gpuAsked = true;
}

/* The GPU's buffer behind a surface: its name, 0 for system memory. In
 * any process that holds the surface. */
uint64_t IOSurfaceGetMinkarGPUBuffer(IOSurfaceRef buffer) {
	if (buffer == NULL)
		return 0;
	askGPUBuffer(buffer);
	return buffer->gpuBuffer;
}

/* The registry entry ID of the GPU that holds it, 0 for system memory. */
uint64_t IOSurfaceGetMinkarGPU(IOSurfaceRef buffer) {
	if (buffer == NULL)
		return 0;
	askGPUBuffer(buffer);
	return buffer->gpuEntryID;
}

/* ---- Making and finding surfaces ------------------------------------- */

static IOSurfaceRef newSurface(void) {
	pthread_once(&once, setUp);
	IOSurfaceRef s = (IOSurfaceRef)_CFRuntimeCreateInstance(NULL, surfaceTypeID,
	    sizeof(struct __IOSurface) - sizeof(CFRuntimeBase), NULL);
	return s;
}

IOSurfaceRef IOSurfaceCreate(CFDictionaryRef properties) {
	IOSurfaceRef s = newSurface();
	memory_object_size_t entrySize;

	if (s == NULL)
		return NULL;
	if (!readGeometry(s, properties) || s->width == 0 || s->height == 0) {
		CFRelease(s);
		return NULL;
	}
	if (wantsGPUMemory(properties)) {
		if (!allocateInGPU(s)) {
			CFRelease(s);
			return NULL;
		}
	} else if (mach_vm_allocate(mach_task_self(), &s->address, s->mapSize, VM_FLAGS_ANYWHERE) !=
	    KERN_SUCCESS) {
		s->address = 0;
		CFRelease(s);
		return NULL;
	}
	entrySize = s->mapSize;
	if (mach_make_memory_entry_64(mach_task_self(), &entrySize, s->address,
	    VM_PROT_READ | VM_PROT_WRITE, &s->memory, MACH_PORT_NULL) != KERN_SUCCESS) {
		s->memory = MACH_PORT_NULL;
		CFRelease(s);
		return NULL;
	}
	SurfaceTrailer *t = trailer(s);
	t->magic = kTrailerMagic;
	atomic_store(&t->seed, 0);
	atomic_store(&t->useCount, 0);

	if (registry != MACH_PORT_NULL) {
		CFDataRef p = serialize(properties);
		int32_t result = -1;
		if (iosr_create(registry, session, s->memory, s->allocSize,
		    p ? (uint8_t *)CFDataGetBytePtr(p) : NULL, p ? (mach_msg_type_number_t)CFDataGetLength(p) : 0,
		    &s->surfaceID, &result) != KERN_SUCCESS || result != 0)
			s->surfaceID = 0;
		if (p)
			CFRelease(p);
		/* The registry holds the GPU memory while the surface lives. */
		if (s->surfaceID != 0 && s->gpuBuffer != 0 &&
		    (iosr_attach_gpu_buffer(registry, s->surfaceID, gpuEntryID, s->gpuBuffer, &result) !=
		    KERN_SUCCESS || result != 0)) {
			CFRelease(s);
			return NULL;
		}
	}
	return s;
}

static IOSurfaceRef mapSurface(IOSurfaceID surfaceID, mach_port_t memory, uint64_t allocSize,
    CFDictionaryRef properties) {
	IOSurfaceRef s = newSurface();

	if (s == NULL)
		return NULL;
	s->memory = memory;
	s->surfaceID = surfaceID;
	if (!readGeometry(s, properties) || s->allocSize != allocSize ||
	    mach_vm_map(mach_task_self(), &s->address, s->mapSize, 0, VM_FLAGS_ANYWHERE, memory, 0,
	    FALSE, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE) !=
	    KERN_SUCCESS || trailer(s)->magic != kTrailerMagic) {
		CFRelease(s);
		return NULL;
	}
	return s;
}

IOSurfaceRef IOSurfaceLookup(IOSurfaceID csid) {
	mach_port_t memory = MACH_PORT_NULL;
	uint64_t allocSize = 0;
	vm_offset_t props = 0;
	mach_msg_type_number_t propsLen = 0;
	int32_t result = -1;

	pthread_once(&once, setUp);
	if (registry == MACH_PORT_NULL || csid == 0 ||
	    iosr_lookup(registry, session, csid, &memory, &allocSize, (uint8_t **)&props, &propsLen,
	    &result) != KERN_SUCCESS || result != 0)
		return NULL;
	CFDictionaryRef p = deserialize((const void *)props, propsLen);
	mach_vm_deallocate(mach_task_self(), props, propsLen);
	IOSurfaceRef s = mapSurface(csid, memory, allocSize, p);
	if (p)
		CFRelease(p);
	if (s == NULL)
		iosr_release(registry, csid);
	return s;
}

IOSurfaceID IOSurfaceGetID(IOSurfaceRef buffer) {
	return buffer ? buffer->surfaceID : 0;
}

static void surfaceFinalize(CFTypeRef cf) {
	IOSurfaceRef s = (IOSurfaceRef)cf;

	if (s->gpuHandle != 0) {
		/* This process's hold on the GPU's buffer; the registry may keep it. */
		uint64_t h = s->gpuHandle;
		if (s->address != 0)
			IOConnectUnmapMemory64(gpu, (uint32_t)h, mach_task_self(), s->address);
		IOConnectCallScalarMethod(gpu, kMinkarAMDGPUDestroyBuffer, &h, 1, NULL, NULL);
	} else if (s->address != 0)
		mach_vm_deallocate(mach_task_self(), s->address, s->mapSize);
	if (s->memory != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), s->memory);
	if (s->surfaceID != 0 && registry != MACH_PORT_NULL)
		iosr_release(registry, s->surfaceID);
	if (s->localValues)
		CFRelease(s->localValues);
}

mach_port_t IOSurfaceCreateMachPort(IOSurfaceRef buffer) {
	mach_port_t port = MACH_PORT_NULL;
	int32_t result = -1;

	if (buffer == NULL || buffer->surfaceID == 0 || registry == MACH_PORT_NULL ||
	    iosr_make_port(registry, buffer->surfaceID, &port, &result) != KERN_SUCCESS || result != 0)
		return MACH_PORT_NULL;
	return port;
}

IOSurfaceRef IOSurfaceLookupFromMachPort(mach_port_t port) {
	uint32_t surfaceID = 0;
	int32_t result = -1;

	pthread_once(&once, setUp);
	if (port == MACH_PORT_NULL || registry == MACH_PORT_NULL ||
	    iosr_port_id(registry, port, &surfaceID, &result) != KERN_SUCCESS || result != 0)
		return NULL;
	return IOSurfaceLookup(surfaceID);
}

/* ---- Locking and the seed --------------------------------------------- */

kern_return_t IOSurfaceLock(IOSurfaceRef buffer, IOSurfaceLockOptions options, uint32_t *seed) {
	if (buffer == NULL)
		return KERN_INVALID_ARGUMENT;
	atomic_fetch_add(&buffer->locks, 1);
	if (seed)
		*seed = atomic_load(&trailer(buffer)->seed);
	return KERN_SUCCESS;
}

/* A lock that was not read-only changes the seed when it ends. */
kern_return_t IOSurfaceUnlock(IOSurfaceRef buffer, IOSurfaceLockOptions options, uint32_t *seed) {
	uint32_t value;

	if (buffer == NULL || atomic_load(&buffer->locks) == 0)
		return KERN_INVALID_ARGUMENT;
	atomic_fetch_sub(&buffer->locks, 1);
	if (options & kIOSurfaceLockReadOnly)
		value = atomic_load(&trailer(buffer)->seed);
	else
		value = atomic_fetch_add(&trailer(buffer)->seed, 1) + 1;
	if (seed)
		*seed = value;
	return KERN_SUCCESS;
}

uint32_t IOSurfaceGetSeed(IOSurfaceRef buffer) {
	return buffer ? atomic_load(&trailer(buffer)->seed) : 0;
}

/* ---- Geometry --------------------------------------------------------- */

size_t IOSurfaceGetAllocSize(IOSurfaceRef b) { return b ? b->allocSize : 0; }
size_t IOSurfaceGetWidth(IOSurfaceRef b) { return b ? b->width : 0; }
size_t IOSurfaceGetHeight(IOSurfaceRef b) { return b ? b->height : 0; }
size_t IOSurfaceGetBytesPerElement(IOSurfaceRef b) { return b ? b->bytesPerElement : 0; }
size_t IOSurfaceGetBytesPerRow(IOSurfaceRef b) { return b ? b->bytesPerRow : 0; }
void *IOSurfaceGetBaseAddress(IOSurfaceRef b) { return b ? (void *)(uintptr_t)b->address : NULL; }
size_t IOSurfaceGetElementWidth(IOSurfaceRef b) { return b ? b->elementWidth : 0; }
size_t IOSurfaceGetElementHeight(IOSurfaceRef b) { return b ? b->elementHeight : 0; }
OSType IOSurfaceGetPixelFormat(IOSurfaceRef b) { return b ? b->pixelFormat : 0; }
Boolean IOSurfaceAllowsPixelSizeCasting(IOSurfaceRef b) { return b ? b->castingAllowed : false; }

IOSurfaceSubsampling IOSurfaceGetSubsampling(IOSurfaceRef b) {
	if (b == NULL)
		return kIOSurfaceSubsamplingUnknown;
	if (b->planeCount < 2)
		return kIOSurfaceSubsamplingNone;
	const SurfacePlane *y = &b->planes[0], *c = &b->planes[1];
	if (c->width * 2 == y->width && c->height * 2 == y->height)
		return kIOSurfaceSubsampling420;
	if (c->width * 2 == y->width && c->height == y->height)
		return kIOSurfaceSubsampling422;
	if (c->width * 4 == y->width && c->height == y->height)
		return kIOSurfaceSubsampling411;
	return kIOSurfaceSubsamplingNone;
}

size_t IOSurfaceGetPlaneCount(IOSurfaceRef b) { return b ? b->planeCount : 0; }

static const SurfacePlane *plane(IOSurfaceRef b, size_t i) {
	return (b != NULL && i < b->planeCount) ? &b->planes[i] : NULL;
}

/* A surface with no planes answers for plane 0 as itself. */
size_t IOSurfaceGetWidthOfPlane(IOSurfaceRef b, size_t i) {
	const SurfacePlane *p = plane(b, i);
	return p ? p->width : (b && i == 0 && b->planeCount == 0) ? b->width : 0;
}
size_t IOSurfaceGetHeightOfPlane(IOSurfaceRef b, size_t i) {
	const SurfacePlane *p = plane(b, i);
	return p ? p->height : (b && i == 0 && b->planeCount == 0) ? b->height : 0;
}
size_t IOSurfaceGetBytesPerElementOfPlane(IOSurfaceRef b, size_t i) {
	const SurfacePlane *p = plane(b, i);
	return p ? p->bytesPerElement : (b && i == 0 && b->planeCount == 0) ? b->bytesPerElement : 0;
}
size_t IOSurfaceGetBytesPerRowOfPlane(IOSurfaceRef b, size_t i) {
	const SurfacePlane *p = plane(b, i);
	return p ? p->bytesPerRow : (b && i == 0 && b->planeCount == 0) ? b->bytesPerRow : 0;
}
size_t IOSurfaceGetElementWidthOfPlane(IOSurfaceRef b, size_t i) {
	const SurfacePlane *p = plane(b, i);
	return p ? p->elementWidth : (b && i == 0 && b->planeCount == 0) ? b->elementWidth : 0;
}
size_t IOSurfaceGetElementHeightOfPlane(IOSurfaceRef b, size_t i) {
	const SurfacePlane *p = plane(b, i);
	return p ? p->elementHeight : (b && i == 0 && b->planeCount == 0) ? b->elementHeight : 0;
}
void *IOSurfaceGetBaseAddressOfPlane(IOSurfaceRef b, size_t i) {
	const SurfacePlane *p = plane(b, i);
	if (p)
		return (void *)(uintptr_t)(b->address + p->offset);
	return (b && i == 0 && b->planeCount == 0) ? IOSurfaceGetBaseAddress(b) : NULL;
}

/* ---- Values ----------------------------------------------------------- */

/* The values, from the registry (or this process's, without it). */
static CFMutableDictionaryRef copyValues(IOSurfaceRef b) {
	vm_offset_t bytes = 0;
	mach_msg_type_number_t len = 0;
	int32_t result = -1;
	CFMutableDictionaryRef d = NULL;

	if (b->surfaceID != 0 && registry != MACH_PORT_NULL &&
	    iosr_copy_values(registry, b->surfaceID, (uint8_t **)&bytes, &len, &result) ==
	    KERN_SUCCESS && result == 0) {
		CFPropertyListRef p = deserialize((const void *)bytes, len);
		if (len)
			mach_vm_deallocate(mach_task_self(), bytes, len);
		if (p != NULL && CFGetTypeID(p) == CFDictionaryGetTypeID())
			d = (CFMutableDictionaryRef)p;
		else if (p != NULL)
			CFRelease(p);
		if (d == NULL)
			d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
			    &kCFTypeDictionaryValueCallBacks);
		return d;
	}
	if (b->localValues == NULL)
		b->localValues = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
	return CFDictionaryCreateMutableCopy(NULL, 0, b->localValues);
}

static void storeValues(IOSurfaceRef b, CFDictionaryRef d) {
	if (b->surfaceID != 0 && registry != MACH_PORT_NULL) {
		CFDataRef data = serialize(d);
		int32_t result;
		if (data != NULL) {
			iosr_set_values(registry, b->surfaceID, (uint8_t *)CFDataGetBytePtr(data),
			    (mach_msg_type_number_t)CFDataGetLength(data), &result);
			CFRelease(data);
		}
		return;
	}
	if (b->localValues)
		CFRelease(b->localValues);
	b->localValues = CFDictionaryCreateMutableCopy(NULL, 0, d);
}

void IOSurfaceSetValue(IOSurfaceRef b, CFStringRef key, CFTypeRef value) {
	if (b == NULL || key == NULL || value == NULL)
		return;
	CFMutableDictionaryRef d = copyValues(b);
	CFDictionarySetValue(d, key, value);
	storeValues(b, d);
	CFRelease(d);
}

CFTypeRef IOSurfaceCopyValue(IOSurfaceRef b, CFStringRef key) {
	if (b == NULL || key == NULL)
		return NULL;
	CFMutableDictionaryRef d = copyValues(b);
	CFTypeRef v = CFDictionaryGetValue(d, key);
	if (v)
		CFRetain(v);
	CFRelease(d);
	return v;
}

void IOSurfaceRemoveValue(IOSurfaceRef b, CFStringRef key) {
	if (b == NULL || key == NULL)
		return;
	CFMutableDictionaryRef d = copyValues(b);
	CFDictionaryRemoveValue(d, key);
	storeValues(b, d);
	CFRelease(d);
}

static void addEntry(const void *key, const void *value, void *context) {
	CFDictionarySetValue((CFMutableDictionaryRef)context, key, value);
}

void IOSurfaceSetValues(IOSurfaceRef b, CFDictionaryRef keysAndValues) {
	if (b == NULL || keysAndValues == NULL)
		return;
	CFMutableDictionaryRef d = copyValues(b);
	CFDictionaryApplyFunction(keysAndValues, addEntry, d);
	storeValues(b, d);
	CFRelease(d);
}

CFDictionaryRef IOSurfaceCopyAllValues(IOSurfaceRef b) {
	return b ? copyValues(b) : NULL;
}

void IOSurfaceRemoveAllValues(IOSurfaceRef b) {
	if (b == NULL)
		return;
	CFDictionaryRef empty = CFDictionaryCreate(NULL, NULL, NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	storeValues(b, empty);
	CFRelease(empty);
}

/* ---- Limits and alignment -------------------------------------------- */

size_t IOSurfaceGetPropertyMaximum(CFStringRef property) {
	if (property != NULL && (CFEqual(property, kIOSurfaceWidth) ||
	    CFEqual(property, kIOSurfaceHeight)))
		return 16384;
	return SIZE_MAX;
}

size_t IOSurfaceGetPropertyAlignment(CFStringRef property) {
	if (property != NULL && (CFEqual(property, kIOSurfaceBytesPerRow) ||
	    CFEqual(property, kIOSurfacePlaneBytesPerRow) || CFEqual(property, kIOSurfaceOffset) ||
	    CFEqual(property, kIOSurfacePlaneOffset)))
		return 16;
	if (property != NULL && CFEqual(property, kIOSurfaceAllocSize))
		return (size_t)vm_page_size;
	return 1;
}

size_t IOSurfaceAlignProperty(CFStringRef property, size_t value) {
	return alignUp(value, IOSurfaceGetPropertyAlignment(property));
}

/* ---- Use count and purgeability -------------------------------------- */

void IOSurfaceIncrementUseCount(IOSurfaceRef b) {
	if (b)
		atomic_fetch_add(&trailer(b)->useCount, 1);
}

void IOSurfaceDecrementUseCount(IOSurfaceRef b) {
	if (b)
		atomic_fetch_sub(&trailer(b)->useCount, 1);
}

int32_t IOSurfaceGetUseCount(IOSurfaceRef b) {
	return b ? atomic_load(&trailer(b)->useCount) : 0;
}

Boolean IOSurfaceIsInUse(IOSurfaceRef b) {
	return IOSurfaceGetUseCount(b) > 0;
}

/* The memory is never discarded: a volatile surface keeps its pixels. */
kern_return_t IOSurfaceSetPurgeable(IOSurfaceRef b, uint32_t newState, uint32_t *oldState) {
	if (b == NULL || newState > kIOSurfacePurgeableKeepCurrent)
		return KERN_INVALID_ARGUMENT;
	SurfaceTrailer *t = trailer(b);
	if (oldState)
		*oldState = t->purgeable;
	if (newState != kIOSurfacePurgeableKeepCurrent)
		t->purgeable = newState == kIOSurfacePurgeableEmpty ? kIOSurfacePurgeableVolatile : newState;
	return KERN_SUCCESS;
}
