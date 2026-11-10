// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// Minkar's WPE platform: the display, toplevels and views WPE WebKit draws
// to and takes input from. Each view WebKit makes has a toplevel of its own
// (its window's part). A view's frames arrive as shared memory buffers
// (BGRA, premultiplied), are copied to its AppKit view and given back at
// once; IOSurfaces come later. The display's clipboard is the system
// pasteboard, its keymap reports the modifiers AppKit's events carry, and
// the cursor a page asks for is AppKit's.

#include <wpe/wpe-platform.h>

G_BEGIN_DECLS

/* The display web views use: connected, and WPE's primary display. */
WPEDisplay *_WebWPEDisplayGet(void);

/* A frame: its pixels (valid during the call only), size and stride, and
   the damaged rectangles in view coordinates (none: all of it). */
typedef void (*_WebWPEFrameFunction)(void *context, const guint8 *pixels, int width, int height,
                                     guint stride, const WPERectangle *damage, guint damageCount);

/* Where a view of the display sends its frames (NULL: nowhere). */
void _WebWPEViewSetFrameFunction(WPEView *view, _WebWPEFrameFunction function, void *context);
/* Its size in points, through its toplevel. */
void _WebWPEViewResize(WPEView *view, int width, int height);
/* Shown in a window or not, and that window the key one or not. */
void _WebWPEViewSetMapped(WPEView *view, gboolean mapped);
void _WebWPEViewSetActive(WPEView *view, gboolean active);

/* The keyboard modifiers now held, as the display's keymap reports them. */
void _WebWPESetModifiers(WPEModifiers modifiers);
/* The system pasteboard offered to WebKit, if another program changed it
   since WebKit last saw it (before a paste). */
void _WebWPEClipboardSync(void);

G_END_DECLS
