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
 * Symbolic hotkeys (WindowServer plan, WS4a): as on the reference
 * platform, the server recognizes the system-wide shortcuts and sends
 * each one to its owner instead of delivering it as a key event:
 * Command-Tab to the Dock (which draws the app switcher), Command-Option-
 * Escape to loginwindow, Control-F2 and Command-` to the front app's
 * AppKit, the screenshots and the input sources to SystemUIServer, Option-
 * Command-D to the Dock. Numbered as the reference numbers them, with its
 * defaults; changed by /Library/Preferences/com.apple.symbolichotkeys.plist
 * at start and by the session's user (SLSSetSymbolicHotKey). The app
 * switcher and Force Quit (1-3) are fixed, as there.
 */

#import <CoreFoundation/CoreFoundation.h>

#include <pthread.h>
#include <string.h>
#import "WSServer.h"
#import "WSConnection.h"
#import "WSWindowList.h"
#import "WSEvents.h"
#import "WindowServerServer.h"

#define kModifierMask	(WSModifierShift | WSModifierControl | WSModifierOption | \
			    WSModifierCommand)
/* NSEventModifierFlags (WSKeymap.h's values). */
enum {
	WSModifierShift		= 1 << 17,
	WSModifierControl	= 1 << 18,
	WSModifierOption	= 1 << 19,
	WSModifierCommand	= 1 << 20,
};

typedef struct {
	uint16_t keyCode;
	uint32_t modifiers;
	BOOL enabled;
	BOOL frontApp;		/* owned by the front app, not claimed */
	BOOL known;		/* a hotkey the system has (the others are ignored) */
} WSHotKey;

/* Indexed by hotkey number (0 unused), with the reference's defaults. Key
 * codes: Tab 48, Escape 53, F2 120, ` 50, 3 20, D 2, Space 49. The area
 * screenshots (30, 31) wait for screencapture's interactive selection. */
static WSHotKey hotKeys[kSLSMaxSymbolicHotKeys + 1] = {
	[kSLSHotKeyAppSwitcher]		= { 48, WSModifierCommand, YES, NO, YES },
	[kSLSHotKeyAppSwitcherBackward]	= { 48, WSModifierCommand | WSModifierShift, YES, NO, YES },
	[kSLSHotKeyForceQuit]		= { 53, WSModifierCommand | WSModifierOption, YES, NO, YES },
	[kSLSHotKeyMenuBarFocus]	= { 120, WSModifierControl, YES, YES, YES },
	[kSLSHotKeyNextWindow]		= { 50, WSModifierCommand, YES, YES, YES },
	[kSLSHotKeyScreenshot]		= { 20, WSModifierCommand | WSModifierShift, YES, NO, YES },
	[kSLSHotKeyScreenshotToClipboard] = { 20, WSModifierCommand | WSModifierShift | WSModifierControl, YES, NO, YES },
	[kSLSHotKeyDockHiding]		= { 2, WSModifierCommand | WSModifierOption, YES, NO, YES },
	[kSLSHotKeyPreviousInputSource]	= { 49, WSModifierControl, YES, NO, YES },
	[kSLSHotKeyNextInputSource]	= { 49, WSModifierControl | WSModifierOption, YES, NO, YES },
};

/* The fixed ones, not changed by a preference or a user. */
static BOOL
fixed(uint32_t n)
{
	return n == kSLSHotKeyAppSwitcher || n == kSLSHotKeyAppSwitcherBackward || n == kSLSHotKeyForceQuit;
}
/* The computer's: the defaults with /Library/Preferences' changes. */
static WSHotKey computerHotKeys[kSLSMaxSymbolicHotKeys + 1];
static __weak WSConnection *owners[kSLSMaxSymbolicHotKeys + 1];
/* The table and its owners: claimed on the IPC queue, read by input. */
static pthread_mutex_t hotKeysLock = PTHREAD_MUTEX_INITIALIZER;
/* The key of the last hotkey sent: its key up is not delivered either. */
static int32_t swallowedKeyCode = -1;

static NSString *const kPreferencesPath = @"/Library/Preferences/com.apple.symbolichotkeys.plist";

void
WSHotKeysStart(void)
{
	NSDictionary *prefs = [NSDictionary dictionaryWithContentsOfFile:kPreferencesPath];
	NSDictionary *table = prefs[@"AppleSymbolicHotKeys"];
	unsigned changed = 0;

	if (![table isKindOfClass:[NSDictionary class]]) {
		memcpy(computerHotKeys, hotKeys, sizeof(hotKeys));
		return;
	}
	for (uint32_t n = 1; n <= kSLSMaxSymbolicHotKeys; n++) {
		NSDictionary *entry = table[[NSString stringWithFormat:@"%u", n]];
		if (![entry isKindOfClass:[NSDictionary class]] || !hotKeys[n].known || fixed(n))
			continue;
		if ([entry[@"enabled"] respondsToSelector:@selector(boolValue)])
			hotKeys[n].enabled = [entry[@"enabled"] boolValue];
		NSArray *parameters = entry[@"value"][@"parameters"];
		if ([parameters isKindOfClass:[NSArray class]] && parameters.count >= 3) {
			hotKeys[n].keyCode = (uint16_t)[parameters[1] unsignedIntValue];
			hotKeys[n].modifiers = [parameters[2] unsignedIntValue] & kModifierMask;
		}
		changed++;
	}
	memcpy(computerHotKeys, hotKeys, sizeof(hotKeys));
	fprintf(stderr, "WindowServer: %u symbolic hotkey(s) from %s\n", changed,
	    kPreferencesPath.UTF8String);
}

