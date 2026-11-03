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
 * amdgpu's kernel commands for buffers, contexts and submissions, on the
 * user client: GEM_VA maps a buffer into the client's GPU address space
 * (MapBuffer), CS runs each indirect buffer in it (SubmitGFXVM, which
 * returns once its fence has signalled) and then signals the submission's
 * sync objects and user fence; WAIT_CS and GEM_WAIT_IDLE find everything
 * done. Buffers' metadata and creation info are kept here.
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>
#include <xf86drm.h>
#include <amdgpu_drm.h>
#include "MinkarAMDGPUInterface.h"
#include "minkar_drm_private.h"

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;

/* ---- Buffers, by KMS handle ---- */

#define MINKAR_BO_HANDLES	(kMinkarAMDGPUMaxBuffers + 1)

static struct minkar_drm_bo sBOs[MINKAR_BO_HANDLES];
static bool sBOUsed[MINKAR_BO_HANDLES];
static void *sBOMaps[MINKAR_BO_HANDLES];	/* for user fences, mapped once */

struct minkar_drm_bo *
minkar_drm_bo_lookup(uint32_t handle)
{
	return handle < MINKAR_BO_HANDLES && sBOUsed[handle] ? &sBOs[handle] : NULL;
}

void
minkar_drm_bo_add(uint32_t handle, uint64_t size, uint64_t alignment, uint32_t domains,
    uint64_t flags)
{
	if (handle >= MINKAR_BO_HANDLES)
		return;
	pthread_mutex_lock(&sLock);
	memset(&sBOs[handle], 0, sizeof(sBOs[handle]));
	sBOs[handle].size = size;
	sBOs[handle].alignment = alignment;
	sBOs[handle].domains = domains;
	sBOs[handle].flags = flags;
	sBOUsed[handle] = true;
	sBOMaps[handle] = NULL;
	pthread_mutex_unlock(&sLock);
}

void
minkar_drm_bo_remove(uint32_t handle)
{
	if (handle >= MINKAR_BO_HANDLES)
		return;
	pthread_mutex_lock(&sLock);
	sBOUsed[handle] = false;
	sBOMaps[handle] = NULL;		/* the user client's mapping goes with the buffer */
	pthread_mutex_unlock(&sLock);
}

static int
metadata(struct drm_amdgpu_gem_metadata *m)
{
	pthread_mutex_lock(&sLock);
	struct minkar_drm_bo *b = minkar_drm_bo_lookup(m->handle);
	int err = 0;
	if (b == NULL)
		err = -ENOENT;
	else if (m->op == AMDGPU_GEM_METADATA_OP_SET_METADATA) {
		if (m->data.data_size_bytes > sizeof(b->metadata))
			err = -EINVAL;
		else {
			b->metadataFlags = m->data.flags;
			b->tilingInfo = m->data.tiling_info;
			b->metadataBytes = m->data.data_size_bytes;
			memcpy(b->metadata, m->data.data, m->data.data_size_bytes);
		}
	} else if (m->op == AMDGPU_GEM_METADATA_OP_GET_METADATA) {
		m->data.flags = b->metadataFlags;
		m->data.tiling_info = b->tilingInfo;
		m->data.data_size_bytes = b->metadataBytes;
		memcpy(m->data.data, b->metadata, b->metadataBytes);
	} else
		err = -EINVAL;
	pthread_mutex_unlock(&sLock);
	return err;
}

static int
gemOp(struct drm_amdgpu_gem_op *op)
{
	pthread_mutex_lock(&sLock);
	struct minkar_drm_bo *b = minkar_drm_bo_lookup(op->handle);
	int err = 0;
	if (b == NULL)
		err = -ENOENT;
	else if (op->op == AMDGPU_GEM_OP_GET_GEM_CREATE_INFO) {
		struct drm_amdgpu_gem_create_in *in = (void *)(uintptr_t)op->value;
		in->bo_size = b->size;
		in->alignment = b->alignment;
		in->domains = b->domains;
		in->domain_flags = b->flags;
	} else if (op->op == AMDGPU_GEM_OP_SET_PLACEMENT)
		err = 0;		/* where it is, it stays */
	else
		err = -EINVAL;
	pthread_mutex_unlock(&sLock);
	return err;
}

