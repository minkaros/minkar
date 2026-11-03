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
 * amdgpu.h's functions that Mesa's ac_linux_drm.c uses: the device (one for
 * each descriptor), buffers on the user client's (VRAM ones from
 * CreateBuffer, GTT ones from CreateSystemBuffer, a client's own memory from
 * ImportMemory; a buffer's KMS handle is its user client handle), and the
 * GPU virtual address ranges of the client's space, kept here.
 */

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mach/mach.h>
#include <xf86drm.h>
#include <amdgpu.h>
#include <amdgpu_drm.h>
#include "MinkarAMDGPUInterface.h"
#include "minkar_drm_private.h"

/* The client's GPU address space (amd_si_vm.h's kSIVMBytes): the low 4 GB
 * for 32-bit addresses (descriptors, shaders), the rest general. */
#define MINKAR_VA_START		(1ull << 20)
#define MINKAR_VA_32BIT_END	(4ull << 30)
#define MINKAR_VA_END		(64ull << 30)

struct amdgpu_va {
	struct amdgpu_device *dev;
	uint64_t start, size;
	struct amdgpu_va *next;
};

struct amdgpu_device {
	int fd;
	io_connect_t conn;
	unsigned refs;
	pthread_mutex_t lock;
	struct amdgpu_va *ranges;	/* allocated, sorted by start */
	struct amdgpu_device *next;
};

struct amdgpu_bo {
	struct amdgpu_device *dev;
	uint32_t handle;		/* the user client's buffer */
	uint64_t size, gpu;
	void *cpu;
	unsigned maps;
	bool imported;
};

static pthread_mutex_t sDevicesLock = PTHREAD_MUTEX_INITIALIZER;
static struct amdgpu_device *sDevices;

int
amdgpu_device_initialize(int fd, uint32_t *major_version, uint32_t *minor_version,
    amdgpu_device_handle *device_handle)
{
	struct amdgpu_device *d;

	*device_handle = NULL;
	pthread_mutex_lock(&sDevicesLock);
	for (d = sDevices; d != NULL && d->fd != fd; d = d->next)
		;
	if (d != NULL)
		d->refs++;
	else {
		io_connect_t conn = minkar_drm_connection(fd);
		d = conn != IO_OBJECT_NULL ? calloc(1, sizeof(*d)) : NULL;
		if (d == NULL) {
			pthread_mutex_unlock(&sDevicesLock);
			return conn == IO_OBJECT_NULL ? -ENODEV : -ENOMEM;
		}
		d->fd = fd;
		d->conn = conn;
		d->refs = 1;
		pthread_mutex_init(&d->lock, NULL);
		d->next = sDevices;
		sDevices = d;
	}
	pthread_mutex_unlock(&sDevicesLock);
	drmVersionPtr v = drmGetVersion(fd);
	*major_version = v != NULL ? (uint32_t)v->version_major : 3;
	*minor_version = v != NULL ? (uint32_t)v->version_minor : 0;
	drmFreeVersion(v);
	*device_handle = d;
	return 0;
}

int
amdgpu_device_deinitialize(amdgpu_device_handle d)
{
	pthread_mutex_lock(&sDevicesLock);
	if (--d->refs == 0) {
		for (struct amdgpu_device **p = &sDevices; *p != NULL; p = &(*p)->next)
			if (*p == d) {
				*p = d->next;
				break;
			}
		minkar_drm_close(d->fd);
		while (d->ranges != NULL) {
			struct amdgpu_va *r = d->ranges;
			d->ranges = r->next;
			free(r);
		}
		pthread_mutex_destroy(&d->lock);
		free(d);
	}
	pthread_mutex_unlock(&sDevicesLock);
	return 0;
}

int
amdgpu_device_get_fd(amdgpu_device_handle d)
{
	return d->fd;
}

const char *
amdgpu_get_marketing_name(amdgpu_device_handle d)
{
	(void)d;
	return "AMD GCN 1.0 (MinkarAMDGPU)";
}

int
amdgpu_query_sw_info(amdgpu_device_handle d, enum amdgpu_sw_info info, void *value)
{
	(void)d;
	switch (info) {
	case amdgpu_sw_info_address32_hi:
		*(uint32_t *)value = 0;		/* 32-bit addresses are the low 4 GB */
		return 0;
	case amdgpu_sw_info_address_prt_wa_control_bit:
		*(uint64_t *)value = ~0ull;	/* none */
		return 0;
	}
	return -EINVAL;
}

