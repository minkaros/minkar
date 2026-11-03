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

#include <IOKit/IOLib.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include "MinkarAMDFirmware.h"

#define kFirmwareDir	"/usr/share/firmware/amdgpu/"
#define kFirmwareMax	(1024 * 1024)

bool
MinkarAMDReadFirmware(const char *name, uint8_t **data, size_t *bytes)
{
	char path[128];
	vfs_context_t ctx = vfs_context_create(NULL);	/* the kernel's */
	vnode_t vp = NULLVP;
	struct vnode_attr va;
	uint8_t *buffer = NULL;
	size_t size = 0;
	int resid = 0;
	bool ok = false;

	*data = NULL;
	*bytes = 0;
	if (ctx == NULL)
		return false;
	snprintf(path, sizeof(path), kFirmwareDir "%s", name);
	if (vnode_open(path, FREAD, 0, 0, &vp, ctx) != 0) {
		vfs_context_rele(ctx);
		return false;
	}
	VATTR_INIT(&va);
	VATTR_WANTED(&va, va_data_size);
	if (vnode_getattr(vp, &va, ctx) == 0 && va.va_data_size > 0 &&
	    va.va_data_size <= kFirmwareMax) {
		size = (size_t)va.va_data_size;
		buffer = (uint8_t *)IOMalloc(size);
	}
	if (buffer != NULL &&
	    vn_rdwr(UIO_READ, vp, (caddr_t)buffer, (int)size, 0, UIO_SYSSPACE, 0,
	    vfs_context_ucred(ctx), &resid, vfs_context_proc(ctx)) == 0 && resid == 0)
		ok = true;
	vnode_close(vp, FREAD, ctx);
	vfs_context_rele(ctx);
	if (!ok) {
		if (buffer != NULL)
			IOFree(buffer, size);
		return false;
	}
	*data = buffer;
	*bytes = size;
	return true;
}

void
MinkarAMDFreeFirmware(uint8_t *data, size_t bytes)
{
	if (data != NULL)
		IOFree(data, bytes);
}