static int
kernError(kern_return_t kr)
{
	switch (kr) {
	case KERN_SUCCESS:		return 0;
	case kIOReturnNoMemory:		return -ENOMEM;
	case kIOReturnNoResources:	return -ENOSPC;
	case kIOReturnBadArgument:	return -EINVAL;
	case kIOReturnExclusiveAccess:	return -EEXIST;
	case kIOReturnNotFound:		return -ENOENT;
	case kIOReturnNotPrivileged:	return -EPERM;
	case kIOReturnTimeout:		return -ECANCELED;	/* the processor was reset */
	default:			return -EIO;
	}
}

/* ---- The client's GPU address space ---- */

/* The mappings made, to undo a range (CLEAR, REPLACE). */
#define MINKAR_MAPPINGS	kMinkarAMDGPUMaxMappings
static struct {
	int fd;
	uint64_t va, size;
} sMappings[MINKAR_MAPPINGS];

static int
unmapAt(int fd, io_connect_t conn, uint64_t va)
{
	kern_return_t kr = IOConnectCallScalarMethod(conn, kMinkarAMDGPUUnmapBuffer, &va, 1, NULL, NULL);
	pthread_mutex_lock(&sLock);
	for (unsigned i = 0; i < MINKAR_MAPPINGS; i++)
		if (sMappings[i].size != 0 && sMappings[i].fd == fd && sMappings[i].va == va)
			sMappings[i].size = 0;
	pthread_mutex_unlock(&sLock);
	return kernError(kr);
}

/* The mappings inside [va, va + size), unmapped. */
static void
unmapRange(int fd, io_connect_t conn, uint64_t va, uint64_t size)
{
	uint64_t found[MINKAR_MAPPINGS];
	unsigned n = 0;

	pthread_mutex_lock(&sLock);
	for (unsigned i = 0; i < MINKAR_MAPPINGS; i++)
		if (sMappings[i].size != 0 && sMappings[i].fd == fd && sMappings[i].va >= va &&
		    sMappings[i].va + sMappings[i].size <= va + size)
			found[n++] = sMappings[i].va;
	pthread_mutex_unlock(&sLock);
	for (unsigned i = 0; i < n; i++)
		unmapAt(fd, conn, found[i]);
}

static int
gemVA(int fd, struct drm_amdgpu_gem_va *va)
{
	io_connect_t conn = minkar_drm_connection(fd);
	int err = 0;

	if (conn == IO_OBJECT_NULL)
		return -ENODEV;
	switch (va->operation) {
	case AMDGPU_VA_OP_REPLACE:
		unmapRange(fd, conn, va->va_address, va->map_size);
		/* FALLTHROUGH */
	case AMDGPU_VA_OP_MAP: {
		uint64_t in[4] = { va->handle, va->va_address, va->offset_in_bo, va->map_size };
		err = kernError(IOConnectCallScalarMethod(conn, kMinkarAMDGPUMapBuffer, in, 4, NULL, NULL));
		if (err == 0) {
			pthread_mutex_lock(&sLock);
			for (unsigned i = 0; i < MINKAR_MAPPINGS; i++)
				if (sMappings[i].size == 0) {
					sMappings[i].fd = fd;
					sMappings[i].va = va->va_address;
					sMappings[i].size = va->map_size;
					break;
				}
			pthread_mutex_unlock(&sLock);
		}
		break;
	}
	case AMDGPU_VA_OP_UNMAP:
		err = unmapAt(fd, conn, va->va_address);
		break;
	case AMDGPU_VA_OP_CLEAR:
		unmapRange(fd, conn, va->va_address, va->map_size);
		break;
	default:
		err = -EINVAL;
	}
	/* Mapping is done when the call returns. */
	if (err == 0 && va->vm_timeline_syncobj_out != 0)
		minkar_drm_syncobj_signal(va->vm_timeline_syncobj_out, va->vm_timeline_point);
	return err;
}

/* ---- Contexts and submissions ---- */

static struct {
	bool used, reset;
	uint64_t seq;
} sContexts[256];

