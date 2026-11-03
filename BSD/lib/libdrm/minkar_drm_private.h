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
 * Minkar's libdrm: what Mesa's RADV asks of libdrm and libdrm_amdgpu, on
 * MinkarAMDUserClient (IOKit) instead of Linux's DRM device ioctls. Each GPU
 * MinkarAMDGPU drives is a DRM device whose render node,
 * /dev/dri/renderD<minor>, the kernel makes (its MinkarDRMMinor property);
 * a descriptor opened on that node finds its GPU by the node's minor and
 * keeps one user client connection.
 */

#ifndef _MINKAR_DRM_PRIVATE_H
#define _MINKAR_DRM_PRIVATE_H

#include <IOKit/IOKitLib.h>
#include <stdint.h>

#define MINKAR_DRM_RENDER_MINOR	128
#define MINKAR_DRM_MAX_GPUS	8

struct minkar_drm_gpu {
	io_service_t	service;	/* MinkarAMDGPU */
	uint16_t	vendor, device, subvendor, subdevice;
	uint8_t		revision;
	uint8_t		bus, dev, func;
	unsigned	minor;		/* its render node's */
};

/* The GPUs that have a render node. */
unsigned minkar_drm_gpus(struct minkar_drm_gpu *gpus, unsigned max);
void minkar_drm_release_gpus(struct minkar_drm_gpu *gpus, unsigned n);

/* A descriptor's render node minor (a character device's), or -1. */
int minkar_drm_fd_minor(int fd);
/* The GPU with that minor, or -1 (its index in gpus). */
int minkar_drm_gpu_for_minor(const struct minkar_drm_gpu *gpus, unsigned n, int minor);

/* The user client connection for a descriptor, opened at its first use
 * and kept until minkar_drm_close; IO_OBJECT_NULL if it has no GPU. */
io_connect_t minkar_drm_connection(int fd);
void minkar_drm_close(int fd);

/* amdgpu's kernel commands (DRM_AMDGPU_*, drmCommandWrite*'s index):
 * 0 or a negative errno. */
int minkar_drm_command(int fd, unsigned long index, void *data, unsigned long size);

#endif
