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
#include <mach/mach_vm.h>
#include <mach/mach_init.h>
#include <math.h>
#include <pixman.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#import "WSCompositor.h"
#import "WSDisplayStream.h"
#import "WSSurface.h"

@implementation WSFrameItem
@end

/* ---- The cursor: an arrow drawn here, hot spot at its tip ---- */

static const char *const arrow[] = {
	"X          ",
	"XX         ",
	"X.X        ",
	"X..X       ",
	"X...X      ",
	"X....X     ",
	"X.....X    ",
	"X......X   ",
	"X.......X  ",
	"X........X ",
	"X.....XXXXX",
	"X..X..X    ",
	"X.X X..X   ",
	"XX  X..X   ",
	"X    X..X  ",
	"     X..X  ",
	"      XX   ",
};
enum { kArrowWidth = 11, kArrowHeight = 17 };

static pixman_image_t *
cursorImage(void)
{
	static pixman_image_t *image;
	static uint32_t pixels[kArrowWidth * kArrowHeight];

	if (image == NULL) {
		for (int y = 0; y < kArrowHeight; y++)
			for (int x = 0; x < kArrowWidth; x++) {
				char c = arrow[y][x];
				pixels[y * kArrowWidth + x] = c == 'X' ? 0xff000000 :
				    c == '.' ? 0xffffffff : 0;
			}
		image = pixman_image_create_bits(PIXMAN_a8r8g8b8, kArrowWidth, kArrowHeight,
		    pixels, kArrowWidth * 4);
	}
	return image;
}

/* ---- Statistics, for every display ---- */

static pthread_mutex_t statsLock = PTHREAD_MUTEX_INITIALIZER;
static SLSServerStats stats;

static uint64_t
micros(uint64_t ticks)
{
	static mach_timebase_info_data_t tb;

	if (tb.denom == 0)
		mach_timebase_info(&tb);
	return ticks * tb.numer / tb.denom / 1000;
}

static NSMutableArray<WSCompositor *> *compositors;

void
WSDamageGlobalRect(SLSRect rect)
{
	for (WSCompositor *c in compositors)
		[c damageGlobalRect:rect];
}

/* Ticks with nothing to do before the display link sleeps. */
static const unsigned kIdleTicks = 2;

@implementation WSCompositor {
	WSDisplay *_display;
	dispatch_queue_t _render;
	dispatch_source_t _link;
	pthread_mutex_t _lock;		/* _damage, _pending, _sleeping, the origin */
	pixman_region32_t _damage;
	BOOL _pending;			/* a frame is on its way */
	BOOL _sleeping;			/* the link is suspended */
	unsigned _idle;			/* ticks in a row with nothing to do */
	double _originX, _originY;	/* the display's, global */
	uint32_t _width, _height;
	/* Render queue only: */
	pixman_region32_t _live;	/* where live surfaces showed last frame */
	NSMutableArray<WSDisplayStream *> *_streams;
	pixman_image_t *_shadow;
	uint32_t *_shadowPixels;	/* page-aligned, rows as the framebuffer's */
	size_t _shadowBytes;
	BOOL _gpuPresent;		/* the GPU copies the shadow to the screen */
	/* Window shadow masks (A8) by window size, "WxH": made on the render
	 * queue, so each compositor keeps its own. */
	NSMutableDictionary<NSString *, NSData *> *_shadowMasks;
	uint8_t *_vram;
	uint32_t _vramRowBytes;
}

+ (void)getStats:(SLSServerStats *)out
{
	pthread_mutex_lock(&statsLock);
	*out = stats;
	pthread_mutex_unlock(&statsLock);
}

- (instancetype)initWithDisplay:(WSDisplay *)display
{
	if ((self = [super init]) == nil)
		return nil;
	_display = display;
	pthread_mutex_init(&_lock, NULL);
	pixman_region32_init(&_damage);
	pixman_region32_init(&_live);
	_render = dispatch_queue_create("org.minkar.WindowServer.render",
	    dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,
	    QOS_CLASS_USER_INTERACTIVE, 0));
	if (![self setUpBuffers])
		return nil;
	if (compositors == nil)
		compositors = [NSMutableArray array];
	[compositors addObject:self];
	return self;
}

