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

#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hidsystem/IOLLEvent.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <unistd.h>
#import "WSInput.h"
#import "WSEvents.h"
#import "WSDisplay.h"
#import "WSServer.h"
#include "WSKeymap.h"
#import "WSConnection.h"
#include "WindowServerServer.h"

/* Modifier keys' codes and the flag each one sets. */
static uint32_t
modifierMaskForKey(uint16_t keyCode)
{
	switch (keyCode) {
	case 0x38: case 0x3C: return NX_SHIFTMASK;
	case 0x3B: case 0x3E: return NX_CONTROLMASK;
	case 0x3A: case 0x3D: return NX_ALTERNATEMASK;
	case 0x37: case 0x36: return NX_COMMANDMASK;
	case 0x39: return NX_ALPHASHIFTMASK;
	case 0x3F: return NX_SECONDARYFNMASK;
	}
	return 0;
}

uint64_t
WSNanoseconds(void)
{
	static mach_timebase_info_data_t tb;

	if (tb.denom == 0)
		mach_timebase_info(&tb);
	return mach_absolute_time() * tb.numer / tb.denom;
}

static __unsafe_unretained WSInput *theInput;

/* IOHIDSystem's SetFixedMouseLocData (method 4 of the server connection):
 * 24.8 fixed-point pixels. */
struct mouseLocation {
	uint64_t origTs;
	uint64_t callTs;
	int32_t x;
	int32_t y;
	int32_t pid;
} __attribute__((packed));

/* The server connection's virtual displays (IOHIDSystem methods 7, 8 and
 * 9): register one, unregister it, set its bounds. */
enum {
	kHIDRegisterVirtualDisplay = 7,
	kHIDUnregisterVirtualDisplay = 8,
	kHIDSetVirtualDisplayBounds = 9,
};

/* ---- The keyboard's settings (SLSSetKeyboardSettings) ---- */

static NSString *const kKeyboardPath = @"/var/db/WindowServer/Keyboard.plist";
static SLSKeyboardSettings keyboard = { "us", "", 500, 33 };

/* The keymap of the saved settings (the default, us, if there are none or
 * they do not load), and their repeat; at start, on the input queue. */
static WSKeymap *
WSInputLoadKeyboard(void)
{
	NSDictionary *saved = [NSDictionary dictionaryWithContentsOfFile:kKeyboardPath];
	NSString *layout = saved[@"Layout"], *variant = saved[@"Variant"];
	WSKeymap *km = NULL;

	if ([layout isKindOfClass:[NSString class]] && [layout length] < sizeof(keyboard.layout)) {
		NSString *v = [variant isKindOfClass:[NSString class]] ? variant : @"";
		km = WSKeymapCreate(NULL, NULL, NULL, [layout UTF8String], [v UTF8String], NULL);
		if (km != NULL) {
			strlcpy(keyboard.layout, [layout UTF8String], sizeof(keyboard.layout));
			strlcpy(keyboard.variant, [v UTF8String], sizeof(keyboard.variant));
		}
	}
	if (km == NULL)
		km = WSKeymapCreate(NULL, NULL, NULL, NULL, NULL, NULL);
	uint32_t initial = [saved[@"InitialRepeat"] unsignedIntValue], interval = [saved[@"RepeatInterval"] unsignedIntValue];
	if (initial >= 15 && initial <= 2000 && interval >= 15 && interval <= 2000) {
		keyboard.initialRepeat = initial;
		keyboard.repeatInterval = interval;
	}
	WSEventsSetKeyRepeat((uint64_t)keyboard.initialRepeat * 1000000, (uint64_t)keyboard.repeatInterval * 1000000);
	return km;
}

@implementation WSInput {
	NSMutableArray<NSNumber *> *_screenTokens;	/* one per display */
	io_connect_t _connect;
	volatile EvGlobals *_evg;
	mach_port_t _port;
	dispatch_source_t _source;
	WSKeymap *_keymap;
}

+ (instancetype)startOnQueue:(dispatch_queue_t)queue
{
	WSInput *input = [[WSInput alloc] init];

	if (![input openOnQueue:queue])
		return nil;
	theInput = input;
	return input;
}

