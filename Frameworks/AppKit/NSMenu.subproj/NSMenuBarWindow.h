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
 * The menu bar (WindowServer plan, WS4b; Menus.md, MB0): as on the
 * reference platform each app's AppKit draws its own, in a window at the
 * main menu level on every display, shown while the app is in front. It
 * holds the system menu (the Minkar mark) and the app's menus, the app
 * menu first, in bold. With the new window server only.
 */

#import <AppKit/NSPanel.h>

@class NSMenu, NSMenuItem, NSScreen;

@interface NSMenuBarWindow : NSPanel

/* The menu bars, one per screen: shown with the app's main menu when the
 * app becomes active, ordered out when it resigns, and brought up to date
 * when its main menu changes. */
+ (void)_showMenuBars;
+ (void)_hideMenuBars;
+ (void)_mainMenuChanged;
/* Control-F2: the main screen's menu bar takes the keyboard, its first
 * menu (the system menu) selected. */
+ (void)_focusMenuBar;

@end
