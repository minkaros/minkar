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
#import "WSCompositorBackend.h"
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
	id<WSCompositorBackend> _backend;	/* draws and presents the frames */
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

/*
 * Which backend draws: pixman, unless the environment's WSCompositor or the
 * preference Compositor (org.minkar.WindowServer) says "vulkan". A display
 * whose GPU has no Vulkan driver falls back to pixman.
 */
static Class
backendClass(void)
{
	NSString *name = nil;
	const char *env = getenv("WSCompositor");

	if (env != NULL)
		name = @(env);
	else {
		CFPropertyListRef value = CFPreferencesCopyAppValue(CFSTR("Compositor"),
		    CFSTR("org.minkar.WindowServer"));
		if (value != NULL && CFGetTypeID(value) == CFStringGetTypeID())
			name = [(__bridge NSString *)value copy];
		if (value != NULL)
			CFRelease(value);
	}
	return [name caseInsensitiveCompare:@"vulkan"] == NSOrderedSame ? [WSVulkanBackend class] :
	    [WSPixmanBackend class];
}

/* The backend's buffers, for the current mode. */
- (BOOL)setUpBuffers
{
	uint32_t width = _display.width, height = _display.height;

	if (_backend == nil)
		_backend = [[backendClass() alloc] init];
	BOOL ready = [_backend setUpForDisplay:_display width:width height:height];
	if (!ready && ![_backend isKindOfClass:[WSPixmanBackend class]]) {
		fprintf(stderr, "WindowServer: display 0x%x: %s cannot draw here; pixman does\n",
		    _display.displayID, NSStringFromClass([_backend class]).UTF8String);
		_backend = [[WSPixmanBackend alloc] init];
		ready = [_backend setUpForDisplay:_display width:width height:height];
	}
	if (!ready) {
		fprintf(stderr, "WindowServer: display 0x%x: cannot map the framebuffer\n",
		    _display.displayID);
		return NO;
	}
	fprintf(stderr, "WindowServer: display 0x%x: %ux%u, %s\n", _display.displayID, width, height,
	    _backend.backendDescription.UTF8String);
	pthread_mutex_lock(&_lock);
	_width = width;
	_height = height;
	_originX = _display.x;
	_originY = _display.y;
	pthread_mutex_unlock(&_lock);
	if (_link != NULL)	/* a new mode may run at another rate */
		[self followRefreshRate];
	[self damageAll];
	return YES;
}

/* The display link's period: the current mode's refresh rate. */
- (void)followRefreshRate
{
	const SLSDisplayMode *mode = [_display currentMode];
	double hz = (mode != NULL && mode->refreshRate >= 20) ? mode->refreshRate : 60;
	uint64_t interval = (uint64_t)(NSEC_PER_SEC / hz);

	dispatch_source_set_timer(_link, dispatch_time(DISPATCH_TIME_NOW, interval), interval,
	    interval / 10);
}

- (void)start
{
	/* The display link ticks at the refresh rate while there is work, and
	 * sleeps after kIdleTicks ticks with none; damage wakes it. */
	_link = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _render);
	[self followRefreshRate];
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
		if (![s offerFrame:[_backend frameImage] damage:damage pending:&pending])
			pixman_region32_union(again, again, &pending);
		pixman_region32_fini(&pending);
	}
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

- (void)composite:(NSArray<WSFrameItem *> *)items damage:(pixman_region32_t *)damage
    cursor:(SLSPoint)cursor shape:(WSCursorShape *)shape visible:(BOOL)cursorVisible
    colors:(NSData *)colors originX:(double)ox originY:(double)oy
{
	uint64_t start = mach_absolute_time();
	int nboxes = 0;
	pixman_box32_t *boxes;

	/* The frame may be smaller than the damage after a mode change. */
	pixman_region32_intersect_rect(damage, damage, 0, 0, _backend.frameWidth, _backend.frameHeight);
	boxes = pixman_region32_rectangles(damage, &nboxes);
	if (nboxes == 0)
		return;
	[_backend beginFrame:damage];

	pixman_region32_clear(&_live);
	for (WSFrameItem *item in items) {
		int x = (int)floor(item.frame.x - ox), y = (int)floor(item.frame.y - oy);

		if (item.solid) {
			[_backend fillBlackX:x y:y width:(int)ceil(item.frame.width)
			    height:(int)ceil(item.frame.height)];
			continue;
		}
		WSSurface *surface = item.surface;
		if (surface == nil || !surface.committed || item.alpha <= 0)
			continue;
		/* The shadow first, black through its mask, faded with the
		 * window's alpha. */
		if (item.shadow)
			[_backend drawShadowX:x y:y width:(int)ceil(item.frame.width)
			    height:(int)ceil(item.frame.height) alpha:item.alpha];
		if ([surface live])
			pixman_region32_union_rect(&_live, &_live, x, y, surface.width,
			    surface.height);
		[_backend drawSurface:surface x:x y:y alpha:item.alpha];
	}
	[_backend endWindows];

	/* Streams without the cursor take the frame now. */
	pixman_region32_t again;
	pixman_region32_init(&again);
	if (_streams.count > 0)
		[self offerFrame:damage cursor:NO again:&again];

	/*
	 * The cursor: the front app's own, at its hot spot, or the arrow. The
	 * frame keeps no cursor between frames (streams may not want it): it
	 * is drawn whole, presented where the frame changed, and taken away.
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
	if (cursorSource != NULL) {
		[_backend drawCursor:cursorSource x:cx y:cy width:cw height:ch];
		pixman_image_unref(cursorSource);
	}

	/* Present: the damaged area, through the display's colour table
	 * (transfer table and fade) when it has one. */
	uint64_t area = [_backend present:damage colors:colors];
	/* Devices that show only what they are told: each box, or with many
	 * the box around them all. */
	if (nboxes > 8) {
		pixman_box32_t *all = pixman_region32_extents(damage);
		[_display flushX:all->x1 y:all->y1 width:all->x2 - all->x1 height:all->y2 - all->y1];
	} else
		for (int i = 0; i < nboxes; i++)
			[_display flushX:boxes[i].x1 y:boxes[i].y1 width:boxes[i].x2 - boxes[i].x1
			    height:boxes[i].y2 - boxes[i].y1];

	/* Streams with the cursor; then the frame without it again. What a
	 * stream could not take yet is drawn again at the next tick. */
	if (_streams.count > 0)
		[self offerFrame:damage cursor:YES again:&again];
	[_backend removeCursor];
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
