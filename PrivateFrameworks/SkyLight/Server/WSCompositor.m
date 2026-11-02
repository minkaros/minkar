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
#include <pixman.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#import "WSCompositor.h"
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

@implementation WSCompositor {
	WSDisplay *_display;
	dispatch_queue_t _render;
	dispatch_source_t _link;
	pthread_mutex_t _lock;		/* _damage, _pending, the origin */
	pixman_region32_t _damage;
	BOOL _pending;			/* a frame is on its way */
	double _originX, _originY;	/* the display's, global */
	uint32_t _width, _height;
	/* Render queue only: */
	pixman_image_t *_shadow;
	uint32_t *_shadowPixels;
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
	free(_shadowPixels);
	_shadowPixels = calloc((size_t)width * height, 4);
	if (_shadowPixels == NULL)
		return NO;
	_shadow = pixman_image_create_bits(PIXMAN_x8r8g8b8, width, height, _shadowPixels,
	    width * 4);
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

	/* SHORTCUT(WS5): the timer ticks at the refresh rate even while nothing
	 * changes (no frame is drawn then, but the thread wakes), since GOP has
	 * no vsync; real flips (virtio-gpu) pace frames and let it sleep;
	 * taken in WS2. */
	_link = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _render);
	dispatch_source_set_timer(_link, dispatch_time(DISPATCH_TIME_NOW, interval), interval,
	    interval / 10);
	__unsafe_unretained WSCompositor *me = self;	/* compositors live for good */
	dispatch_source_set_event_handler(_link, ^{ [me tick]; });
	dispatch_resume(_link);
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
	if (x1 > x0 && y1 > y0)
		pixman_region32_union_rect(&_damage, &_damage, (int)x0, (int)y0,
		    (unsigned)(x1 - x0), (unsigned)(y1 - y0));
	pthread_mutex_unlock(&_lock);
}

- (void)damageAll
{
	pthread_mutex_lock(&_lock);
	pixman_region32_union_rect(&_damage, &_damage, 0, 0, _width, _height);
	pthread_mutex_unlock(&_lock);
}

- (void)displayChanged
{
	dispatch_async(_render, ^{
		[self setUpBuffers];
	});
}

/* The display link: a frame when there is damage. */
- (void)tick
{
	pthread_mutex_lock(&_lock);
	BOOL work = pixman_region32_not_empty(&_damage) && !_pending;
	if (work)
		_pending = YES;
	pthread_mutex_unlock(&_lock);
	if (!work) {
		pthread_mutex_lock(&statsLock);
		stats.idleTicks++;
		pthread_mutex_unlock(&statsLock);
		return;
	}
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
	if (cursorVisible && shape != nil) {
		/* The front app's own cursor, at its hot spot. */
		pixman_image_t *image = pixman_image_create_bits(PIXMAN_a8r8g8b8, shape.width,
		    shape.height, (uint32_t *)shape.pixels.bytes, shape.width * 4);
		pixman_image_composite32(PIXMAN_OP_OVER, image, NULL, _shadow, 0, 0, 0, 0,
		    (int)floor(cursor.x - shape.hotX - ox), (int)floor(cursor.y - shape.hotY - oy),
		    shape.width, shape.height);
		pixman_image_unref(image);
	} else if (cursorVisible)
		pixman_image_composite32(PIXMAN_OP_OVER, cursorImage(), NULL, _shadow, 0, 0, 0, 0,
		    (int)floor(cursor.x - ox), (int)floor(cursor.y - oy), kArrowWidth, kArrowHeight);
	pixman_image_set_clip_region32(_shadow, NULL);

	/* Present: the damaged rows, from the shadow to the framebuffer,
	 * through the display's colour table (transfer table and fade) when
	 * it has one. */
	uint32_t shadowRowBytes = (uint32_t)pixman_image_get_stride(_shadow);
	const uint8_t *lut = colors.bytes;	/* red, green, blue */
	for (int i = 0; i < nboxes; i++) {
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