uint64_t
amdgpu_cs_calculate_timeout(uint64_t timeout)
{
	struct timespec now;

	if (timeout == AMDGPU_TIMEOUT_INFINITE || clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return AMDGPU_TIMEOUT_INFINITE;
	uint64_t ns = (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
	return timeout + ns < ns ? AMDGPU_TIMEOUT_INFINITE : timeout + ns;
}

/* ---- Buffers ---- */

static int
kernError(kern_return_t kr)
{
	switch (kr) {
	case KERN_SUCCESS:		return 0;
	case kIOReturnNoMemory:		return -ENOMEM;
	case kIOReturnNoResources:	return -ENOSPC;
	case kIOReturnBadArgument:	return -EINVAL;
	case kIOReturnNotPrivileged:	return -EPERM;
	case kIOReturnUnsupported:	return -ENOTSUP;
	case kIOReturnTimeout:		return -ETIME;
	default:			return -EIO;
	}
}

int
amdgpu_bo_alloc(amdgpu_device_handle d, struct amdgpu_bo_alloc_request *req,
    amdgpu_bo_handle *buf_handle)
{
	uint64_t in = req->alloc_size, out[2] = { 0, 0 };
	uint32_t n = 2;
	bool vram = (req->preferred_heap & AMDGPU_GEM_DOMAIN_VRAM) != 0;

	*buf_handle = NULL;
	if (req->alloc_size == 0)
		return -EINVAL;
	kern_return_t kr = IOConnectCallScalarMethod(d->conn, vram ? kMinkarAMDGPUCreateBuffer :
	    kMinkarAMDGPUCreateSystemBuffer, &in, 1, out, &n);
	if (kr != KERN_SUCCESS)
		return kernError(kr);
	struct amdgpu_bo *bo = calloc(1, sizeof(*bo));
	if (bo == NULL) {
		IOConnectCallScalarMethod(d->conn, kMinkarAMDGPUDestroyBuffer, &out[0], 1, NULL, NULL);
		return -ENOMEM;
	}
	bo->dev = d;
	bo->handle = (uint32_t)out[0];
	bo->gpu = out[1];
	bo->size = (req->alloc_size + 4095) & ~4095ull;
	*buf_handle = bo;
	return 0;
}

int
amdgpu_create_bo_from_user_mem(amdgpu_device_handle d, void *cpu, uint64_t size,
    amdgpu_bo_handle *buf_handle)
{
	uint64_t in[2] = { (uint64_t)(uintptr_t)cpu, size }, out[2] = { 0, 0 };
	uint32_t n = 2;

	*buf_handle = NULL;
	kern_return_t kr = IOConnectCallScalarMethod(d->conn, kMinkarAMDGPUImportMemory, in, 2, out, &n);
	if (kr != KERN_SUCCESS)
		return kernError(kr);
	struct amdgpu_bo *bo = calloc(1, sizeof(*bo));
	if (bo == NULL) {
		IOConnectCallScalarMethod(d->conn, kMinkarAMDGPUDestroyBuffer, &out[0], 1, NULL, NULL);
		return -ENOMEM;
	}
	bo->dev = d;
	bo->handle = (uint32_t)out[0];
	bo->gpu = out[1];
	bo->size = size;
	bo->cpu = cpu;		/* the client's own: never unmapped here */
	bo->imported = true;
	*buf_handle = bo;
	return 0;
}

int
amdgpu_bo_cpu_map(amdgpu_bo_handle bo, void **cpu)
{
	pthread_mutex_lock(&bo->dev->lock);
	if (bo->cpu == NULL) {
		mach_vm_address_t a = 0;
		mach_vm_size_t s = 0;
		kern_return_t kr = IOConnectMapMemory64(bo->dev->conn, bo->handle, mach_task_self(),
		    &a, &s, kIOMapAnywhere);
		if (kr != KERN_SUCCESS) {
			pthread_mutex_unlock(&bo->dev->lock);
			return kernError(kr);
		}
		bo->cpu = (void *)(uintptr_t)a;
	}
	bo->maps++;
	*cpu = bo->cpu;
	pthread_mutex_unlock(&bo->dev->lock);
	return 0;
}

int
amdgpu_bo_cpu_unmap(amdgpu_bo_handle bo)
{
	pthread_mutex_lock(&bo->dev->lock);
	if (bo->maps > 0 && --bo->maps == 0 && !bo->imported) {
		IOConnectUnmapMemory64(bo->dev->conn, bo->handle, mach_task_self(),
		    (mach_vm_address_t)(uintptr_t)bo->cpu);
		bo->cpu = NULL;
	}
	pthread_mutex_unlock(&bo->dev->lock);
	return 0;
}

int
amdgpu_bo_free(amdgpu_bo_handle bo)
{
	if (bo->cpu != NULL && !bo->imported)
		IOConnectUnmapMemory64(bo->dev->conn, bo->handle, mach_task_self(),
		    (mach_vm_address_t)(uintptr_t)bo->cpu);
	uint64_t h = bo->handle;
	IOConnectCallScalarMethod(bo->dev->conn, kMinkarAMDGPUDestroyBuffer, &h, 1, NULL, NULL);
	free(bo);
	return 0;
}

int
amdgpu_bo_export(amdgpu_bo_handle bo, enum amdgpu_bo_handle_type type, uint32_t *shared_handle)
{
	if (type != amdgpu_bo_handle_type_kms && type != amdgpu_bo_handle_type_kms_noimport)
		return -ENOTSUP;	/* across processes: IOSurfaces, later */
	*shared_handle = bo->handle;
	return 0;
}

int
amdgpu_bo_import(amdgpu_device_handle d, enum amdgpu_bo_handle_type type, uint32_t shared_handle,
    struct amdgpu_bo_import_result *output)
{
	(void)d;
	(void)type;
	(void)shared_handle;
	(void)output;
	return -ENOTSUP;
}

/* ---- The client's GPU virtual addresses ---- */

int
amdgpu_va_range_query(amdgpu_device_handle d, enum amdgpu_gpu_va_range type, uint64_t *start,
    uint64_t *end)
{
	(void)d;
	if (type != amdgpu_gpu_va_range_general)
		return -EINVAL;
	*start = MINKAR_VA_START;
	*end = MINKAR_VA_END;
	return 0;
}

int
amdgpu_va_range_alloc(amdgpu_device_handle d, enum amdgpu_gpu_va_range type, uint64_t size,
    uint64_t align, uint64_t required, uint64_t *allocated, amdgpu_va_handle *handle,
    uint64_t flags)
{
	uint64_t lo = MINKAR_VA_START, hi = (flags & AMDGPU_VA_RANGE_32_BIT) ? MINKAR_VA_32BIT_END :
	    MINKAR_VA_END;

	if (type != amdgpu_gpu_va_range_general || size == 0)
		return -EINVAL;
	if (!(flags & AMDGPU_VA_RANGE_32_BIT))
		lo = MINKAR_VA_32BIT_END;	/* keep the low 4 GB for what needs it */
	size = (size + 4095) & ~4095ull;
	align = align < 4096 ? 4096 : align;
	struct amdgpu_va *r = calloc(1, sizeof(*r));
	if (r == NULL)
		return -ENOMEM;
	pthread_mutex_lock(&d->lock);
	/* First fit over the gaps between the sorted ranges. */
	uint64_t at = required != 0 ? required : (lo + align - 1) & ~(align - 1);
	struct amdgpu_va **p = &d->ranges;
	for (; *p != NULL; p = &(*p)->next) {
		struct amdgpu_va *q = *p;
		if (q->start + q->size <= at)
			continue;
		if (at + size <= q->start)
			break;			/* fits before q */
		if (required != 0) {
			at = 0;			/* taken */
			break;
		}
		at = (q->start + q->size + align - 1) & ~(align - 1);
	}
	if (at == 0 || at < lo || at + size > hi || at + size < at) {
		pthread_mutex_unlock(&d->lock);
		free(r);
		return -ENOMEM;
	}
	r->dev = d;
	r->start = at;
	r->size = size;
	r->next = *p;
	*p = r;
	pthread_mutex_unlock(&d->lock);
	*allocated = at;
	*handle = r;
	return 0;
}

int
amdgpu_va_range_free(amdgpu_va_handle r)
{
	struct amdgpu_device *d = r->dev;

	pthread_mutex_lock(&d->lock);
	for (struct amdgpu_va **p = &d->ranges; *p != NULL; p = &(*p)->next)
		if (*p == r) {
			*p = r->next;
			break;
		}
	pthread_mutex_unlock(&d->lock);
	free(r);
	return 0;
}