static int
context(union drm_amdgpu_ctx *c)
{
	uint32_t id = c->in.ctx_id;
	int err = 0;

	pthread_mutex_lock(&sLock);
	switch (c->in.op) {
	case AMDGPU_CTX_OP_ALLOC_CTX:
		err = -ENOMEM;
		for (id = 1; id < 256; id++)
			if (!sContexts[id].used) {
				memset(&sContexts[id], 0, sizeof(sContexts[id]));
				sContexts[id].used = true;
				memset(&c->out, 0, sizeof(c->out));
				c->out.alloc.ctx_id = id;
				err = 0;
				break;
			}
		break;
	case AMDGPU_CTX_OP_FREE_CTX:
		if (id < 256)
			sContexts[id].used = false;
		break;
	case AMDGPU_CTX_OP_QUERY_STATE:
	case AMDGPU_CTX_OP_QUERY_STATE2: {
		bool reset = id < 256 && sContexts[id].reset;
		memset(&c->out, 0, sizeof(c->out));
		if (c->in.op == AMDGPU_CTX_OP_QUERY_STATE2)
			c->out.state.flags = reset ? AMDGPU_CTX_QUERY2_FLAGS_RESET |
			    AMDGPU_CTX_QUERY2_FLAGS_GUILTY : 0;
		else
			c->out.state.reset_status = reset ? AMDGPU_CTX_GUILTY_RESET : AMDGPU_CTX_NO_RESET;
		break;
	}
	case AMDGPU_CTX_OP_GET_STABLE_PSTATE:
	case AMDGPU_CTX_OP_SET_STABLE_PSTATE:
		memset(&c->out, 0, sizeof(c->out));	/* no power states yet */
		break;
	default:
		err = -EINVAL;
	}
	pthread_mutex_unlock(&sLock);
	return err;
}

/* A user fence: seq written at the buffer's offset, as the processor's
 * end-of-pipe write would have. */
static void
userFence(io_connect_t conn, const struct drm_amdgpu_cs_chunk_fence *f, uint64_t seq)
{
	pthread_mutex_lock(&sLock);
	void *cpu = f->handle < MINKAR_BO_HANDLES ? sBOMaps[f->handle] : NULL;
	pthread_mutex_unlock(&sLock);
	if (cpu == NULL) {
		mach_vm_address_t a = 0;
		mach_vm_size_t s = 0;
		if (IOConnectMapMemory64(conn, f->handle, mach_task_self(), &a, &s, kIOMapAnywhere) !=
		    KERN_SUCCESS)
			return;
		cpu = (void *)(uintptr_t)a;
		pthread_mutex_lock(&sLock);
		if (f->handle < MINKAR_BO_HANDLES)
			sBOMaps[f->handle] = cpu;
		pthread_mutex_unlock(&sLock);
	}
	*(volatile uint64_t *)((uintptr_t)cpu + f->offset) = seq;
	__sync_synchronize();
}

