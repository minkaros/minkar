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
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
		*value = 0;	/* not yet */
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

int
drmIoctl(int fd, unsigned long request, void *arg)
{
	(void)fd;
	(void)request;
	(void)arg;
	errno = ENOSYS;
	return -1;
}

int
drmCommandWriteRead(int fd, unsigned long index, void *data, unsigned long size)
{
	return minkar_drm_command(fd, index, data, size);
}

int
drmCommandWrite(int fd, unsigned long index, void *data, unsigned long size)
{
	return minkar_drm_command(fd, index, data, size);
}

int
drmCommandRead(int fd, unsigned long index, void *data, unsigned long size)
{
	return minkar_drm_command(fd, index, data, size);
}

/* ---- Sync objects: not yet (DRM_CAP_SYNCOBJ is 0) ---- */

int drmSyncobjCreate(int fd, uint32_t flags, uint32_t *handle) { (void)fd; (void)flags; (void)handle; return -ENOSYS; }
int drmSyncobjDestroy(int fd, uint32_t handle) { (void)fd; (void)handle; return -ENOSYS; }
int drmSyncobjHandleToFD(int fd, uint32_t handle, int *obj_fd) { (void)fd; (void)handle; (void)obj_fd; return -ENOSYS; }
int drmSyncobjFDToHandle(int fd, int obj_fd, uint32_t *handle) { (void)fd; (void)obj_fd; (void)handle; return -ENOSYS; }
int drmSyncobjImportSyncFile(int fd, uint32_t handle, int sync_file_fd) { (void)fd; (void)handle; (void)sync_file_fd; return -ENOSYS; }
int drmSyncobjExportSyncFile(int fd, uint32_t handle, int *sync_file_fd) { (void)fd; (void)handle; (void)sync_file_fd; return -ENOSYS; }
int drmSyncobjWait(int fd, uint32_t *handles, unsigned num_handles, int64_t timeout_nsec, unsigned flags, uint32_t *first_signaled) { (void)fd; (void)handles; (void)num_handles; (void)timeout_nsec; (void)flags; (void)first_signaled; return -ENOSYS; }
int drmSyncobjReset(int fd, const uint32_t *handles, uint32_t handle_count) { (void)fd; (void)handles; (void)handle_count; return -ENOSYS; }
int drmSyncobjSignal(int fd, const uint32_t *handles, uint32_t handle_count) { (void)fd; (void)handles; (void)handle_count; return -ENOSYS; }
int drmSyncobjTimelineSignal(int fd, const uint32_t *handles, uint64_t *points, uint32_t handle_count) { (void)fd; (void)handles; (void)points; (void)handle_count; return -ENOSYS; }
int drmSyncobjTimelineWait(int fd, uint32_t *handles, uint64_t *points, unsigned num_handles, int64_t timeout_nsec, unsigned flags, uint32_t *first_signaled) { (void)fd; (void)handles; (void)points; (void)num_handles; (void)timeout_nsec; (void)flags; (void)first_signaled; return -ENOSYS; }
int drmSyncobjQuery(int fd, uint32_t *handles, uint64_t *points, uint32_t handle_count) { (void)fd; (void)handles; (void)points; (void)handle_count; return -ENOSYS; }
int drmSyncobjQuery2(int fd, uint32_t *handles, uint64_t *points, uint32_t handle_count, uint32_t flags) { (void)fd; (void)handles; (void)points; (void)handle_count; (void)flags; return -ENOSYS; }
int drmSyncobjTransfer(int fd, uint32_t dst_handle, uint64_t dst_point, uint32_t src_handle, uint64_t src_point, uint32_t flags) { (void)fd; (void)dst_handle; (void)dst_point; (void)src_handle; (void)src_point; (void)flags; return -ENOSYS; }
