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
 * Minkar's additions to libdrm's interface.
 *
 * Buffers shared across processes (MinkarAMDUserClient's shared buffers,
 * known by a name of 64 random bits that their maker hands out: IOSurfaces
 * in VRAM, the window server's frames) are imported into a driver as
 * dma-bufs are on Linux: by a descriptor, which minkar_drm_shared_buffer_fd
 * (in libdrm_amdgpu) makes for a name.
 * Such a descriptor is this process's (it means nothing to another one);
 * Vulkan's import (VkImportMemoryFdInfoKHR, a dma-buf) takes it, and
 * closes it.
 */

#ifndef _MINKAR_DRM_H
#define _MINKAR_DRM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A descriptor for the shared buffer with this name, or -1 (errno). */
int minkar_drm_shared_buffer_fd(uint64_t name);

#ifdef __cplusplus
}
#endif

#endif
