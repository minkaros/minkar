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
 * Escape to loginwindow, Control-F2 to the front app's AppKit. The table
 * comes from defaults, changed by
 * /Library/Preferences/org.minkar.symbolichotkeys.plist.
 */

#import <CoreFoundation/CoreFoundation.h>

#import "WSServer.h"
#import "WSConnection.h"
#import "WSWindowList.h"
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
} WSHotKey;

/* Indexed by hotkey number (0 unused). Key codes: Tab 48, Escape 53, F2 120. */
static WSHotKey hotKeys[kSLSMaxSymbolicHotKeys + 1] = {
	[kSLSHotKeyAppSwitcher]		= { 48, WSModifierCommand, YES, NO },
	[kSLSHotKeyAppSwitcherBackward]	= { 48, WSModifierCommand | WSModifierShift, YES, NO },
	[kSLSHotKeyForceQuit]		= { 53, WSModifierCommand | WSModifierOption, YES, NO },
	[kSLSHotKeyMenuBarFocus]	= { 120, WSModifierControl, YES, YES },
};
static __weak WSConnection *owners[kSLSMaxSymbolicHotKeys + 1];
/* The key of the last hotkey sent: its key up is not delivered either. */
static int32_t swallowedKeyCode = -1;

static NSString *const kPreferencesPath = @"/Library/Preferences/org.minkar.symbolichotkeys.plist";

void
WSHotKeysStart(void)
{
	NSDictionary *prefs = [NSDictionary dictionaryWithContentsOfFile:kPreferencesPath];
	NSDictionary *table = prefs[@"AppleSymbolicHotKeys"];
	unsigned changed = 0;

	if (![table isKindOfClass:[NSDictionary class]])
		return;
	for (uint32_t n = 1; n <= kSLSMaxSymbolicHotKeys; n++) {
		NSDictionary *entry = table[[NSString stringWithFormat:@"%u", n]];
		if (![entry isKindOfClass:[NSDictionary class]])
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
	fprintf(stderr, "WindowServer: %u symbolic hotkey(s) from %s\n", changed,
	    kPreferencesPath.UTF8String);
}

BOOL
WSHotKeysHandle(const SLSEvent *e, WSWindow *keyTarget)
{
	if (e->type == kSLSEventKeyUp && e->keyCode == swallowedKeyCode) {
		swallowedKeyCode = -1;
		return YES;
	}
	if (e->type != kSLSEventKeyDown)
		return NO;
	for (uint32_t n = 1; n <= kSLSMaxSymbolicHotKeys; n++) {
		WSHotKey *h = &hotKeys[n];
		if (!h->enabled || h->keyCode != e->keyCode ||
		    (e->flags & kModifierMask) != h->modifiers)
			continue;
		SLSEvent hot = *e;
		hot.type = kSLSEventSymbolicHotKey;
		hot.buttonNumber = n;
		swallowedKeyCode = e->keyCode;
		WSConnection *owner = h->frontApp ? keyTarget.owner : owners[n];
		if (owner != nil) {
			hot.window = h->frontApp ? keyTarget.windowID : 0;
			WSEventsEnqueue(owner, &hot);
		}
		return YES;
	}
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
	else if (hotkey == 0 || hotkey > kSLSMaxSymbolicHotKeys || hotKeys[hotkey].frontApp)
		*result = kSLSErrorIllegalArgument;
	else if (!WSMayControlInput(c))
		*result = kSLSErrorCannotComplete;	/* SHORTCUT(M5): the built-in policy */
	else if (claim) {
		owners[hotkey] = c;
		*result = kSLSErrorSuccess;
	} else {
		if (owners[hotkey] == c)
			owners[hotkey] = nil;
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
	if (hotkey == 0 || hotkey > kSLSMaxSymbolicHotKeys) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	*keycode = hotKeys[hotkey].keyCode;
	*modifiers = hotKeys[hotkey].modifiers;
	*enabled = hotKeys[hotkey].enabled;
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}
