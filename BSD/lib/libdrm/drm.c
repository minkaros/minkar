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

/* xf86drm.h's functions that Mesa's RADV and its Vulkan runtime use. */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <CoreFoundation/CoreFoundation.h>
#include <xf86drm.h>
#include "minkar_drm_private.h"

/* ---- The GPUs ---- */

static uint32_t
dataValue(io_service_t s, CFStringRef key)
{
	uint32_t v = 0;
	CFTypeRef p = IORegistryEntryCreateCFProperty(s, key, kCFAllocatorDefault, 0);
	if (p != NULL && CFGetTypeID(p) == CFDataGetTypeID() && CFDataGetLength(p) >= 4)
		memcpy(&v, CFDataGetBytePtr(p), 4);
	if (p != NULL)
		CFRelease(p);
	return v;
}

unsigned
minkar_drm_gpus(struct minkar_drm_gpu *gpus, unsigned max)
{
	io_iterator_t it;
	io_service_t s;
	unsigned n = 0;

	if (IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("MinkarAMDGPU"),
	    &it) != KERN_SUCCESS)
		return 0;
	while ((s = IOIteratorNext(it)) != IO_OBJECT_NULL) {
		io_registry_entry_t pci = IO_OBJECT_NULL;
		unsigned minor = 0;
		CFTypeRef m = IORegistryEntryCreateCFProperty(s, CFSTR("MinkarDRMMinor"),
		    kCFAllocatorDefault, 0);
		if (m != NULL && CFGetTypeID(m) == CFNumberGetTypeID())
			CFNumberGetValue(m, kCFNumberIntType, &minor);
		if (m != NULL)
			CFRelease(m);
		if (n >= max || minor == 0 ||
		    IORegistryEntryGetParentEntry(s, kIOServicePlane, &pci) != KERN_SUCCESS) {
			IOObjectRelease(s);
			continue;
		}
		struct minkar_drm_gpu *g = &gpus[n++];
		memset(g, 0, sizeof(*g));
		g->service = s;
		g->minor = minor;
		g->vendor = (uint16_t)dataValue(pci, CFSTR("vendor-id"));
		g->device = (uint16_t)dataValue(pci, CFSTR("device-id"));
		g->subvendor = (uint16_t)dataValue(pci, CFSTR("subsystem-vendor-id"));
		g->subdevice = (uint16_t)dataValue(pci, CFSTR("subsystem-id"));
		g->revision = (uint8_t)dataValue(pci, CFSTR("revision-id"));
		/* "reg"'s first cell: bus << 16 | device << 11 | function << 8. */
		uint32_t reg = dataValue(pci, CFSTR("reg"));
		g->bus = (uint8_t)(reg >> 16);
		g->dev = (uint8_t)((reg >> 11) & 0x1f);
		g->func = (uint8_t)((reg >> 8) & 0x7);
		IOObjectRelease(pci);
	}
	IOObjectRelease(it);
	/* The GPU that lights the display first: Vulkan's first device, the
	 * one most apps draw with, is then the one whose VRAM holds windows'
	 * surfaces (IOSurfaces in VRAM go there). */
	for (unsigned i = 1; i < n; i++) {
		CFTypeRef lights = IORegistryEntryCreateCFProperty(gpus[i].service,
		    CFSTR("MinkarLightsDisplay"), kCFAllocatorDefault, 0);
		bool display = lights == kCFBooleanTrue;
		if (lights != NULL)
			CFRelease(lights);
		if (display) {
			struct minkar_drm_gpu g = gpus[i];
			memmove(&gpus[1], &gpus[0], i * sizeof(gpus[0]));
			gpus[0] = g;
			break;
		}
	}
	return n;
}

void
minkar_drm_release_gpus(struct minkar_drm_gpu *gpus, unsigned n)
{
	for (unsigned i = 0; i < n; i++)
		IOObjectRelease(gpus[i].service);
}

int
minkar_drm_gpu_for_minor(const struct minkar_drm_gpu *gpus, unsigned n, int minor)
{
	for (unsigned i = 0; i < n; i++)
		if ((int)gpus[i].minor == minor)
			return (int)i;
	return -1;
}

