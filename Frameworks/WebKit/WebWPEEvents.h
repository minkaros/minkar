// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// AppKit's events as WPE's, for a web view's WPE view: the pointer (in the
// view's flipped coordinates), scrolling, keys and modifiers.

#import <AppKit/AppKit.h>
#include <wpe/wpe-platform.h>

NS_ASSUME_NONNULL_BEGIN

/* The event for view, located at point (view coordinates), or NULL when
   it has no WPE counterpart. The caller sends it (wpe_view_event) and
   unrefs it. Every event also updates the modifiers the keymap reports. */
WPEEvent *_Nullable _WebWPEEventCreate(WPEView *view, NSEvent *event, NSPoint point);

/* A key's events for a flagsChanged: event (a modifier pressed or let
   go): its key down or up. */
WPEEvent *_Nullable _WebWPEModifierEventCreate(WPEView *view, NSEvent *event);

NS_ASSUME_NONNULL_END