/* A new keymap, on the input queue: the old one goes. */
- (WSKeymap *)keymap
{
	return _keymap;
}

- (void)replaceKeymap:(WSKeymap *)keymap
{
	WSKeymap *old = _keymap;

	_keymap = keymap;
	if (old != NULL)
		WSKeymapDestroy(old);
}

- (BOOL)openOnQueue:(dispatch_queue_t)queue
{
	io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault,
	    IOServiceMatching(kIOHIDSystemClass));
	uint64_t version = kIOHIDCurrentShmemVersion;
	mach_vm_address_t addr = 0;
	mach_vm_size_t size = 0;
	kern_return_t kr;

	_keymap = WSInputLoadKeyboard();
	if (_keymap == NULL)
		fprintf(stderr, "WindowServer: no keyboard layout: keys are not delivered\n");
	if (service == IO_OBJECT_NULL) {
		fprintf(stderr, "WindowServer: no IOHIDSystem: no input\n");
		return NO;
	}
	kr = IOServiceOpen(service, mach_task_self(), kIOHIDServerConnectType, &_connect);
	IOObjectRelease(service);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "WindowServer: IOHIDSystem refused its server connection "
		    "(0x%x): no input\n", kr);
		return NO;
	}
	/* Method 0 makes the shared memory; then it is mapped. */
	if (IOConnectCallScalarMethod(_connect, 0, &version, 1, NULL, NULL) != KERN_SUCCESS ||
	    IOConnectMapMemory64(_connect, kIOHIDGlobalMemory, mach_task_self(), &addr, &size,
	    kIOMapAnywhere) != KERN_SUCCESS) {
		fprintf(stderr, "WindowServer: no IOHIDSystem shared memory: no input\n");
		return NO;
	}
	volatile EvOffsets *offsets = (volatile EvOffsets *)(uintptr_t)addr;
	_evg = (volatile EvGlobals *)(uintptr_t)(addr + offsets->evGlobalsOffset);

	mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &_port);
	IOConnectSetNotificationPort(_connect, kIOHIDEventNotification, _port, 0);
	_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, _port, 0, queue);
	__unsafe_unretained WSInput *me = self;	/* lives as long as the server */
	dispatch_source_set_event_handler(_source, ^{ [me readQueue]; });
	dispatch_resume(_source);
	[self publishDisplays];
	fprintf(stderr, "WindowServer: reading input from IOHIDSystem\n");
	/* Events queued before we listened. */
	dispatch_async(queue, ^{ [me readQueue]; });
	return YES;
}

/* Tells IOHIDSystem where each display is on the desktop: the pointer
 * moves over them and only them. Without this it would move over the boot
 * framebuffer alone. */
- (void)publishDisplays
{
	NSArray<WSDisplay *> *displays = WSDisplays;

	if (_screenTokens == nil)
		_screenTokens = [NSMutableArray array];
	while (_screenTokens.count < displays.count) {
		uint64_t token = 0;
		uint32_t n = 1;
		if (IOConnectCallScalarMethod(_connect, kHIDRegisterVirtualDisplay, NULL, 0, &token,
		    &n) != KERN_SUCCESS || token == 0) {
			fprintf(stderr, "WindowServer: IOHIDSystem refused a display\n");
			break;
		}
		[_screenTokens addObject:@(token)];
	}
	while (_screenTokens.count > displays.count) {
		uint64_t token = _screenTokens.lastObject.unsignedLongLongValue;
		IOConnectCallScalarMethod(_connect, kHIDUnregisterVirtualDisplay, &token, 1, NULL, NULL);
		[_screenTokens removeLastObject];
	}
	for (NSUInteger i = 0; i < _screenTokens.count; i++) {
		WSDisplay *d = displays[i];
		/* Token, then min x, max x, min y, max y (16-bit, may be < 0). */
		uint64_t in[5] = {
			_screenTokens[i].unsignedLongLongValue,
			(uint64_t)(int64_t)d.x, (uint64_t)(int64_t)(d.x + d.width),
			(uint64_t)(int64_t)d.y, (uint64_t)(int64_t)(d.y + d.height),
		};
		kern_return_t kr = IOConnectCallScalarMethod(_connect, kHIDSetVirtualDisplayBounds,
		    in, 5, NULL, NULL);
		if (kr != KERN_SUCCESS)
			fprintf(stderr, "WindowServer: IOHIDSystem refused display 0x%x's bounds "
			    "(0x%x)\n", d.displayID, kr);
	}
}

