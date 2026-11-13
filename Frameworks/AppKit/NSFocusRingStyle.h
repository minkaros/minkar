/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSGraphicsContext.h>
#import <ApplicationServices/ApplicationServices.h>

void _NSFocusRingSaveState(NSGraphicsContext *context);
void _NSFocusRingRestoreState(NSGraphicsContext *context);
/* With a focus ring style set: draws the ring and, as the style says, the
   fill (by calling fill), and returns YES; otherwise NO, nothing drawn. */
BOOL _NSFillWithFocusRing(CGContextRef context, CGPathRef path, void (^fill)(void));