/* The framebuffer mapping and the shadow buffer, for the current mode. */
- (BOOL)setUpBuffers
{
	uint8_t *vram = NULL;
	uint32_t rowBytes = 0, width = _display.width, height = _display.height;

	if (![_display mapFramebuffer:&vram rowBytes:&rowBytes]) {
		fprintf(stderr, "WindowServer: display 0x%x: cannot map the framebuffer\n",
		    _display.displayID);
		return NO;
	}
	if (_shadow != NULL)
		pixman_image_unref(_shadow);
	/* Rows as long as the framebuffer's, so damaged rows are one range in
	 * both; page-aligned and whole pages, for the GPU to take. */
	[_display tearDownGPUPresent];
	if (_shadowPixels != NULL)
		mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)(uintptr_t)_shadowPixels,
		    _shadowBytes);
	_shadowPixels = NULL;
	if (rowBytes < width * 4)
		rowBytes = width * 4;
	_shadowBytes = ((size_t)rowBytes * height + 4095) & ~(size_t)4095;
	mach_vm_address_t pixels = 0;
	if (mach_vm_allocate(mach_task_self(), &pixels, _shadowBytes, VM_FLAGS_ANYWHERE) !=
	    KERN_SUCCESS)
		return NO;
	_shadowPixels = (uint32_t *)(uintptr_t)pixels;
	_shadow = pixman_image_create_bits(PIXMAN_x8r8g8b8, width, height, _shadowPixels,
	    rowBytes);
	_gpuPresent = [_display setUpGPUPresentFrom:_shadowPixels bytes:_shadowBytes];
	fprintf(stderr, "WindowServer: display 0x%x: %ux%u, presented by %s\n", _display.displayID,
	    width, height, _gpuPresent ? "the GPU's DMA engine" : "the CPU");
	_vram = vram;
	_vramRowBytes = rowBytes;
	pthread_mutex_lock(&_lock);
	_width = width;
	_height = height;
	_originX = _display.x;
	_originY = _display.y;
	pthread_mutex_unlock(&_lock);
	[self damageAll];
	return YES;
}

- (void)start
{
	const SLSDisplayMode *mode = [_display currentMode];
	double hz = (mode != NULL && mode->refreshRate >= 20) ? mode->refreshRate : 60;
	uint64_t interval = (uint64_t)(NSEC_PER_SEC / hz);

	/* The display link ticks at the refresh rate while there is work, and
	 * sleeps after kIdleTicks ticks with none; damage wakes it. */
	_link = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _render);
	dispatch_source_set_timer(_link, dispatch_time(DISPATCH_TIME_NOW, interval), interval,
	    interval / 10);
	__unsafe_unretained WSCompositor *me = self;	/* compositors live for good */
	dispatch_source_set_event_handler(_link, ^{ [me tick]; });
	dispatch_resume(_link);
}

/* Called with _lock held, after damage was added. */
- (void)wakeLink
{
	if (_sleeping) {
		_sleeping = NO;
		dispatch_resume(_link);
	}
}

- (void)damageGlobalRect:(SLSRect)r
{
	pthread_mutex_lock(&_lock);
	double x0 = floor(r.x - _originX), y0 = floor(r.y - _originY);
	double x1 = ceil(r.x + r.width - _originX), y1 = ceil(r.y + r.height - _originY);
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > _width) x1 = _width;
	if (y1 > _height) y1 = _height;
	if (x1 > x0 && y1 > y0) {
		pixman_region32_union_rect(&_damage, &_damage, (int)x0, (int)y0,
		    (unsigned)(x1 - x0), (unsigned)(y1 - y0));
		[self wakeLink];
	}
	pthread_mutex_unlock(&_lock);
}

- (void)damageAll
{
	pthread_mutex_lock(&_lock);
	pixman_region32_union_rect(&_damage, &_damage, 0, 0, _width, _height);
	[self wakeLink];
	pthread_mutex_unlock(&_lock);
}

- (void)addStream:(id)stream
{
	dispatch_async(_render, ^{
		if (self->_streams == nil)
			self->_streams = [NSMutableArray array];
		[self->_streams addObject:stream];
		/* Its first frame: everything it shows. */
		[self damageAll];
	});
}