/* One block for a device and what it points to, as libdrm allocates it. */
static drmDevicePtr
deviceFor(const struct minkar_drm_gpu *g)
{
	struct block {
		drmDevice d;
		char *nodes[DRM_NODE_MAX];
		char render[32];
		drmPciBusInfo bus;
		drmPciDeviceInfo info;
	} *b = calloc(1, sizeof(*b));

	if (b == NULL)
		return NULL;
	snprintf(b->render, sizeof(b->render), "/dev/dri/renderD%u", g->minor);
	b->nodes[DRM_NODE_RENDER] = b->render;
	b->d.nodes = b->nodes;
	b->d.available_nodes = 1 << DRM_NODE_RENDER;
	b->d.bustype = DRM_BUS_PCI;
	b->bus.domain = 0;
	b->bus.bus = g->bus;
	b->bus.dev = g->dev;
	b->bus.func = g->func;
	b->info.vendor_id = g->vendor;
	b->info.device_id = g->device;
	b->info.subvendor_id = g->subvendor;
	b->info.subdevice_id = g->subdevice;
	b->info.revision_id = g->revision;
	b->d.businfo.pci = &b->bus;
	b->d.deviceinfo.pci = &b->info;
	return &b->d;
}

int
drmGetDevices2(uint32_t flags, drmDevicePtr devices[], int max_devices)
{
	struct minkar_drm_gpu gpus[MINKAR_DRM_MAX_GPUS];
	unsigned n = minkar_drm_gpus(gpus, MINKAR_DRM_MAX_GPUS);

	(void)flags;
	if (devices == NULL) {
		minkar_drm_release_gpus(gpus, n);
		return (int)n;
	}
	int count = 0;
	for (unsigned i = 0; i < n && count < max_devices; i++) {
		drmDevicePtr d = deviceFor(&gpus[i]);
		if (d != NULL)
			devices[count++] = d;
	}
	minkar_drm_release_gpus(gpus, n);
	return count;
}

int
drmGetDevices(drmDevicePtr devices[], int max_devices)
{
	return drmGetDevices2(0, devices, max_devices);
}

void
drmFreeDevice(drmDevicePtr *device)
{
	if (device != NULL) {
		free(*device);	/* the block's start */
		*device = NULL;
	}
}

void
drmFreeDevices(drmDevicePtr devices[], int count)
{
	for (int i = 0; devices != NULL && i < count; i++)
		drmFreeDevice(&devices[i]);
}

static int
deviceWithMinor(int minor, drmDevicePtr *device)
{
	struct minkar_drm_gpu gpus[MINKAR_DRM_MAX_GPUS];
	unsigned n = minkar_drm_gpus(gpus, MINKAR_DRM_MAX_GPUS);
	int i = minkar_drm_gpu_for_minor(gpus, n, minor), err = -ENODEV;

	if (i >= 0) {
		*device = deviceFor(&gpus[i]);
		err = *device != NULL ? 0 : -ENOMEM;
	}
	minkar_drm_release_gpus(gpus, n);
	return err;
}

int
drmGetDevice2(int fd, uint32_t flags, drmDevicePtr *device)
{
	(void)flags;
	return deviceWithMinor(minkar_drm_fd_minor(fd), device);
}

int
drmGetDevice(int fd, drmDevicePtr *device)
{
	return drmGetDevice2(fd, 0, device);
}

int
drmGetDeviceFromDevId(dev_t dev_id, uint32_t flags, drmDevicePtr *device)
{
	(void)flags;
	return deviceWithMinor((int)minor(dev_id), device);
}

/* The same device: the same bus and place on it. */
int
drmDevicesEqual(drmDevicePtr a, drmDevicePtr b)
{
	if (a == NULL || b == NULL || a->bustype != b->bustype || a->bustype != DRM_BUS_PCI)
		return 0;
	return memcmp(a->businfo.pci, b->businfo.pci, sizeof(drmPciBusInfo)) == 0;
}

/* ---- Descriptors ---- */

int
minkar_drm_fd_minor(int fd)
{
	struct stat st;

	if (fstat(fd, &st) != 0 || !S_ISCHR(st.st_mode) ||
	    (int)minor(st.st_rdev) < MINKAR_DRM_RENDER_MINOR)
		return -1;
	return (int)minor(st.st_rdev);
}

/* Descriptors' connections; a small table, as there are few. */
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static struct {
	int fd;
	io_connect_t conn;
} sConnections[64];