- (void)setLocationX:(double)x y:(double)y
{
	struct mouseLocation loc = { 0, 0, (int32_t)(x * 256), (int32_t)(y * 256), getpid() };

	IOConnectCallStructMethod(_connect, 4, &loc, sizeof(loc), NULL, NULL);
}

/* Empties the port, then reads LLEHead up to LLETail, as IOHIDSystem
 * expects. */
- (void)readQueue
{
	struct {
		mach_msg_header_t header;
		mach_msg_trailer_t trailer;
	} msg;

	while (mach_msg(&msg.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg), _port, 0,
	    MACH_PORT_NULL) == MACH_MSG_SUCCESS)
		;
	while (_evg->LLEHead != _evg->LLETail) {
		volatile NXEQElement *element = &_evg->lleq[_evg->LLEHead];
		[self dispatch:&element->event];
		_evg->LLEHead = element->next;
	}
}

- (void)dispatch:(volatile NXEvent *)nx
{
	SLSEvent e;

	memset(&e, 0, sizeof(e));
	e.timestamp = WSNanoseconds();
	/* The pointer's moves always bring a frame: they measure the
	 * input-to-present latency. */
	if (nx->type == NX_MOUSEMOVED || nx->type == NX_LMOUSEDRAGGED ||
	    nx->type == NX_RMOUSEDRAGGED || nx->type == NX_OMOUSEDRAGGED)
		WSStatsNoteInput(nx->time);
	e.x = nx->location.x;	/* IOHIDSystem's desktop: the main display's pixels */
	e.y = nx->location.y;
	switch (nx->type) {
	case NX_MOUSEMOVED: case NX_LMOUSEDRAGGED: case NX_RMOUSEDRAGGED:
	case NX_OMOUSEDRAGGED:
		e.type = kSLSEventMouseMoved;	/* WSRouteEvent tells drags from moves */
		break;
	case NX_LMOUSEDOWN: case NX_LMOUSEUP: case NX_RMOUSEDOWN: case NX_RMOUSEUP:
	case NX_OMOUSEDOWN: case NX_OMOUSEUP:
		e.type = nx->type;		/* the same numbers as CGEventType's */
		e.buttonNumber = nx->type <= NX_LMOUSEUP ? 0 : nx->type <= NX_RMOUSEUP ? 1 : 2;
		break;
	case NX_SCROLLWHEELMOVED:
		e.type = kSLSEventScrollWheel;
		e.scrollY = nx->data.scrollWheel.deltaAxis1;
		e.scrollX = nx->data.scrollWheel.deltaAxis2;
		break;
	case NX_KEYDOWN: case NX_KEYUP:
		/* The server repeats held keys itself (WSEvents); IOHIDSystem's
		 * own repeats would come on top of them. */
		if (nx->data.key.repeat)
			return;
		if (![self translateKey:nx->data.key.keyCode down:nx->type == NX_KEYDOWN into:&e])
			return;
		break;
	case NX_FLAGSCHANGED: {
		uint16_t keyCode = nx->data.key.keyCode;
		uint32_t mask = modifierMaskForKey(keyCode);
		if (mask == 0 ||
		    ![self translateKey:keyCode down:(nx->flags & mask) != 0 into:&e])
			return;
		break;
	}
	default:
		return;
	}
	if (e.type != kSLSEventKeyDown && e.type != kSLSEventKeyUp &&
	    e.type != kSLSEventFlagsChanged)
		e.flags = _keymap ? WSKeymapModifierFlags(_keymap) : 0;
	WSRouteEvent(&e);
}