- (id)removeStreamWithID:(uint32_t)streamID
{
	__block WSDisplayStream *found = nil;

	dispatch_sync(_render, ^{
		for (WSDisplayStream *s in self->_streams)
			if (s.streamID == streamID)
				found = s;
		if (found != nil) {
			[found stop];
			[self->_streams removeObject:found];
		}
	});
	return found;
}

- (void)removeStreamsOfOwner:(id)owner
{
	dispatch_async(_render, ^{
		for (WSDisplayStream *s in [self->_streams copy])
			if (s.owner == owner || s.owner == nil) {
				[s stop];
				[self->_streams removeObject:s];
			}
	});
}

/* Render queue: the frame to the streams that take it with or without
 * the cursor; what one cannot take yet is drawn again at the next tick. */
- (void)offerFrame:(pixman_region32_t *)damage cursor:(BOOL)cursor
    again:(pixman_region32_t *)again
{
	for (WSDisplayStream *s in _streams) {
		if (s.showsCursor != cursor)
			continue;
		pixman_region32_t pending;
		pixman_region32_init(&pending);
		if (![s offerFrame:_shadow damage:damage pending:&pending])
			pixman_region32_union(again, again, &pending);
		pixman_region32_fini(&pending);
	}
}

/*
 * The damaged rows as bands (overlapping and touching rows merged), each
 * copied whole by the GPU's DMA engine from the shadow to the framebuffer,
 * which have the same layout. NO if a copy failed: the GPU path is given
 * up, and the caller copies this frame itself.
 */
- (BOOL)presentByGPU:(pixman_box32_t *)boxes count:(int)n rowBytes:(uint32_t)rowBytes
{
	int32_t bands[2 * 64];
	int nbands = 0;

	for (int i = 0; i < n; i++) {
		int32_t y1 = boxes[i].y1, y2 = boxes[i].y2;
		int j;
		for (j = 0; j < nbands; j++)
			if (y1 <= bands[2 * j + 1] && y2 >= bands[2 * j]) {
				bands[2 * j] = MIN(bands[2 * j], y1);
				bands[2 * j + 1] = MAX(bands[2 * j + 1], y2);
				break;
			}
		if (j == nbands) {
			if (nbands == 64) {	/* many: one band around them all */
				bands[0] = MIN(bands[0], y1);
				bands[1] = MAX(bands[1], y2);
				for (j = 1; j < nbands; j++) {
					bands[0] = MIN(bands[0], bands[2 * j]);
					bands[1] = MAX(bands[1], bands[2 * j + 1]);
				}
				nbands = 1;
			} else {
				bands[2 * nbands] = y1;
				bands[2 * nbands + 1] = y2;
				nbands++;
			}
		}
	}
	for (int j = 0; j < nbands; j++)
		if (![_display gpuPresentOffset:(size_t)bands[2 * j] * rowBytes
		    bytes:(size_t)(bands[2 * j + 1] - bands[2 * j]) * rowBytes]) {
			fprintf(stderr, "WindowServer: display 0x%x: the GPU did not take a frame; "
			    "the CPU presents from now on\n", _display.displayID);
			_gpuPresent = NO;
			return NO;
		}
	return YES;
}

- (void)displayChanged
{
	dispatch_async(_render, ^{
		[self setUpBuffers];
	});
}

/* The display link: a frame when there is damage; asleep after a few
 * ticks with none (a frame on its way keeps it awake). */