io_connect_t
minkar_drm_connection(int fd)
{
	io_connect_t conn = IO_OBJECT_NULL;

	pthread_mutex_lock(&sLock);
	for (unsigned i = 0; i < sizeof(sConnections) / sizeof(sConnections[0]); i++)
		if (sConnections[i].conn != IO_OBJECT_NULL && sConnections[i].fd == fd)
			conn = sConnections[i].conn;
	if (conn == IO_OBJECT_NULL) {
		struct minkar_drm_gpu gpus[MINKAR_DRM_MAX_GPUS];
		unsigned n = minkar_drm_gpus(gpus, MINKAR_DRM_MAX_GPUS);
		int index = minkar_drm_gpu_for_minor(gpus, n, minkar_drm_fd_minor(fd));
		if (index >= 0 &&
		    IOServiceOpen(gpus[index].service, mach_task_self(), 0, &conn) != KERN_SUCCESS)
			conn = IO_OBJECT_NULL;
		minkar_drm_release_gpus(gpus, n);
		for (unsigned i = 0; conn != IO_OBJECT_NULL &&
		    i < sizeof(sConnections) / sizeof(sConnections[0]); i++)
			if (sConnections[i].conn == IO_OBJECT_NULL) {
				sConnections[i].fd = fd;
				sConnections[i].conn = conn;
				break;
			}
	}
	pthread_mutex_unlock(&sLock);
	return conn;
}

void
minkar_drm_close(int fd)
{
	pthread_mutex_lock(&sLock);
	for (unsigned i = 0; i < sizeof(sConnections) / sizeof(sConnections[0]); i++)
		if (sConnections[i].conn != IO_OBJECT_NULL && sConnections[i].fd == fd) {
			IOServiceClose(sConnections[i].conn);
			sConnections[i].conn = IO_OBJECT_NULL;
		}
	pthread_mutex_unlock(&sLock);
}

static char *
nodeName(int fd)
{
	int minor = minkar_drm_fd_minor(fd);
	char *name = NULL;
	if (minor >= 0 && asprintf(&name, "/dev/dri/renderD%d", minor) < 0)
		name = NULL;
	return name;
}

char *
drmGetRenderDeviceNameFromFd(int fd)
{
	return nodeName(fd);
}

char *
drmGetPrimaryDeviceNameFromFd(int fd)
{
	return nodeName(fd);	/* the render node is the only one */
}

/* ---- The driver ---- */

/* The kernel driver as amdgpu's: Mesa reads its version for what it
 * may use. */
#define MINKAR_DRM_MAJOR	3
#define MINKAR_DRM_MINOR	61

drmVersionPtr
drmGetVersion(int fd)
{
	if (minkar_drm_fd_minor(fd) < 0) {
		errno = ENODEV;
		return NULL;
	}
	drmVersionPtr v = calloc(1, sizeof(*v));
	if (v == NULL)
		return NULL;
	v->version_major = MINKAR_DRM_MAJOR;
	v->version_minor = MINKAR_DRM_MINOR;
	v->version_patchlevel = 0;
	v->name = strdup("amdgpu");
	v->name_len = 6;
	v->date = strdup("20261103");
	v->date_len = 8;
	v->desc = strdup("Minkar's MinkarAMDGPU");
	v->desc_len = (int)strlen(v->desc);
	return v;
}

void
drmFreeVersion(drmVersionPtr v)
{
	if (v == NULL)
		return;
	free(v->name);
	free(v->date);
	free(v->desc);
	free(v);
}

int
drmGetCap(int fd, uint64_t capability, uint64_t *value)
{
	if (minkar_drm_fd_minor(fd) < 0)
		return -ENODEV;
	switch (capability) {
	case DRM_CAP_SYNCOBJ:
	case DRM_CAP_SYNCOBJ_TIMELINE:
		*value = 1;	/* in this process (drm.c's sync objects) */
		return 0;
	default:
		*value = 0;
		return -EINVAL;
	}
}

char *
drmGetFormatModifierName(uint64_t modifier)
{
	(void)modifier;
	return NULL;	/* the caller frees it: none known */
}

/* ---- Commands: the kernel interface, on the user client ---- */

/* An ioctl as Mesa's os_drm.h makes it (patched to come here on Darwin):
 * the driver commands by their number and size. */
