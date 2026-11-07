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
 * NSCursor with the new window server (WindowServer plan, WS4a): a cursor
 * is a premultiplied BGRA image and a hot spot, set for this app with
 * SLSSetCursorImage; the screen shows the front app's. The arrow is the
 * server's own (a NULL image).
 *
 * SHORTCUT(Phase 5): the standard cursors are plain shapes drawn here
 * (black, with a white outline added around them), and the ones without a
 * shape are the arrow, until Atlas's cursor set; taken in WS4a.
 */

#import <AppKit/NSCursor.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSImage.h>
#import <CoreGraphics/CoreGraphics.h>
#include <SkyLight/SkyLight.h>
#include <string.h>

@interface NSCursorImage : NSObject {
@public
    NSData *_pixels;
    uint32_t _width, _height;
    double _hotX, _hotY;
}
@end

@implementation NSCursorImage
- (void)dealloc {
    [_pixels release];
    [super dealloc];
}
@end

/* ---- The standard shapes: 'X' is the cursor, the rest transparent ---- */

typedef struct {
    const char *name;		/* the NSCursor class method */
    int hotX, hotY;		/* in the shape, before the outline */
    const char *rows[20];
} NSCursorShape;

static const NSCursorShape shapes[] = {
    { "IBeamCursor", 3, 8, {
        "XXX XXX",
        "   X   ",
        "   X   ",
        "   X   ",
        "   X   ",
        "   X   ",
        "   X   ",
        "   X   ",
        "   X   ",
        "   X   ",
        "   X   ",
        "   X   ",
        "   X   ",
        "   X   ",
        "   X   ",
        "XXX XXX",
        NULL } },
    { "crosshairCursor", 7, 7, {
        "       X       ",
        "       X       ",
        "       X       ",
        "       X       ",
        "       X       ",
        "               ",
        "               ",
        "XXXXX     XXXXX",
        "               ",
        "               ",
        "       X       ",
        "       X       ",
        "       X       ",
        "       X       ",
        "       X       ",
        NULL } },
    { "pointingHandCursor", 5, 0, {
        "    XX       ",
        "   X  X      ",
        "   X  X      ",
        "   X  X      ",
        "   X  XXXX   ",
        "   X  X  XXX ",
        " XXX  X  X  X",
        "X  X        X",
        "X           X",
        " X          X",
        "  X         X",
        "  X        X ",
        "   X       X ",
        "   X      X  ",
        "    XXXXXX   ",
        NULL } },
    { "resizeLeftRightCursor", 8, 4, {
        "   X       X   ",
        "  XX       XX  ",
        " XXXXXXXXXXXXX ",
        "XXXXXXXXXXXXXXX",
        " XXXXXXXXXXXXX ",
        "  XX       XX  ",
        "   X       X   ",
        NULL } },
    { "resizeUpDownCursor", 3, 7, {
        "   X   ",
        "  XXX  ",
        " XXXXX ",
        "XXXXXXX",
        "  XXX  ",
        "  XXX  ",
        "  XXX  ",
        "  XXX  ",
        "  XXX  ",
        "  XXX  ",
        "  XXX  ",
        "XXXXXXX",
        " XXXXX ",
        "  XXX  ",
        "   X   ",
        NULL } },
    { "operationNotAllowedCursor", 7, 7, {
        "     XXXXX     ",
        "   XX     XX   ",
        "  X         X  ",
        " X         XX X",
        " X        XX  X",
        "X        XX   X",
        "X       XX    X",
        "X      XX     X",
        "X     XX      X",
        "X    XX       X",
        " X  XX        X",
        " X XX        X ",
        "  X         X  ",
        "   XX     XX   ",
        "     XXXXX     ",
        NULL } },
};

/* The ones drawn as another shape. */
static const char *const aliases[][2] = {
    { "resizeLeftCursor", "resizeLeftRightCursor" },
    { "resizeRightCursor", "resizeLeftRightCursor" },
    { "resizeUpCursor", "resizeUpDownCursor" },
    { "resizeDownCursor", "resizeUpDownCursor" },
};

/* A shape with a one-pixel white outline around its black pixels. */
static NSCursorImage *imageForShape(const NSCursorShape *s) {
    int w = 0, h = 0;

    while (s->rows[h] != NULL) {
        int n = (int)strlen(s->rows[h]);
        if (n > w)
            w = n;
        h++;
    }
    int W = w + 2, H = h + 2;
    uint32_t *px = calloc((size_t)W * H, 4);
#define INK(x, y) ((y) >= 0 && (y) < h && (x) >= 0 && (x) < (int)strlen(s->rows[y]) && \
    s->rows[y][x] == 'X')
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int sx = x - 1, sy = y - 1;
            if (INK(sx, sy))
                px[y * W + x] = 0xff000000;
            else {
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        if (INK(sx + dx, sy + dy))
                            px[y * W + x] = 0xffffffff;
            }
        }
#undef INK
    NSCursorImage *image = [NSCursorImage new];
    image->_pixels = [[NSData alloc] initWithBytesNoCopy:px length:(NSUInteger)W * H * 4
        freeWhenDone:YES];
    image->_width = W;
    image->_height = H;
    image->_hotX = s->hotX + 1;
    image->_hotY = s->hotY + 1;
    return image;
}

/* A standard cursor, or nil for the server's arrow. */
id _NSCursorSkyLightWithName(NSString *name) {
    const char *n = [name UTF8String];

    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++)
        if (strcmp(n, aliases[i][0]) == 0)
            n = aliases[i][1];
    for (size_t i = 0; i < sizeof(shapes) / sizeof(shapes[0]); i++)
        if (strcmp(n, shapes[i].name) == 0)
            return imageForShape(&shapes[i]);
    return nil;
}

/* An image's cursor: drawn into BGRA at its size, the hot spot from the
 * top left (as NSCursor's). */
id _NSCursorSkyLightWithImage(NSImage *image, NSPoint hotSpot) {
    size_t width = (size_t)ceil([image size].width), height = (size_t)ceil([image size].height);

    if (width == 0 || height == 0 || width > kSLSMaxCursorSize || height > kSLSMaxCursorSize)
        return nil;
    uint8_t *px = calloc(height, width * 4);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(px, width, height, 8, width * 4, space,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(space);
    if (context == NULL) {
        free(px);
        return nil;
    }
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:
        [NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO]];
    [image drawInRect:NSMakeRect(0, 0, width, height) fromRect:NSZeroRect
        operation:NSCompositeCopy fraction:1.0];
    [NSGraphicsContext restoreGraphicsState];
    [pool release];
    CGContextRelease(context);

    NSCursorImage *cursor = [NSCursorImage new];
    cursor->_pixels = [[NSData alloc] initWithBytesNoCopy:px length:height * width * 4
        freeWhenDone:YES];
    cursor->_width = (uint32_t)width;
    cursor->_height = (uint32_t)height;
    cursor->_hotX = hotSpot.x;
    cursor->_hotY = hotSpot.y;
    return cursor;
}

void _NSCursorSkyLightSet(id object) {
    NSCursorImage *cursor = object;

    if (cursor == nil)
        SLSSetCursorImage(NULL, 0, 0, 0, 0);
    else
        SLSSetCursorImage([cursor->_pixels bytes], cursor->_width, cursor->_height,
            cursor->_hotX, cursor->_hotY);
}