static int
submit(int fd, union drm_amdgpu_cs *cs)
{
	io_connect_t conn = minkar_drm_connection(fd);
	uint32_t ctx = cs->in.ctx_id;
	const uint64_t *chunks = (const uint64_t *)(uintptr_t)cs->in.chunks;
	const struct drm_amdgpu_cs_chunk_fence *fence = NULL;
	int err = 0;

	if (conn == IO_OBJECT_NULL)
		return -ENODEV;
	pthread_mutex_lock(&sLock);
	bool known = ctx < 256 && sContexts[ctx].used, reset = known && sContexts[ctx].reset;
	pthread_mutex_unlock(&sLock);
	if (!known)
		return -EINVAL;
	if (reset)
		return -ECANCELED;

	/* The indirect buffers, in order, each in the client's space; the
	 * waits are met already (every earlier submission has finished). */
	for (uint32_t i = 0; i < cs->in.num_chunks && err == 0; i++) {
		const struct drm_amdgpu_cs_chunk *c = (const void *)(uintptr_t)chunks[i];
		if (c->chunk_id == AMDGPU_CHUNK_ID_IB) {
			const struct drm_amdgpu_cs_chunk_ib *ib = (const void *)(uintptr_t)c->chunk_data;
			if (ib->ip_type != AMDGPU_HW_IP_GFX || (ib->flags & AMDGPU_IB_FLAG_CE)) {
				err = -EINVAL;		/* the graphics ring only, no constant engine */
				break;
			}
			uint64_t in[2] = { ib->va_start, ib->ib_bytes / 4 }, out[2] = { 0, 0 };
			uint32_t n = 2;
			err = kernError(IOConnectCallScalarMethod(conn, kMinkarAMDGPUSubmitGFXVM, in, 2,
			    out, &n));
			if (err == 0 && out[1] != 0 && getenv("MINKAR_DRM_DEBUG") != NULL)
				fprintf(stderr, "libdrm: submission at 0x%llx faulted (0x%llx)\n",
				    (unsigned long long)ib->va_start, (unsigned long long)out[1]);
		} else if (c->chunk_id == AMDGPU_CHUNK_ID_FENCE)
			fence = (const void *)(uintptr_t)c->chunk_data;
	}
	pthread_mutex_lock(&sLock);
	if (err == -ECANCELED)
		sContexts[ctx].reset = true;
	uint64_t seq = ++sContexts[ctx].seq;
	pthread_mutex_unlock(&sLock);
	if (err != 0)
		return err;

	/* Done: the user fence, then the objects the submission signals. */
	if (fence != NULL)
		userFence(conn, fence, seq);
	for (uint32_t i = 0; i < cs->in.num_chunks; i++) {
		const struct drm_amdgpu_cs_chunk *c = (const void *)(uintptr_t)chunks[i];
		if (c->chunk_id == AMDGPU_CHUNK_ID_SYNCOBJ_OUT) {
			const struct drm_amdgpu_cs_chunk_sem *s = (const void *)(uintptr_t)c->chunk_data;
			for (uint32_t k = 0; k < c->length_dw * 4 / sizeof(*s); k++)
				minkar_drm_syncobj_signal(s[k].handle, 0);
		} else if (c->chunk_id == AMDGPU_CHUNK_ID_SYNCOBJ_TIMELINE_SIGNAL) {
			const struct drm_amdgpu_cs_chunk_syncobj *s = (const void *)(uintptr_t)c->chunk_data;
			for (uint32_t k = 0; k < c->length_dw * 4 / sizeof(*s); k++)
				minkar_drm_syncobj_signal(s[k].handle, s[k].point);
		}
	}
	memset(&cs->out, 0, sizeof(cs->out));
	cs->out.handle = seq;
	return 0;
}

/* Everything has finished by the time anyone asks. */
static int
waitCS(union drm_amdgpu_wait_cs *w)
{
	memset(&w->out, 0, sizeof(w->out));
	return 0;
}

static int
waitIdle(union drm_amdgpu_gem_wait_idle *w)
{
	pthread_mutex_lock(&sLock);
	struct minkar_drm_bo *b = minkar_drm_bo_lookup(w->in.handle);
	uint32_t domain = b != NULL ? b->domains : AMDGPU_GEM_DOMAIN_VRAM;
	pthread_mutex_unlock(&sLock);
	memset(&w->out, 0, sizeof(w->out));
	w->out.domain = domain;
	return 0;
}

int
minkar_drm_amdgpu_command(int fd, unsigned long index, void *data, unsigned long size)
{
	switch (index) {
	case DRM_AMDGPU_CTX:
		return size >= sizeof(union drm_amdgpu_ctx) ? context(data) : -EINVAL;
	case DRM_AMDGPU_CS:
		return size >= sizeof(union drm_amdgpu_cs) ? submit(fd, data) : -EINVAL;
	case DRM_AMDGPU_GEM_VA:
		return size >= sizeof(struct drm_amdgpu_gem_va) ? gemVA(fd, data) : -EINVAL;
	case DRM_AMDGPU_WAIT_CS:
		return size >= sizeof(union drm_amdgpu_wait_cs) ? waitCS(data) : -EINVAL;
	case DRM_AMDGPU_GEM_WAIT_IDLE:
		return size >= sizeof(union drm_amdgpu_gem_wait_idle) ? waitIdle(data) : -EINVAL;
	case DRM_AMDGPU_GEM_METADATA:
		return size >= sizeof(struct drm_amdgpu_gem_metadata) ? metadata(data) : -EINVAL;
	case DRM_AMDGPU_GEM_OP:
		return size >= sizeof(struct drm_amdgpu_gem_op) ? gemOp(data) : -EINVAL;
	case DRM_AMDGPU_VM:
		return 0;		/* a reserved context: each client has one already */
	default:
		return -ENOSYS;
	}
}
