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

#include <mach/mach_time.h>
#include <math.h>
#include <stdio.h>
#import "WSDisplayStream.h"

static uint32_t nextStreamID = 1;

@implementation WSDisplayStream {
	SLSDisplayStreamOptions _options;
	/* CF objects, not bridged here: a C array, not an NSArray. */
	IOSurfaceRef _surfaces[8];
	uint32_t _count;
	mach_port_t _notify;
	pixman_region32_t _pending;	/* changed since the last frame sent */
	uint64_t _lastSent;		/* mach_absolute_time */
	BOOL _stopped;
}

- (instancetype)initWithDisplayID:(uint32_t)display surfaces:(const uint32_t *)surfaces
    count:(uint32_t)count options:(SLSDisplayStreamOptions)options notify:(mach_port_t)notify
    owner:(WSConnection *)owner
{
	if ((self = [super init]) == nil)
		return nil;
	pixman_region32_init(&_pending);	/* dealloc finishes it */
	if (count == 0 || count > 8 || options.outputWidth == 0 || options.outputHeight == 0)
		return nil;
	for (uint32_t i = 0; i < count; i++) {
		IOSurfaceRef s = IOSurfaceLookup(surfaces[i]);
		if (s == NULL)
			return nil;
		_surfaces[_count++] = s;	/* released in dealloc */
		if (IOSurfaceGetWidth(s) != options.outputWidth ||
		    IOSurfaceGetHeight(s) != options.outputHeight ||
		    IOSurfaceGetBytesPerElement(s) != 4)
			return nil;
	}
	_options = options;
	_notify = notify;
	_owner = owner;
	_displayID = display;
	_streamID = __atomic_fetch_add(&nextStreamID, 1, __ATOMIC_RELAXED);
	return self;
}

- (void)dealloc
{
	for (uint32_t i = 0; i < _count; i++)
		CFRelease(_surfaces[i]);
	pixman_region32_fini(&_pending);
	if (_notify != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), _notify);
}

- (BOOL)showsCursor
{
	return _options.showCursor != 0;
}

static uint64_t
microsSince(uint64_t then)
{
	static mach_timebase_info_data_t tb;

	if (tb.denom == 0)
		mach_timebase_info(&tb);
	return (mach_absolute_time() - then) * tb.numer / tb.denom / 1000;
}

/* A frame message, sent without waiting: NO when the client's queue is full. */
- (BOOL)send:(uint32_t)status surface:(uint32_t)surface rects:(const SLSIntRect *)rects
    count:(uint32_t)count
{
	SLSDisplayStreamFrame m;

	memset(&m, 0, sizeof(m));
	m.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
	m.header.msgh_size = sizeof(m);
	m.header.msgh_remote_port = _notify;
	m.header.msgh_id = kSLSDisplayStreamMessageID;
	m.stream = _streamID;
	m.status = status;
	m.surface = surface;
	m.count = count;
	m.time = mach_absolute_time();
	if (count > 0)
		memcpy(m.rects, rects, count * sizeof(SLSIntRect));
	return mach_msg(&m.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(m), 0,
	    MACH_PORT_NULL, 0, MACH_PORT_NULL) == MACH_MSG_SUCCESS;
}

- (BOOL)offerFrame:(pixman_image_t *)shadow damage:(pixman_region32_t *)damage
    pending:(pixman_region32_t *)pending
{
	if (_stopped)
		return YES;
	int dw = pixman_image_get_width(shadow), dh = pixman_image_get_height(shadow);
	int sx = _options.sourceX, sy = _options.sourceY;
	int sw = (int)_options.sourceWidth, sh = (int)_options.sourceHeight;
	if (sw <= 0 || sh <= 0) {
		sx = sy = 0;
		sw = dw;
		sh = dh;
	}
	pixman_region32_t changed;
	pixman_region32_init_rect(&changed, sx, sy, (unsigned)sw, (unsigned)sh);
	pixman_region32_intersect(&changed, &changed, damage);
	pixman_region32_union(&_pending, &_pending, &changed);
	pixman_region32_fini(&changed);
	if (!pixman_region32_not_empty(&_pending))
		return YES;	/* nothing it shows changed */

	/* Too soon, or every surface still in the client's hands: later. */
	IOSurfaceRef target = NULL;
	if (_options.minFrameMicros == 0 || _lastSent == 0 ||
	    microsSince(_lastSent) >= _options.minFrameMicros)
		for (uint32_t i = 0; i < _count; i++)
			if (!IOSurfaceIsInUse(_surfaces[i])) {
				target = _surfaces[i];
				break;
			}
	if (target == NULL) {
		pixman_region32_copy(pending, &_pending);
		return NO;
	}

	/* The whole source, scaled: every surface may hold an older frame. */
	uint32_t ow = _options.outputWidth, oh = _options.outputHeight;
	IOSurfaceLock(target, 0, NULL);
	pixman_image_t *dst = pixman_image_create_bits(PIXMAN_a8r8g8b8, (int)ow, (int)oh,
	    IOSurfaceGetBaseAddress(target), (int)IOSurfaceGetBytesPerRow(target));
	double scaleX = (double)sw / ow, scaleY = (double)sh / oh;
	pixman_transform_t t;
	pixman_transform_init_scale(&t, pixman_double_to_fixed(scaleX),
	    pixman_double_to_fixed(scaleY));
	pixman_image_set_transform(shadow, &t);
	if (scaleX != 1 || scaleY != 1)
		pixman_image_set_filter(shadow, PIXMAN_FILTER_BILINEAR, NULL, 0);
	pixman_image_composite32(PIXMAN_OP_SRC, shadow, NULL, dst,
	    (int)floor(sx / scaleX), (int)floor(sy / scaleY), 0, 0, 0, 0, (int)ow, (int)oh);
	pixman_image_set_transform(shadow, NULL);
	pixman_image_set_filter(shadow, PIXMAN_FILTER_NEAREST, NULL, 0);
	pixman_image_unref(dst);
	IOSurfaceUnlock(target, 0, NULL);

	/* What changed, in output pixels; the box around it all past the
	 * message's room. */
	SLSIntRect rects[kSLSMaxStreamRects];
	int n = 0;
	pixman_box32_t *boxes = pixman_region32_rectangles(&_pending, &n);
	pixman_box32_t one;
	if (n > kSLSMaxStreamRects) {
		one = *pixman_region32_extents(&_pending);
		boxes = &one;
		n = 1;
	}
	for (int i = 0; i < n; i++) {
		int x0 = (int)floor((boxes[i].x1 - sx) / scaleX);
		int y0 = (int)floor((boxes[i].y1 - sy) / scaleY);
		int x1 = (int)ceil((boxes[i].x2 - sx) / scaleX);
		int y1 = (int)ceil((boxes[i].y2 - sy) / scaleY);
		rects[i] = (SLSIntRect){ x0, y0, x1 - x0, y1 - y0 };
	}

	IOSurfaceIncrementUseCount(target);
	if (![self send:kSLSDisplayStreamFrameComplete surface:IOSurfaceGetID(target)
	    rects:rects count:(uint32_t)n]) {
		IOSurfaceDecrementUseCount(target);
		pixman_region32_copy(pending, &_pending);
		return NO;
	}
	pixman_region32_clear(&_pending);
	_lastSent = mach_absolute_time();
	return YES;
}

- (void)stop
{
	if (_stopped)
		return;
	_stopped = YES;
	[self send:kSLSDisplayStreamStopped surface:0 rects:NULL count:0];
}

@end