- (void)tick
{
	pthread_mutex_lock(&_lock);
	/* Live surfaces (an app drawing into a captured display) show again
	 * at every frame, as a scanout would. */
	if (pixman_region32_not_empty(&_live))
		pixman_region32_union(&_damage, &_damage, &_live);
	BOOL damaged = pixman_region32_not_empty(&_damage);
	BOOL work = damaged && !_pending;
	if (work)
		_pending = YES;
	BOOL idle = !damaged && !_pending;
	_idle = idle ? _idle + 1 : 0;
	if (idle && _idle >= kIdleTicks && !_sleeping) {
		_sleeping = YES;
		_idle = 0;
		dispatch_suspend(_link);
	}
	pthread_mutex_unlock(&_lock);
	if (idle) {
		pthread_mutex_lock(&statsLock);
		stats.idleTicks++;
		pthread_mutex_unlock(&statsLock);
	}
	if (!work)
		return;
	/* The windows and the damage are taken together, on the IPC queue. */
	dispatch_async(WSIPCQueue, ^{
		NSArray<WSFrameItem *> *items = WSWindowSnapshot();
		SLSPoint cursor;
		WSCursorShape *shape = nil;
		BOOL cursorVisible = WSCursorSnapshot(&cursor, &shape);
		NSData *colors = WSDisplayColorTable(self->_display);
		pixman_region32_t *damage = malloc(sizeof(*damage));

		pixman_region32_init(damage);
		pthread_mutex_lock(&self->_lock);
		pixman_region32_copy(damage, &self->_damage);
		pixman_region32_clear(&self->_damage);
		double ox = self->_originX, oy = self->_originY;
		pthread_mutex_unlock(&self->_lock);
		dispatch_async(self->_render, ^{
			[self composite:items damage:damage cursor:cursor shape:shape
			    visible:cursorVisible colors:colors originX:ox originY:oy];
			pixman_region32_fini(damage);
			free(damage);
			pthread_mutex_lock(&self->_lock);
			self->_pending = NO;
			pthread_mutex_unlock(&self->_lock);
		});
	});
}

/*
 * A window's shadow mask, kWSShadowRadius wider on each side: dark under
 * the window, fading out to nothing at the radius. Separable, from one
 * falloff per axis.
 */
- (NSData *)shadowMaskForWidth:(int)w height:(int)h stride:(int *)stride
{
	const int R = kWSShadowRadius;
	const double maxAlpha = 0.35;
	int mw = w + 2 * R, mh = h + 2 * R;
	NSString *key = [NSString stringWithFormat:@"%dx%d", w, h];
	NSData *mask = _shadowMasks[key];

	*stride = (mw + 3) & ~3;
	if (mask != nil)
		return mask;
	if (_shadowMasks == nil || _shadowMasks.count > 64)
		_shadowMasks = [NSMutableDictionary dictionary];
	NSMutableData *data = [NSMutableData dataWithLength:(NSUInteger)*stride * mh];
	uint8_t *bytes = data.mutableBytes;
	double *fx = calloc(mw, sizeof(double)), *fy = calloc(mh, sizeof(double));
	for (int pass = 0; pass < 2; pass++) {
		int len = pass ? h : w, n = pass ? mh : mw;
		double *f = pass ? fy : fx;
		for (int i = 0; i < n; i++) {
			int p = i - R;	/* from the window's edge */
			double d = p < 0 ? -p : (p >= len ? p - len + 1 : 0);
			double t = d >= R ? 0 : 1 - d / R;
			f[i] = t * t;
		}
	}
	for (int y = 0; y < mh; y++)
		for (int x = 0; x < mw; x++)
			bytes[y * *stride + x] = (uint8_t)(255 * maxAlpha * fx[x] * fy[y] + 0.5);
	free(fx);
	free(fy);
	_shadowMasks[key] = data;
	return data;
}

