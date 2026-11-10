// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// GLib's main context (WPE WebKit's, on the main thread) run by the main
// thread's run loop, in its common modes, so web views work in any
// application without a GLib main loop of its own.

#import <Foundation/Foundation.h>

/* Called on the main thread before the first web view: GLib's default
   context is then the main run loop's. Calling it again does nothing. */
void _WebGLibRunLoopStart(void);
