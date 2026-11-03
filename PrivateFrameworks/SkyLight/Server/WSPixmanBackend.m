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

#include <mach/mach_init.h>
#include <mach/mach_vm.h>
#include <stdio.h>
#include <stdlib.h>
#import "WSCompositorBackend.h"
#import "WSDisplay.h"
#import "WSSurface.h"

/*
 * pixman's backend: the frame is drawn into a shadow buffer in memory with
 * rows as long as the framebuffer's, then its damaged rows go to the
 * framebuffer, by the GPU's DMA engine when the display has one (and no
 * colour table applies), else by the CPU.
 */
@implementation WSPixmanBackend {
	WSDisplay *_display;
	pixman_image_t *_shadow;
	uint32_t *_shadowPixels;	/* page-aligned, rows as the framebuffer's */
	size_t _shadowBytes;
	BOOL _gpuPresent;		/* the GPU copies the shadow to the screen */
	uint8_t *_vram;
	uint32_t _vramRowBytes;
	/* Window shadow masks (A8) by window size, "WxH". */
	NSMutableDictionary<NSString *, NSData *> *_shadowMasks;
	/* What the cursor covers, put back by removeCursor. */
	pixman_image_t *_under;
	int _cx, _cy, _cw, _ch;
}

- (NSString *)backendDescription
{
	return _gpuPresent ? @"pixman, presented by the GPU's DMA engine" : @"pixman, presented by the CPU";
}

- (uint32_t)frameWidth
{
	return _shadow != NULL ? (uint32_t)pixman_image_get_width(_shadow) : 0;
}

- (uint32_t)frameHeight
{
	return _shadow != NULL ? (uint32_t)pixman_image_get_height(_shadow) : 0;
}

- (BOOL)setUpForDisplay:(WSDisplay *)display width:(uint32_t)width height:(uint32_t)height
{
	uint8_t *vram = NULL;
	uint32_t rowBytes = 0;

	_display = display;
	if (![display mapFramebuffer:&vram rowBytes:&rowBytes])
		return NO;
	if (_shadow != NULL)
		pixman_image_unref(_shadow);
	_shadow = NULL;
	/* Rows as long as the framebuffer's, so damaged rows are one range in
	 * both; page-aligned and whole pages, for the GPU to take. */
	[display tearDownGPUPresent];
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
	_gpuPresent = [display setUpGPUPresentFrom:_shadowPixels bytes:_shadowBytes];
	_vram = vram;
	_vramRowBytes = rowBytes;
	return YES;
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

- (void)beginFrame:(pixman_region32_t *)damage
{
	pixman_color_t black = { 0, 0, 0, 0xffff };
	int n = 0;
	pixman_box32_t *boxes = pixman_region32_rectangles(damage, &n);

	pixman_image_set_clip_region32(_shadow, damage);
	pixman_image_fill_boxes(PIXMAN_OP_SRC, _shadow, &black, n, boxes);
}

- (void)fillBlackX:(int)x y:(int)y width:(int)width height:(int)height
{
	pixman_color_t black = { 0, 0, 0, 0xffff };
	pixman_box32_t box = { x, y, x + width, y + height };

	pixman_image_fill_boxes(PIXMAN_OP_SRC, _shadow, &black, 1, &box);
}

- (void)drawShadowX:(int)x y:(int)y width:(int)sw height:(int)sh alpha:(double)alpha
{
	int stride;
	NSData *maskData = [self shadowMaskForWidth:sw height:sh stride:&stride];
	pixman_image_t *maskImage = pixman_image_create_bits(PIXMAN_a8, sw + 2 * kWSShadowRadius,
	    sh + 2 * kWSShadowRadius, (uint32_t *)(uintptr_t)maskData.bytes, stride);
	pixman_color_t shade = { 0, 0, 0, (uint16_t)(alpha * 0xffff) };
	pixman_image_t *fill = pixman_image_create_solid_fill(&shade);

	pixman_image_composite32(PIXMAN_OP_OVER, fill, maskImage, _shadow, 0, 0, 0, 0,
	    x - kWSShadowRadius, y - kWSShadowRadius + kWSShadowOffsetY,
	    sw + 2 * kWSShadowRadius, sh + 2 * kWSShadowRadius);
	pixman_image_unref(fill);
	pixman_image_unref(maskImage);
}

- (void)drawSurface:(WSSurface *)surface x:(int)x y:(int)y alpha:(double)alpha
{
	const uint8_t *pixels = [surface lockFront];
	pixman_image_t *src = pixman_image_create_bits(PIXMAN_a8r8g8b8, surface.width,
	    surface.height, (uint32_t *)(uintptr_t)pixels, surface.rowBytes);
	pixman_image_t *mask = NULL;

	if (alpha < 1) {
		pixman_color_t a = { 0, 0, 0, (uint16_t)(alpha * 0xffff) };
		mask = pixman_image_create_solid_fill(&a);
	}
	pixman_image_composite32(PIXMAN_OP_OVER, src, mask, _shadow, 0, 0, 0, 0, x, y,
	    surface.width, surface.height);
	if (mask != NULL)
		pixman_image_unref(mask);
	pixman_image_unref(src);
	[surface unlock];
}

- (void)endWindows
{
	pixman_image_set_clip_region32(_shadow, NULL);
}

- (pixman_image_t *)frameImage
{
	return _shadow;
}

- (void)drawCursor:(pixman_image_t *)cursor x:(int)x y:(int)y width:(int)w height:(int)h
{
	[self removeCursor];
	_under = pixman_image_create_bits(PIXMAN_x8r8g8b8, w, h, NULL, 0);
	pixman_image_composite32(PIXMAN_OP_SRC, _shadow, NULL, _under, x, y, 0, 0, 0, 0, w, h);
	pixman_image_composite32(PIXMAN_OP_OVER, cursor, NULL, _shadow, 0, 0, 0, 0, x, y, w, h);
	_cx = x;
	_cy = y;
	_cw = w;
	_ch = h;
}

- (void)removeCursor
{
	if (_under == NULL)
		return;
	pixman_image_composite32(PIXMAN_OP_SRC, _under, NULL, _shadow, 0, 0, 0, 0, _cx, _cy, _cw, _ch);
	pixman_image_unref(_under);
	_under = NULL;
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

- (uint64_t)present:(pixman_region32_t *)damage colors:(NSData *)colors
{
	int nboxes = 0;
	pixman_box32_t *boxes = pixman_region32_rectangles(damage, &nboxes);
	uint32_t shadowRowBytes = (uint32_t)pixman_image_get_stride(_shadow);
	const uint8_t *lut = colors.bytes;	/* red, green, blue */
	uint64_t area = 0;
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
	return area;
}

@end