- (BOOL)translateKey:(uint16_t)keyCode down:(BOOL)down into:(SLSEvent *)e
{
	WSKeyEvent key;

	if (_keymap == NULL || !WSKeymapTranslate(_keymap, keyCode, down, &key))
		return NO;
	switch (key.kind) {
	case WSKeyDown: e->type = kSLSEventKeyDown; break;
	case WSKeyUp: e->type = kSLSEventKeyUp; break;
	case WSKeyFlagsChanged: e->type = kSLSEventFlagsChanged; break;
	default: return NO;
	}
	e->keyCode = keyCode;
	e->flags = key.modifierFlags;	/* WSModifier* are the CGEventFlags bits */
	memcpy(e->characters, key.characters, sizeof(e->characters));
	memcpy(e->charactersIgnoringModifiers, key.charactersIgnoringModifiers,
	    sizeof(e->charactersIgnoringModifiers));
	return YES;
}

@end

void
WSInputDisplaysChanged(void)
{
	[theInput publishDisplays];
}

void
WSInputSetCursor(double x, double y)
{
	[theInput setLocationX:x y:y];
}

void
WSInputTranslatePosted(SLSEvent *e)
{
	[theInput translateKey:e->keyCode down:e->type == kSLSEventKeyDown into:e];
}

/* The keyboard's layout and repeat, for root or the console's user: a
 * new keymap made on the input queue (the old one goes there too). */
kern_return_t
wss_keyboard_settings_set(mach_port_t server, audit_token_t token, SLSKeyboardSettings settings,
    int32_t *result)
{
	WSConnection *connection = [WSConnection connectionForToken:token];

	(void)server;
	settings.layout[sizeof(settings.layout) - 1] = '\0';
	settings.variant[sizeof(settings.variant) - 1] = '\0';
	if (connection == nil || !WSMayControlInput(connection)) {
		*result = connection == nil ? kSLSErrorInvalidConnection : kSLSErrorCannotComplete;
		return KERN_SUCCESS;
	}
	if (settings.layout[0] == '\0' || settings.initialRepeat < 15 || settings.initialRepeat > 2000 ||
	    settings.repeatInterval < 15 || settings.repeatInterval > 2000) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	WSKeymap *km = WSKeymapCreate(NULL, NULL, NULL, settings.layout, settings.variant, NULL);
	if (km == NULL) {
		*result = kSLSErrorIllegalArgument;
		return KERN_SUCCESS;
	}
	dispatch_sync(WSInputQueue, ^{
		[theInput replaceKeymap:km];
		keyboard = settings;
		WSEventsSetKeyRepeat((uint64_t)settings.initialRepeat * 1000000,
		    (uint64_t)settings.repeatInterval * 1000000);
	});
	NSDictionary *saved = @{ @"Layout": @(settings.layout), @"Variant": @(settings.variant),
				 @"InitialRepeat": @(settings.initialRepeat),
				 @"RepeatInterval": @(settings.repeatInterval) };
	[[NSFileManager defaultManager] createDirectoryAtPath:[kKeyboardPath stringByDeletingLastPathComponent]
	    withIntermediateDirectories:YES attributes:nil error:NULL];
	[saved writeToFile:kKeyboardPath atomically:YES];
	fprintf(stderr, "WindowServer: keyboard layout %s%s%s, repeat %u ms then every %u ms\n",
	    settings.layout, settings.variant[0] ? " " : "", settings.variant,
	    settings.initialRepeat, settings.repeatInterval);
	*result = kSLSErrorSuccess;
	return KERN_SUCCESS;
}

/* A key's characters with modifiers, in the layout in use: from a state of
   their own, on the input queue, where the keymap lives. */
kern_return_t
wss_keyboard_translate(mach_port_t server, uint32_t keyCode, uint32_t modifierFlags,
    SLSKeyCharacters *characters, int32_t *result)
{
	(void)server;
	memset(characters, 0, sizeof(*characters));
	*result = kSLSErrorFailure;
	dispatch_sync(WSInputQueue, ^{
		WSKeymap *km = [theInput keymap];

		if (km != NULL && keyCode < 0x10000 &&
		    WSKeymapCharactersFor(km, (uint16_t)keyCode, modifierFlags,
		    characters->characters, characters->charactersIgnoringModifiers))
			*result = kSLSErrorSuccess;
	});
	return KERN_SUCCESS;
}

kern_return_t
wss_keyboard_settings_get(mach_port_t server, SLSKeyboardSettings *settings)
{
	(void)server;
	dispatch_sync(WSInputQueue, ^{
		*settings = keyboard;
	});
	return KERN_SUCCESS;
}
