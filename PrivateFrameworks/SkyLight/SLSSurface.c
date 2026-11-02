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
 * Window surfaces on the client: each window's two buffers mapped from the
 * memory entry the server gives, the back buffer's index, and after a
 * commit the copy that keeps the back buffer equal to the front one.
 */

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "SkyLight.h"
#include "WindowServer.h"	/* MIG user side */

struct surface {
	struct surface		*next;
	SLSWindowID		window;
	uint32_t		surface;	/* the server's name for it */
	uint32_t		width, height, rowBytes;
	uint32_t		back;
	mach_vm_address_t	base;		/* both buffers */
	mach_vm_size_t		size;		/* one buffer */
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct surface *surfaces;

static struct surface **
find(SLSWindowID window)
{
	struct surface **s;

	for (s = &surfaces; *s != NULL; s = &(*s)->next)
		if ((*s)->window == window)
			break;
	return s;
}

/* Forgets a window's surface (its size changed, or it was released). */
void
_SLSForgetSurface(SLSWindowID window)
{
	pthread_mutex_lock(&lock);
	struct surface **p = find(window), *s = *p;
	if (s != NULL) {
		*p = s->next;
		mach_vm_deallocate(mach_task_self(), s->base, 2 * s->size);
		free(s);
	}
	pthread_mutex_unlock(&lock);
}

/* The window's frame changed: its surface goes if the size did. */
void
_SLSFrameChanged(SLSWindowID window, double width, double height)
{
	uint32_t w = (uint32_t)ceil(width), h = (uint32_t)ceil(height);
	int stale;

	pthread_mutex_lock(&lock);
	struct surface *s = *find(window);
	stale = s != NULL && (s->width != w || s->height != h);
	pthread_mutex_unlock(&lock);
	if (stale)
		_SLSForgetSurface(window);
}

/* The window's surface, fetched and mapped if needed; called locked. */
static int
surfaceFor(SLSWindowID window, struct surface **out)
{
	struct surface *s = *find(window);
	mach_port_t server = SLSServerPort(), memory = MACH_PORT_NULL;
	int32_t result = kSLSErrorFailure;
	uint32_t surface, width, height, rowBytes, back;
	mach_vm_address_t base = 0;

	if (s != NULL) {
		*out = s;
		return kSLSErrorSuccess;
	}
	if (server == MACH_PORT_NULL || ws_window_surface(server, window, &result, &surface,
	    &width, &height, &rowBytes, &back, &memory) != KERN_SUCCESS)
		return kSLSErrorFailure;
	if (result != kSLSErrorSuccess)
		return result;
	mach_vm_size_t size = (mach_vm_size_t)rowBytes * height;
	kern_return_t kr = mach_vm_map(mach_task_self(), &base, 2 * size, 0, VM_FLAGS_ANYWHERE,
	    memory, 0, FALSE, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE,
	    VM_INHERIT_NONE);
	mach_port_deallocate(mach_task_self(), memory);	/* the mapping keeps it */
	if (kr != KERN_SUCCESS)
		return kSLSErrorFailure;
	if ((s = calloc(1, sizeof(*s))) == NULL) {
		mach_vm_deallocate(mach_task_self(), base, 2 * size);
		return kSLSErrorFailure;
	}
	s->window = window;
	s->surface = surface;
	s->width = width;
	s->height = height;
	s->rowBytes = rowBytes;
	s->back = back;
	s->base = base;
	s->size = size;
	s->next = surfaces;
	surfaces = s;
	*out = s;
	return kSLSErrorSuccess;
}

static uint8_t *
buffer(struct surface *s, uint32_t index)
{
	return (uint8_t *)(uintptr_t)(s->base + index * s->size);
}

/* The part of r inside the surface; 0 when nothing is left. */
static int
clip(const struct surface *s, SLSIntRect r, SLSIntRect *out)
{
	int64_t x0 = r.x < 0 ? 0 : r.x, y0 = r.y < 0 ? 0 : r.y;
	int64_t x1 = (int64_t)r.x + r.width, y1 = (int64_t)r.y + r.height;

	if (x1 > s->width)
		x1 = s->width;
	if (y1 > s->height)
		y1 = s->height;
	if (r.width <= 0 || r.height <= 0 || x1 <= x0 || y1 <= y0)
		return 0;
	*out = (SLSIntRect){ (int32_t)x0, (int32_t)y0, (int32_t)(x1 - x0), (int32_t)(y1 - y0) };
	return 1;
}

static void
copyRect(uint8_t *dst, uint32_t dstRowBytes, const uint8_t *src, uint32_t srcRowBytes,
    SLSIntRect r)
{
	for (int32_t y = r.y; y < r.y + r.height; y++)
		memcpy(dst + (size_t)y * dstRowBytes + (size_t)r.x * 4,
		    src + (size_t)y * srcRowBytes + (size_t)r.x * 4, (size_t)r.width * 4);
}

int
SLSWindowBackBuffer(SLSWindowID window, void **pixels, uint32_t *width, uint32_t *height,
    uint32_t *rowBytes)
{
	struct surface *s;
	int err;

	if (pixels == NULL || width == NULL || height == NULL || rowBytes == NULL)
		return kSLSErrorIllegalArgument;
	pthread_mutex_lock(&lock);
	if ((err = surfaceFor(window, &s)) == kSLSErrorSuccess) {
		*pixels = buffer(s, s->back);
		*width = s->width;
		*height = s->height;
		*rowBytes = s->rowBytes;
	}
	pthread_mutex_unlock(&lock);
	return err;
}

/* Called locked, with the surface known. */
static int
commit(struct surface *s, const SLSIntRect *damage, uint32_t count)
{
	SLSIntRect rects[kSLSMaxDamageRects];
	uint32_t n = 0, back = 0;
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	/* More rectangles than a message holds: their bounding box. */
	if (count > kSLSMaxDamageRects) {
		int32_t x0 = INT32_MAX, y0 = INT32_MAX, x1 = INT32_MIN, y1 = INT32_MIN;
		for (uint32_t i = 0; i < count; i++) {
			if (damage[i].x < x0) x0 = damage[i].x;
			if (damage[i].y < y0) y0 = damage[i].y;
			if (damage[i].x + damage[i].width > x1) x1 = damage[i].x + damage[i].width;
			if (damage[i].y + damage[i].height > y1) y1 = damage[i].y + damage[i].height;
		}
		SLSIntRect box = { x0, y0, x1 - x0, y1 - y0 };
		if (clip(s, box, &rects[0]))
			n = 1;
	} else
		for (uint32_t i = 0; i < count; i++)
			if (clip(s, damage[i], &rects[n]))
				n++;
	if (server == MACH_PORT_NULL || ws_window_commit(server, s->window, s->surface, s->back,
	    rects, n, &result, &back) != KERN_SUCCESS)
		return kSLSErrorFailure;
	if (result != kSLSErrorSuccess)
		return result;
	/* The new back buffer gets what the committed one changed. */
	for (uint32_t i = 0; i < n; i++)
		copyRect(buffer(s, back), s->rowBytes, buffer(s, s->back), s->rowBytes, rects[i]);
	s->back = back;
	return kSLSErrorSuccess;
}

int
SLSWindowCommit(SLSWindowID window, const SLSIntRect *damage, uint32_t count)
{
	struct surface *s;
	int err;

	if (damage == NULL && count != 0)
		return kSLSErrorIllegalArgument;
	pthread_mutex_lock(&lock);
	s = *find(window);
	err = s ? commit(s, damage, count) : kSLSErrorIllegalArgument;
	pthread_mutex_unlock(&lock);
	/* The window's size changed: the next back buffer is a new surface. */
	if (err == kSLSErrorRangeCheck)
		_SLSForgetSurface(window);
	return err;
}

int
SLSWindowFlushPixels(SLSWindowID window, const void *pixels, uint32_t rowBytes,
    const SLSIntRect *damage, uint32_t count)
{
	struct surface *s;
	int err;

	if (pixels == NULL || (damage == NULL && count != 0))
		return kSLSErrorIllegalArgument;
	/* Twice at most: the surface may have gone stale since it was mapped. */
	for (int attempt = 0; attempt < 2; attempt++) {
		pthread_mutex_lock(&lock);
		if ((err = surfaceFor(window, &s)) == kSLSErrorSuccess) {
			for (uint32_t i = 0; i < count; i++) {
				SLSIntRect r;
				if (clip(s, damage[i], &r))
					copyRect(buffer(s, s->back), s->rowBytes, pixels, rowBytes, r);
			}
			err = commit(s, damage, count);
		}
		pthread_mutex_unlock(&lock);
		if (err != kSLSErrorRangeCheck)
			break;
		_SLSForgetSurface(window);
	}
	return err;
}

int
SLSWindowSetIOSurface(SLSWindowID window, uint32_t surfaceID)
{
	int32_t result = kSLSErrorFailure;
	mach_port_t server = SLSServerPort();

	if (server == MACH_PORT_NULL ||
	    ws_window_set_iosurface(server, window, surfaceID, &result) != KERN_SUCCESS)
		return kSLSErrorFailure;
	/* Its own buffers, if it had any, are gone. */
	_SLSForgetSurface(window);
	return result;
}