int
drmIoctl(int fd, unsigned long request, void *arg)
{
	unsigned nr = (unsigned)(request & 0xff);
	int err = -ENOTTY;

	if (nr >= DRM_COMMAND_BASE && nr < DRM_COMMAND_END)
		err = minkar_drm_command(fd, nr - DRM_COMMAND_BASE, arg, IOCPARM_LEN(request));
	if (err < 0) {
		errno = -err;
		return -1;
	}
	return 0;
}

/* libdrm's: -errno, with errno set. */
static int
command(int fd, unsigned long index, void *data, unsigned long size)
{
	int err = minkar_drm_command(fd, index, data, size);
	if (err < 0)
		errno = -err;
	return err;
}

int
drmCommandWriteRead(int fd, unsigned long index, void *data, unsigned long size)
{
	return command(fd, index, data, size);
}

int
drmCommandWrite(int fd, unsigned long index, void *data, unsigned long size)
{
	return command(fd, index, data, size);
}

int
drmCommandRead(int fd, unsigned long index, void *data, unsigned long size)
{
	return command(fd, index, data, size);
}

/* ---- Sync objects ----
 *
 * Kept in this process: each a binary state (signaled or not) and a
 * timeline point. Submissions finish before they return (the user
 * client's SubmitGFXVM waits for the fence), so a submission signals its
 * objects as it returns; waits sleep on a condition until then or the
 * deadline (CLOCK_MONOTONIC, nanoseconds; 0: just look). Not shared with
 * other processes yet (no file descriptors for them).
 */

#include <time.h>

/* The table grows as needed (Vulkan's tests chain tens of thousands). */
#define MINKAR_SYNCOBJS_MAX	(1u << 20)

static pthread_mutex_t sSyncLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sSyncCond = PTHREAD_COND_INITIALIZER;
static struct syncobj {
	bool used, signaled;
	uint64_t point;
} *sSyncobjs;
static uint32_t sSyncobjCap, sSyncobjHint = 1;

/* Called locked. */
static struct syncobj *
syncobj(uint32_t handle)
{
	return handle > 0 && handle < sSyncobjCap && sSyncobjs[handle].used ?
	    &sSyncobjs[handle] : NULL;
}

/* MINKAR_DRM_DEBUG=1: a sync object call that failed (not a timeout). */
static int
syncobjResult(const char *call, uint32_t handle, int err)
{
	static int trace = -1;
	if (trace < 0)
		trace = getenv("MINKAR_DRM_DEBUG") != NULL;
	if (trace && err != 0 && err != -ETIME)
		fprintf(stderr, "libdrm: %s of sync object %u: %d\n", call, handle, err);
	return err;
}

/* Twice the room (4096 at first); called locked. */
static bool
growSyncobjs(void)
{
	uint32_t n = sSyncobjCap == 0 ? 4096 : sSyncobjCap * 2;
	if (n > MINKAR_SYNCOBJS_MAX || n <= sSyncobjCap)
		return false;
	struct syncobj *t = realloc(sSyncobjs, n * sizeof(*t));
	if (t == NULL)
		return false;
	memset(t + sSyncobjCap, 0, (n - sSyncobjCap) * sizeof(*t));
	sSyncobjs = t;
	sSyncobjCap = n;
	return true;
}

int
drmSyncobjCreate(int fd, uint32_t flags, uint32_t *handle)
{
	(void)fd;
	pthread_mutex_lock(&sSyncLock);
	for (;;) {
		/* From the last one taken or freed, then from the start. */
		for (uint32_t k = 0; k + 1 < sSyncobjCap; k++) {
			uint32_t h = 1 + (sSyncobjHint - 1 + k) % (sSyncobjCap - 1);
			if (!sSyncobjs[h].used) {
				sSyncobjs[h].used = true;
				sSyncobjs[h].signaled = (flags & DRM_SYNCOBJ_CREATE_SIGNALED) != 0;
				sSyncobjs[h].point = 0;
				sSyncobjHint = h + 1 < sSyncobjCap ? h + 1 : 1;
				*handle = h;
				pthread_mutex_unlock(&sSyncLock);
				return 0;
			}
		}
		if (!growSyncobjs())
			break;
	}
	pthread_mutex_unlock(&sSyncLock);
	return syncobjResult("create", 0, -ENOMEM);
}

