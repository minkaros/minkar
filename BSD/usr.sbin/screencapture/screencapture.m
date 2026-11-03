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
 * screencapture: the displays, a region of them or a window, as the
 * window server composites them (CGDisplayCreateImage,
 * CGWindowListCreateImage), to image files (CGImageDestination) or the
 * pasteboard. Interactive selection (-i, -w) and the cursor (-C) come with
 * the desktop.
 */

#import <AppKit/NSPasteboard.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
usage(void)
{
	fprintf(stderr,
	    "usage: screencapture [-cmx] [-D display] [-l windowid] [-R x,y,w,h] [-t format]\n"
	    "                     [-T seconds] [file ...]\n"
	    "  -c         to the pasteboard, not to files\n"
	    "  -D n       display n only (1 is the main one)\n"
	    "  -l id      the window with this number only\n"
	    "  -m         the main display only\n"
	    "  -R x,y,w,h a rectangle of the desktop, in points from the main display's top left\n"
	    "  -t format  png (the default) or tiff\n"
	    "  -T seconds wait this long first\n"
	    "  -x         no sound\n"
	    "  files      one for each display captured\n");
	exit(1);
}

static CFStringRef
typeFor(const char *format)
{
	if (strcmp(format, "png") == 0)
		return CFSTR("public.png");
	if (strcmp(format, "jpg") == 0 || strcmp(format, "jpeg") == 0)
		return CFSTR("public.jpeg");
	if (strcmp(format, "tiff") == 0 || strcmp(format, "tif") == 0)
		return CFSTR("public.tiff");
	return NULL;
}

static NSData *
encode(CGImageRef image, CFStringRef type)
{
	CFMutableDataRef data = CFDataCreateMutable(NULL, 0);
	CGImageDestinationRef d = CGImageDestinationCreateWithData(data, type, 1, NULL);
	BOOL ok = d != NULL;
	if (ok) {
		CGImageDestinationAddImage(d, image, NULL);
		ok = CGImageDestinationFinalize(d);
		CFRelease(d);
	}
	if (!ok) {
		CFRelease(data);
		return nil;
	}
	return [(NSData *)data autorelease];
}

int
main(int argc, char *argv[])
{
	@autoreleasepool {
		BOOL toPasteboard = NO, mainOnly = NO, haveRect = NO;
		long displayIndex = 0;
		CGWindowID window = kCGNullWindowID;
		CGRect rect = CGRectNull;
		CFStringRef type = CFSTR("public.png");
		double delay = 0;
		int ch;

		while ((ch = getopt(argc, argv, "cCD:il:mR:t:T:wxoaSUW")) != -1) {
			switch (ch) {
			case 'c': toPasteboard = YES; break;
			case 'm': mainOnly = YES; break;
			case 'x': case 'o': case 'a': break;	/* no sound; shadows are not drawn */
			case 'D':
				displayIndex = strtol(optarg, NULL, 10);
				if (displayIndex < 1)
					usage();
				break;
			case 'l':
				window = (CGWindowID)strtoul(optarg, NULL, 10);
				if (window == kCGNullWindowID)
					usage();
				break;
			case 'R': {
				double x, y, w, h;
				if (sscanf(optarg, "%lf,%lf,%lf,%lf", &x, &y, &w, &h) != 4 || w <= 0 || h <= 0)
					usage();
				rect = CGRectMake(x, y, w, h);
				haveRect = YES;
				break;
			}
			case 't': {
				type = typeFor(optarg);
				/* A format without an encoder (JPEG, until libjpeg is built
				 * before the frameworks) is refused here, not written empty. */
				CFMutableDataRef probe = CFDataCreateMutable(NULL, 0);
				CGImageDestinationRef d = type != NULL ?
				    CGImageDestinationCreateWithData(probe, type, 1, NULL) : NULL;
				CFRelease(probe);
				if (d == NULL) {
					fprintf(stderr, "screencapture: %s: not a format it writes "
					    "(png, tiff)\n", optarg);
					return 1;
				}
				CFRelease(d);
				break;
			}
			case 'T':
				delay = strtod(optarg, NULL);
				break;
			case 'i': case 'w': case 'W': case 'S': case 'U':
				fprintf(stderr, "screencapture: -%c: interactive selection is not "
				    "available yet\n", ch);
				return 1;
			case 'C':
				fprintf(stderr, "screencapture: -C: the cursor is not captured yet\n");
				return 1;
			default:
				usage();
			}
		}
		argc -= optind;
		argv += optind;
		if (!toPasteboard && argc == 0)
			usage();
		if (delay > 0)
			usleep((useconds_t)(delay * 1e6));

		/* What to capture: a window, a rectangle, or displays. */
		NSMutableArray *images = [NSMutableArray array];
		if (window != kCGNullWindowID || haveRect) {
			CGImageRef image = window != kCGNullWindowID ?
			    CGWindowListCreateImage(haveRect ? rect : CGRectNull,
			    kCGWindowListOptionIncludingWindow, window, kCGWindowImageBoundsIgnoreFraming) :
			    CGWindowListCreateImage(rect, kCGWindowListOptionOnScreenOnly, kCGNullWindowID,
			    kCGWindowImageDefault);
			if (image != NULL)
				[images addObject:(id)image];
			CGImageRelease(image);
		} else {
			CGDirectDisplayID displays[16];
			uint32_t n = 0;
			if (CGGetActiveDisplayList(16, displays, &n) != kCGErrorSuccess)
				n = 0;
			for (uint32_t i = 0; i < n; i++) {
				if (mainOnly && displays[i] != CGMainDisplayID())
					continue;
				if (displayIndex != 0 && (long)i + 1 != displayIndex)
					continue;
				CGImageRef image = CGDisplayCreateImage(displays[i]);
				if (image != NULL)
					[images addObject:(id)image];
				CGImageRelease(image);
			}
		}
		if ([images count] == 0) {
			fprintf(stderr, "screencapture: nothing captured (no window server, no such "
			    "display or window, or not allowed)\n");
			return 1;
		}

		if (toPasteboard) {
			NSData *png = encode((CGImageRef)[images objectAtIndex:0], CFSTR("public.png"));
			NSData *tiff = encode((CGImageRef)[images objectAtIndex:0], CFSTR("public.tiff"));
			NSPasteboard *pb = [NSPasteboard generalPasteboard];
			[pb clearContents];
			if ((png == nil || ![pb setData:png forType:NSPasteboardTypePNG]) |
			    (tiff == nil || ![pb setData:tiff forType:NSPasteboardTypeTIFF])) {
				fprintf(stderr, "screencapture: the pasteboard did not take it\n");
				return 1;
			}
			return 0;
		}
		int failures = 0;
		for (NSUInteger i = 0; i < [images count] && i < (NSUInteger)argc; i++) {
			NSData *data = encode((CGImageRef)[images objectAtIndex:i], type);
			if (data == nil || ![data writeToFile:@(argv[i]) atomically:YES]) {
				fprintf(stderr, "screencapture: %s: cannot write it\n", argv[i]);
				failures++;
			}
		}
		return failures != 0;
	}
}
