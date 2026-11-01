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

#include <pthread.h>
#import "WSSurface.h"

static uint32_t nextSurfaceID = 1;

@implementation WSSurface {
	pthread_mutex_t _lock;
	mach_vm_address_t _base;
	mach_vm_size_t _size;		/* one buffer */
	mach_vm_size_t _allocated;	/* both, in whole pages */
	uint32_t _front;
}

- (instancetype)initWithWidth:(uint32_t)width height:(uint32_t)height
{
	memory_object_size_t entrySize;

	if ((self = [super init]) == nil)
		return nil;
	pthread_mutex_init(&_lock, NULL);
	_surfaceID = nextSurfaceID++;
	_width = width;
	_height = height;
	_rowBytes = width * 4;
	/* The second buffer starts right after the first (rowBytes * height),
	 * as the client computes it; the whole is rounded to pages. */
	_size = (mach_vm_size_t)_rowBytes * height;
	_allocated = round_page(2 * _size);
	/* Zero-filled: transparent until drawn. */
	if (mach_vm_allocate(mach_task_self(), &_base, _allocated, VM_FLAGS_ANYWHERE) !=
	    KERN_SUCCESS)
		return nil;
	entrySize = _allocated;
	if (mach_make_memory_entry_64(mach_task_self(), &entrySize, _base,
	    VM_PROT_READ | VM_PROT_WRITE, &_memoryEntry, MACH_PORT_NULL) != KERN_SUCCESS ||
	    entrySize < _allocated) {
		mach_vm_deallocate(mach_task_self(), _base, _allocated);
		_base = 0;
		return nil;
	}
	_front = 1;	/* the client starts drawing into buffer 0 */
	return self;
}

- (void)dealloc
{
	if (_memoryEntry != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), _memoryEntry);
	if (_base != 0)
		mach_vm_deallocate(mach_task_self(), _base, _allocated);
	pthread_mutex_destroy(&_lock);
}

- (uint32_t)backBuffer
{
	pthread_mutex_lock(&_lock);
	uint32_t back = 1 - _front;
	pthread_mutex_unlock(&_lock);
	return back;
}

- (BOOL)commitBuffer:(uint32_t)buffer
{
	BOOL good;

	pthread_mutex_lock(&_lock);
	good = buffer == 1 - _front;
	if (good) {
		_front = buffer;
		_committed = YES;
	}
	pthread_mutex_unlock(&_lock);
	return good;
}

- (const uint8_t *)lockFront
{
	pthread_mutex_lock(&_lock);
	return (const uint8_t *)(uintptr_t)(_base + _front * _size);
}

- (void)unlock
{
	pthread_mutex_unlock(&_lock);
}

@end