int
drmSyncobjDestroy(int fd, uint32_t handle)
{
	(void)fd;
	pthread_mutex_lock(&sSyncLock);
	struct syncobj *o = syncobj(handle);
	if (o != NULL)
		memset(o, 0, sizeof(*o));
	pthread_mutex_unlock(&sSyncLock);
	return syncobjResult("destroy", handle, o != NULL ? 0 : -EINVAL);
}

/* For submissions: signaled, or the timeline at point (0: binary). */
void
minkar_drm_syncobj_signal(uint32_t handle, uint64_t point)
{
	pthread_mutex_lock(&sSyncLock);
	struct syncobj *o = syncobj(handle);
	if (o != NULL) {
		o->signaled = true;
		if (point > o->point)
			o->point = point;
		pthread_cond_broadcast(&sSyncCond);
	}
	pthread_mutex_unlock(&sSyncLock);
}

static bool
reached(const struct syncobj *o, uint64_t point)
{
	return point == 0 ? o->signaled : o->point >= point;
}

/* Until all (or one) of the objects are at their points, or the deadline. */
static int
waitFor(uint32_t *handles, const uint64_t *points, unsigned n, int64_t deadline, unsigned flags,
    uint32_t *first)
{
	bool all = (flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL) != 0;
	int err = 0;

	pthread_mutex_lock(&sSyncLock);
	for (;;) {
		unsigned done = 0;
		int which = -1;
		for (unsigned i = 0; i < n; i++) {
			struct syncobj *o = syncobj(handles[i]);
			if (o == NULL) {
				err = -EINVAL;
				goto out;
			}
			if (reached(o, points != NULL ? points[i] : 0)) {
				done++;
				if (which < 0)
					which = (int)i;
			}
		}
		if ((all && done == n) || (!all && done > 0)) {
			if (first != NULL)
				*first = which < 0 ? 0 : (uint32_t)which;
			break;
		}
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		int64_t ns = (int64_t)now.tv_sec * 1000000000ll + now.tv_nsec;
		if (deadline <= ns) {
			err = -ETIME;
			break;
		}
		/* The condition's clock is the real time one: a bounded nap. */
		struct timespec until;
		clock_gettime(CLOCK_REALTIME, &until);
		int64_t nap = deadline - ns < 100000000ll ? deadline - ns : 100000000ll;
		until.tv_nsec += nap % 1000000000ll;
		until.tv_sec += nap / 1000000000ll + until.tv_nsec / 1000000000l;
		until.tv_nsec %= 1000000000l;
		pthread_cond_timedwait(&sSyncCond, &sSyncLock, &until);
	}
out:
	pthread_mutex_unlock(&sSyncLock);
	/* As libdrm's ioctl wrappers: errno too (Mesa reads ETIME there). */
	if (err != 0)
		errno = -err;
	return err;
}

int
drmSyncobjWait(int fd, uint32_t *handles, unsigned num_handles, int64_t timeout_nsec, unsigned flags,
    uint32_t *first_signaled)
{
	(void)fd;
	return syncobjResult("wait", num_handles ? handles[0] : 0,
	    waitFor(handles, NULL, num_handles, timeout_nsec, flags, first_signaled));
}

int
drmSyncobjTimelineWait(int fd, uint32_t *handles, uint64_t *points, unsigned num_handles,
    int64_t timeout_nsec, unsigned flags, uint32_t *first_signaled)
{
	(void)fd;
	return syncobjResult("timeline wait", num_handles ? handles[0] : 0,
	    waitFor(handles, points, num_handles, timeout_nsec, flags, first_signaled));
}

int
drmSyncobjReset(int fd, const uint32_t *handles, uint32_t handle_count)
{
	(void)fd;
	pthread_mutex_lock(&sSyncLock);
	for (uint32_t i = 0; i < handle_count; i++) {
		struct syncobj *o = syncobj(handles[i]);
		if (o != NULL)
			o->signaled = false;
	}
	pthread_mutex_unlock(&sSyncLock);
	return 0;
}

int
drmSyncobjSignal(int fd, const uint32_t *handles, uint32_t handle_count)
{
	(void)fd;
	for (uint32_t i = 0; i < handle_count; i++)
		minkar_drm_syncobj_signal(handles[i], 0);
	return 0;
}

int
drmSyncobjTimelineSignal(int fd, const uint32_t *handles, uint64_t *points, uint32_t handle_count)
{
	(void)fd;
	for (uint32_t i = 0; i < handle_count; i++)
		minkar_drm_syncobj_signal(handles[i], points[i]);
	return 0;
}