- (void)composite:(NSArray<WSFrameItem *> *)items damage:(pixman_region32_t *)damage
    cursor:(SLSPoint)cursor shape:(WSCursorShape *)shape visible:(BOOL)cursorVisible
    colors:(NSData *)colors originX:(double)ox originY:(double)oy
{
	uint64_t start = mach_absolute_time();
	pixman_color_t black = { 0, 0, 0, 0xffff };
	int nboxes = 0;
	pixman_box32_t *boxes;
	uint64_t area = 0;

	/* The shadow may be smaller than the damage after a mode change. */
	pixman_region32_intersect_rect(damage, damage, 0, 0,
	    pixman_image_get_width(_shadow), pixman_image_get_height(_shadow));
	boxes = pixman_region32_rectangles(damage, &nboxes);
	if (nboxes == 0)
		return;
	pixman_image_set_clip_region32(_shadow, damage);
	pixman_image_fill_boxes(PIXMAN_OP_SRC, _shadow, &black, nboxes, boxes);

	pixman_region32_clear(&_live);
	for (WSFrameItem *item in items) {
		int x = (int)floor(item.frame.x - ox), y = (int)floor(item.frame.y - oy);

		if (item.solid) {
			pixman_box32_t box = { x, y, x + (int)ceil(item.frame.width),
			    y + (int)ceil(item.frame.height) };
			pixman_image_fill_boxes(PIXMAN_OP_SRC, _shadow, &black, 1, &box);
			continue;
		}
		WSSurface *surface = item.surface;
		if (surface == nil || !surface.committed || item.alpha <= 0)
			continue;
		if (item.shadow) {
			/* The shadow first, black through its mask, faded with the
			 * window's alpha. */
			int sw = (int)ceil(item.frame.width), sh = (int)ceil(item.frame.height), stride;
			NSData *maskData = [self shadowMaskForWidth:sw height:sh stride:&stride];
			pixman_image_t *maskImage = pixman_image_create_bits(PIXMAN_a8,
			    sw + 2 * kWSShadowRadius, sh + 2 * kWSShadowRadius,
			    (uint32_t *)(uintptr_t)maskData.bytes, stride);
			pixman_color_t shade = { 0, 0, 0, (uint16_t)(item.alpha * 0xffff) };
			pixman_image_t *fill = pixman_image_create_solid_fill(&shade);
			pixman_image_composite32(PIXMAN_OP_OVER, fill, maskImage, _shadow, 0, 0, 0, 0,
			    x - kWSShadowRadius, y - kWSShadowRadius + kWSShadowOffsetY,
			    sw + 2 * kWSShadowRadius, sh + 2 * kWSShadowRadius);
			pixman_image_unref(fill);
			pixman_image_unref(maskImage);
		}
		if ([surface live])
			pixman_region32_union_rect(&_live, &_live, x, y, surface.width,
			    surface.height);
		const uint8_t *pixels = [surface lockFront];
		pixman_image_t *src = pixman_image_create_bits(PIXMAN_a8r8g8b8, surface.width,
		    surface.height, (uint32_t *)(uintptr_t)pixels, surface.rowBytes);
		pixman_image_t *mask = NULL;
		if (item.alpha < 1) {
			pixman_color_t a = { 0, 0, 0, (uint16_t)(item.alpha * 0xffff) };
			mask = pixman_image_create_solid_fill(&a);
		}
		pixman_image_composite32(PIXMAN_OP_OVER, src, mask, _shadow, 0, 0, 0, 0, x, y,
		    surface.width, surface.height);
		if (mask != NULL)
			pixman_image_unref(mask);
		pixman_image_unref(src);
		[surface unlock];
	}
	pixman_image_set_clip_region32(_shadow, NULL);

	/* Streams without the cursor take the frame now. */
	pixman_region32_t again;
	pixman_region32_init(&again);
	if (_streams.count > 0)
		[self offerFrame:damage cursor:NO again:&again];

	/*
	 * The cursor: the front app's own, at its hot spot, or the arrow. The
	 * shadow keeps no cursor between frames (streams may not want it):
	 * what is under it is kept here, it is drawn whole, presented where
	 * the frame changed, and what was under it is put back.
	 */
	pixman_image_t *cursorSource = NULL;
	int cx = 0, cy = 0, cw = 0, ch = 0;
	if (cursorVisible && shape != nil) {
		cursorSource = pixman_image_create_bits(PIXMAN_a8r8g8b8, shape.width,
		    shape.height, (uint32_t *)shape.pixels.bytes, shape.width * 4);
		cx = (int)floor(cursor.x - shape.hotX - ox);
		cy = (int)floor(cursor.y - shape.hotY - oy);
		cw = shape.width;
		ch = shape.height;
	} else if (cursorVisible) {
		cursorSource = pixman_image_ref(cursorImage());
		cx = (int)floor(cursor.x - ox);
		cy = (int)floor(cursor.y - oy);
		cw = kArrowWidth;
		ch = kArrowHeight;
	}
	pixman_image_t *under = NULL;
	if (cursorSource != NULL) {
		under = pixman_image_create_bits(PIXMAN_x8r8g8b8, cw, ch, NULL, 0);
		pixman_image_composite32(PIXMAN_OP_SRC, _shadow, NULL, under, cx, cy, 0, 0, 0, 0,
		    cw, ch);
		pixman_image_composite32(PIXMAN_OP_OVER, cursorSource, NULL, _shadow, 0, 0, 0, 0,
		    cx, cy, cw, ch);
		pixman_image_unref(cursorSource);
	}

	/* Present: the damaged rows, from the shadow to the framebuffer,
	 * through the display's colour table (transfer table and fade) when
	 * it has one. */
	uint32_t shadowRowBytes = (uint32_t)pixman_image_get_stride(_shadow);
	const uint8_t *lut = colors.bytes;	/* red, green, blue */
	BOOL presented = NO;
	if (_gpuPresent && lut == NULL && shadowRowBytes == _vramRowBytes)
		presented = [self presentByGPU:boxes count:nboxes rowBytes:shadowRowBytes];
	for (int i = 0; i < nboxes && presented; i++)
		area += (uint64_t)(boxes[i].x2 - boxes[i].x1) * (boxes[i].y2 - boxes[i].y1);
	for (int i = 0; i < nboxes && !presented; i++) {
		size_t bytes = (size_t)(boxes[i].x2 - boxes[i].x1) * 4;
		for (int y = boxes[i].y1; y < boxes[i].y2; y++) {
			uint8_t *to = _vram + (size_t)y * _vramRowBytes + (size_t)boxes[i].x1 * 4;
			const uint8_t *from = (uint8_t *)_shadowPixels + (size_t)y * shadowRowBytes +
			    (size_t)boxes[i].x1 * 4;
			if (lut == NULL) {
				memcpy(to, from, bytes);
				continue;
			}
			for (size_t x = 0; x < bytes; x += 4) {	/* B, G, R, X */
				to[x] = lut[512 + from[x]];
				to[x + 1] = lut[256 + from[x + 1]];
				to[x + 2] = lut[from[x + 2]];
				to[x + 3] = from[x + 3];
			}
		}
		area += (uint64_t)(boxes[i].x2 - boxes[i].x1) * (boxes[i].y2 - boxes[i].y1);
	}
	/* Devices that show only what they are told: each box, or with many
	 * the box around them all. */
	if (nboxes > 8) {
		pixman_box32_t *all = pixman_region32_extents(damage);
		[_display flushX:all->x1 y:all->y1 width:all->x2 - all->x1 height:all->y2 - all->y1];
	} else
		for (int i = 0; i < nboxes; i++)
			[_display flushX:boxes[i].x1 y:boxes[i].y1 width:boxes[i].x2 - boxes[i].x1
			    height:boxes[i].y2 - boxes[i].y1];

	/* Streams with the cursor; then the shadow without it again. What a
	 * stream could not take yet is drawn again at the next tick. */
	if (_streams.count > 0)
		[self offerFrame:damage cursor:YES again:&again];
	if (under != NULL) {
		pixman_image_composite32(PIXMAN_OP_SRC, under, NULL, _shadow, 0, 0, 0, 0, cx, cy,
		    cw, ch);
		pixman_image_unref(under);
	}
	if (pixman_region32_not_empty(&again)) {
		pthread_mutex_lock(&_lock);
		pixman_region32_union(&_damage, &_damage, &again);
		[self wakeLink];
		pthread_mutex_unlock(&_lock);
	}
	pixman_region32_fini(&again);

	uint64_t us = micros(mach_absolute_time() - start);
	pthread_mutex_lock(&statsLock);
	stats.frames++;
	stats.damagedPixels += area;
	stats.lastDamagedPixels = area;
	stats.lastFrameMicros = us;
	stats.totalFrameMicros += us;
	pthread_mutex_unlock(&statsLock);
}

@end

void
WSStatsCountCommit(void)
{
	pthread_mutex_lock(&statsLock);
	stats.commits++;
	pthread_mutex_unlock(&statsLock);
}

void
WSStatsCountEvent(void)
{
	pthread_mutex_lock(&statsLock);
	stats.events++;
	pthread_mutex_unlock(&statsLock);
}