/* Input queue. */
BOOL
WSHotKeysHandle(const SLSEvent *e, WSConnection *keyOwner, SLSWindowID keyWindow,
    WSConnection *front)
{
	if (e->type == kSLSEventKeyUp && e->keyCode == swallowedKeyCode) {
		swallowedKeyCode = -1;
		return YES;
	}
	if (e->type != kSLSEventKeyDown)
		return NO;
	pthread_mutex_lock(&hotKeysLock);
	for (uint32_t n = 1; n <= kSLSMaxSymbolicHotKeys; n++) {
		WSHotKey *h = &hotKeys[n];
		if (!h->known || !h->enabled || h->keyCode != e->keyCode ||
		    (e->flags & kModifierMask) != h->modifiers)
			continue;
		SLSEvent hot = *e;
		hot.type = kSLSEventSymbolicHotKey;
		hot.buttonNumber = n;
		swallowedKeyCode = e->keyCode;
		/* The front app's hotkeys go to the key window, or the front app
		 * with none (a capture's shielding window first). */
		WSConnection *owner = h->frontApp ? (keyOwner != nil ? keyOwner : front) : owners[n];
		pthread_mutex_unlock(&hotKeysLock);
		if (owner != nil) {
			hot.window = h->frontApp ? keyWindow : 0;
			WSEventsEnqueue(owner, &hot);
		}
		return YES;
	}
	pthread_mutex_unlock(&hotKeysLock);
	return NO;
}

/* ---- Routines ---- */

kern_return_t
wss_hotkey_claim(mach_port_t server, audit_token_t token, uint32_t hotkey, int32_t claim,
    int32_t *result)
{
	WSConnection *c = [WSConnection connectionForToken:token];

	if (c == nil)
		*result = kSLSErrorInvalidConnection;
	else if (hotkey == 0 || hotkey > kSLSMaxSymbolicHotKeys || !hotKeys[hotkey].known || hotKeys[hotkey].frontApp)
		*result = kSLSErrorIllegalArgument;
	else if (!WSMayControlInput(c))
		*result = kSLSErrorCannotComplete;	/* SHORTCUT(M5): the built-in policy */
	else if (claim) {
		pthread_mutex_lock(&hotKeysLock);
		owners[hotkey] = c;
		pthread_mutex_unlock(&hotKeysLock);
		*result = kSLSErrorSuccess;
	} else {
		pthread_mutex_lock(&hotKeysLock);
		if (owners[hotkey] == c)
			owners[hotkey] = nil;
		pthread_mutex_unlock(&hotKeysLock);
		*result = kSLSErrorSuccess;
	}
	return KERN_SUCCESS;
}

kern_return_t
wss_hotkey_get(mach_port_t server, uint32_t hotkey, int32_t *result, uint32_t *keycode,
    uint32_t *modifiers, int32_t *enabled)
{
	*keycode = 0;
	*modifiers = 0;
	*enabled = 0;
	if (hotkey == 0 || hotkey > kSLSMaxSymbolicHotKeys || !hotKeys[hotkey].known) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	*keycode = hotKeys[hotkey].keyCode;
	*modifiers = hotKeys[hotkey].modifiers;
	*enabled = hotKeys[hotkey].enabled;
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

/* The session's user's own key for a hotkey (System Preferences'
 * Keyboard Shortcuts); the fixed ones stay. */
kern_return_t
wss_hotkey_set(mach_port_t server, audit_token_t token, uint32_t hotkey, uint32_t keycode,
    uint32_t modifiers, int32_t enabled, int32_t *result)
{
	WSConnection *c = [WSConnection connectionForToken:token];

	if (c == nil)
		*result = kSLSErrorInvalidConnection;
	else if (hotkey > kSLSMaxSymbolicHotKeys || (hotkey != 0 && (!hotKeys[hotkey].known || fixed(hotkey))))
		*result = kSLSErrorIllegalArgument;
	else if (!WSMayControlInput(c))
		*result = kSLSErrorCannotComplete;
	else if (hotkey == 0) {
		pthread_mutex_lock(&hotKeysLock);
		memcpy(hotKeys, computerHotKeys, sizeof(hotKeys));
		pthread_mutex_unlock(&hotKeysLock);
		*result = kSLSErrorSuccess;
	} else {
		pthread_mutex_lock(&hotKeysLock);
		hotKeys[hotkey].keyCode = (uint16_t)keycode;
		hotKeys[hotkey].modifiers = modifiers & kModifierMask;
		hotKeys[hotkey].enabled = enabled != 0;
		pthread_mutex_unlock(&hotKeysLock);
		*result = kSLSErrorSuccess;
	}
	return KERN_SUCCESS;
}