int
drmSyncobjQuery2(int fd, uint32_t *handles, uint64_t *points, uint32_t handle_count, uint32_t flags)
{
	(void)fd;
	(void)flags;
	int err = 0;
	pthread_mutex_lock(&sSyncLock);
	for (uint32_t i = 0; i < handle_count; i++) {
		struct syncobj *o = syncobj(handles[i]);
		if (o == NULL)
			err = -EINVAL;
		else
			points[i] = o->point;
	}
	pthread_mutex_unlock(&sSyncLock);
	return syncobjResult("query", handle_count ? handles[0] : 0, err);
}

int
drmSyncobjQuery(int fd, uint32_t *handles, uint64_t *points, uint32_t handle_count)
{
	return drmSyncobjQuery2(fd, handles, points, handle_count, 0);
}

/* src at src_point (0: its binary state) to dst at dst_point (0: binary). */
int
drmSyncobjTransfer(int fd, uint32_t dst_handle, uint64_t dst_point, uint32_t src_handle,
    uint64_t src_point, uint32_t flags)
{
	(void)fd;
	(void)flags;
	pthread_mutex_lock(&sSyncLock);
	struct syncobj *s = syncobj(src_handle), *d = syncobj(dst_handle);
	bool done = s != NULL && reached(s, src_point);
	pthread_mutex_unlock(&sSyncLock);
	if (s == NULL || d == NULL)
		return syncobjResult("transfer", s == NULL ? src_handle : dst_handle, -EINVAL);
	if (done)
		minkar_drm_syncobj_signal(dst_handle, dst_point);
	else if (dst_point == 0) {
		pthread_mutex_lock(&sSyncLock);
		d->signaled = false;
		pthread_mutex_unlock(&sSyncLock);
	}
	return 0;
}

/* Across processes (file descriptors, sync files): not yet. */
int drmSyncobjHandleToFD(int fd, uint32_t handle, int *obj_fd) { (void)fd; (void)handle; (void)obj_fd; return syncobjResult("HandleToFD", 0, -ENOSYS); }
int drmSyncobjFDToHandle(int fd, int obj_fd, uint32_t *handle) { (void)fd; (void)obj_fd; (void)handle; return syncobjResult("FDToHandle", 0, -ENOSYS); }

/*
 * Sync files: a descriptor holding a sync object's fence. Every submission
 * here has finished when it returns, so a fence that exists has signaled:
 * a sync file is a descriptor of this process (/dev/null's) that the table
 * below marks as one. Exporting an object without a fence (never signaled)
 * fails, as with the kernel; importing one signals the object.
 */
#define MINKAR_SYNC_FILES	1024
static bool sSyncFiles[MINKAR_SYNC_FILES];	/* by descriptor */

int
drmSyncobjExportSyncFile(int fd, uint32_t handle, int *sync_file_fd)
{
	(void)fd;
	pthread_mutex_lock(&sSyncLock);
	struct syncobj *o = syncobj(handle);
	bool fence = o != NULL && (o->signaled || o->point > 0);
	pthread_mutex_unlock(&sSyncLock);
	if (o == NULL || !fence)
		return syncobjResult("sync file export", handle, -EINVAL);
	int f = open("/dev/null", O_RDONLY | O_CLOEXEC);
	if (f < 0)
		return syncobjResult("sync file export", handle, -errno);
	if (f >= MINKAR_SYNC_FILES) {
		close(f);
		return syncobjResult("sync file export", handle, -EMFILE);
	}
	pthread_mutex_lock(&sSyncLock);
	sSyncFiles[f] = true;
	pthread_mutex_unlock(&sSyncLock);
	*sync_file_fd = f;
	return 0;
}

int
drmSyncobjImportSyncFile(int fd, uint32_t handle, int sync_file_fd)
{
	(void)fd;
	pthread_mutex_lock(&sSyncLock);
	bool known = sync_file_fd >= 0 && sync_file_fd < MINKAR_SYNC_FILES && sSyncFiles[sync_file_fd];
	bool there = syncobj(handle) != NULL;
	pthread_mutex_unlock(&sSyncLock);
	if (!known || !there)
		return syncobjResult("sync file import", handle, -EINVAL);
	minkar_drm_syncobj_signal(handle, 0);
	return 0;
}
