/*
 * Copyright (C) 2022-2024 Zoe Knox <zoe@pixin.net>
 * Modifications Copyright (c) 2026 The Minkar contributors
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <mach/mach.h>
#include <sys/event.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hidsystem/IOLLEvent.h>
#import "common.h"
#import "message.h"
#import "WSInput.h"
#import "WSKeymap.h"

double clipTo(double val, double min, double max) {
    if(val < min) val = min;
    else if(val > max) val = max;
    return val;
}

_Static_assert(WSModifierCapsLock == NSAlphaShiftKeyMask, "modifier flags");
_Static_assert(WSModifierShift == NSShiftKeyMask, "modifier flags");
_Static_assert(WSModifierControl == NSControlKeyMask, "modifier flags");
_Static_assert(WSModifierOption == NSAlternateKeyMask, "modifier flags");
_Static_assert(WSModifierCommand == NSCommandKeyMask, "modifier flags");
_Static_assert(WSModifierNumericPad == NSNumericPadKeyMask, "modifier flags");
_Static_assert(WSModifierFunction == NSFunctionKeyMask, "modifier flags");

enum { kVK_Tab = 0x30 };

@implementation WSInput {
    int logLevel;
    unsigned int modifierFlags;
    WSKeymap *keymap;

    NSRect geometry; // size of the active screen
    double pointerX;
    double pointerY;
    BOOL buttonDown[WSInputButtonCount];

    // IOHIDSystem's event server connection: its shared low-level event
    // queue, the port it tells when events arrive, and a kqueue on that
    // port so the run loop can poll() for it.
    io_connect_t hidConnect;
    volatile EvGlobals *evg;
    mach_port_t hidPort;
    int hidQueue;
}

static unsigned int modifierMaskForKey(uint16_t keyCode) {
    switch(keyCode) {
        case 0x38: case 0x3C: return NX_SHIFTMASK;
        case 0x3B: case 0x3E: return NX_CONTROLMASK;
        case 0x3A: case 0x3D: return NX_ALTERNATEMASK;
        case 0x37: case 0x36: return NX_COMMANDMASK;
        case 0x39: return NX_ALPHASHIFTMASK;
        case 0x3F: return NX_SECONDARYFNMASK;
    }
    return 0;
}

-(BOOL)openHIDSystem {
    hidQueue = -1;
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault,
        IOServiceMatching(kIOHIDSystemClass));
    if(!service) {
        NSLog(@"No IOHIDSystem: no input");
        return NO;
    }
    kern_return_t kr = IOServiceOpen(service, mach_task_self(), kIOHIDServerConnectType,
        &hidConnect);
    IOObjectRelease(service);
    if(kr != KERN_SUCCESS) {
        NSLog(@"IOHIDSystem server connection refused (0x%x): no input", kr);
        hidConnect = IO_OBJECT_NULL;
        return NO;
    }

    uint64_t version = kIOHIDCurrentShmemVersion;
    mach_vm_address_t addr = 0;
    mach_vm_size_t size = 0;
    if(IOConnectCallScalarMethod(hidConnect, 0, &version, 1, NULL, NULL) != KERN_SUCCESS ||
        IOConnectMapMemory64(hidConnect, kIOHIDGlobalMemory, mach_task_self(), &addr, &size,
            kIOMapAnywhere) != KERN_SUCCESS) {
        NSLog(@"IOHIDSystem shared memory not available: no input");
        IOServiceClose(hidConnect);
        hidConnect = IO_OBJECT_NULL;
        return NO;
    }
    volatile EvOffsets *eop = (volatile EvOffsets *)(uintptr_t)addr;
    evg = (volatile EvGlobals *)(uintptr_t)(addr + eop->evGlobalsOffset);

    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &hidPort);
    IOConnectSetNotificationPort(hidConnect, kIOHIDEventNotification, hidPort, 0);

    hidQueue = kqueue();
    struct kevent64_s ev;
    EV_SET64(&ev, hidPort, EVFILT_MACHPORT, EV_ADD, 0, 0, 0, 0, 0);
    if(hidQueue < 0 || kevent64(hidQueue, &ev, 1, NULL, 0, 0, NULL) < 0) {
        NSLog(@"No kqueue for the input port: input only when the screen is redrawn");
        if(hidQueue >= 0)
            close(hidQueue);
        hidQueue = -1;
    }
    if(logLevel >= WS_INFO)
        NSLog(@"Input: IOHIDSystem event queue mapped (%llu bytes)", (unsigned long long)size);
    return YES;
}

// One low-level event (IOHIDSystem's NXEvent) for the backend calls below.
// Locations are desktop coordinates from the top left; ours grow upwards.
-(void)dispatchLowLevelEvent:(volatile NXEvent *)e target:(NSObject *)target {
    double x = e->location.x;
    double y = geometry.size.height - e->location.y;

    switch(e->type) {
        case NX_MOUSEMOVED:
        case NX_LMOUSEDRAGGED:
        case NX_RMOUSEDRAGGED:
        case NX_OMOUSEDRAGGED:
            [self pointerMovedToX:x y:y target:target];
            break;
        case NX_LMOUSEDOWN: case NX_LMOUSEUP:
        case NX_RMOUSEDOWN: case NX_RMOUSEUP:
        case NX_OMOUSEDOWN: case NX_OMOUSEUP: {
            int button = e->data.mouse.buttonNumber;
            BOOL down = (e->type == NX_LMOUSEDOWN || e->type == NX_RMOUSEDOWN ||
                e->type == NX_OMOUSEDOWN);
            if(x != pointerX || y != pointerY)
                [self pointerMovedToX:x y:y target:target];
            if(button < WSInputButtonCount)
                [self button:button pressed:down target:target];
            break;
        }
        case NX_KEYDOWN:
        case NX_KEYUP:
            [self key:e->data.key.keyCode pressed:(e->type == NX_KEYDOWN) target:target];
            break;
        case NX_FLAGSCHANGED: {
            uint16_t keyCode = e->data.key.keyCode;
            unsigned int mask = modifierMaskForKey(keyCode);
            if(mask != 0)
                [self key:keyCode pressed:((e->flags & mask) != 0) target:target];
            break;
        }
        default:
            break;  // FIXME: scroll wheel events (WS3)
    }
}

-init {
    self = [super init];
    if(self == nil)
        return nil;
    if(![self setKeyboardLayout:nil variant:nil])
        NSLog(@"No keyboard layout: keys will not be delivered");
    [self openHIDSystem];
    return self;
}

-(void)dealloc {
    WSKeymapDestroy(keymap);
    if(hidQueue >= 0)
        close(hidQueue);
    if(hidConnect != IO_OBJECT_NULL)
        IOServiceClose(hidConnect);
    if(hidPort != MACH_PORT_NULL)
        mach_port_mod_refs(mach_task_self(), hidPort, MACH_PORT_RIGHT_RECEIVE, -1);
}

-(BOOL)setKeyboardLayout:(NSString *)layout variant:(NSString *)variant {
    WSKeymap *newKeymap = WSKeymapCreate(NULL, NULL, NULL, [layout UTF8String],
        [variant UTF8String], NULL);
    if(newKeymap == NULL)
        return NO;
    WSKeymapDestroy(keymap);
    keymap = newKeymap;
    modifierFlags = WSKeymapModifierFlags(keymap);
    return YES;
}

-(void)key:(uint16_t)keyCode pressed:(BOOL)pressed target:(NSObject *)target {
    WSKeyEvent key;
    if(keymap == NULL || !WSKeymapTranslate(keymap, keyCode, pressed, &key))
        return;
    modifierFlags = key.modifierFlags;

    const unsigned int shortcutMask = NSShiftKeyMask | NSControlKeyMask |
        NSAlternateKeyMask | NSCommandKeyMask;
    unsigned int mods = modifierFlags & shortcutMask;

    // Shortcuts reserved for WindowServer itself
    if(key.kind == WSKeyDown && mods == (NSCommandKeyMask | NSShiftKeyMask) &&
        strcmp(key.charactersIgnoringModifiers, "Q") == 0) { // FIXME: remove
        [target signalQuit];
        return;
    }
    if(mods == NSCommandKeyMask && keyCode == kVK_Tab) {
        if(key.kind == WSKeyDown)
            [target switchApp];
        return;
    }

    struct mach_event me;
    memset(&me, 0, sizeof(me));
    switch(key.kind) {
        case WSKeyDown: me.code = NSKeyDown; break;
        case WSKeyUp: me.code = NSKeyUp; break;
        case WSKeyFlagsChanged: me.code = NSFlagsChanged; break;
        default: return;
    }
    me.state = me.code;
    me.keycode = keyCode;
    me.mods = modifierFlags;
    me.x = pointerX;
    me.y = pointerY;
    _Static_assert(sizeof(me.chars) == sizeof(key.characters), "chars size");
    memcpy(me.chars, key.characters, sizeof(me.chars));
    memcpy(me.charsIg, key.charactersIgnoringModifiers, sizeof(me.charsIg));
    me.repeat = 0; // FIXME: key repeat
    if(logLevel >= WS_INFO)
        NSLog(@"Input event: type=KEY code=0x%02x down=%d mods=0x%x", keyCode, pressed,
            modifierFlags);
    [target sendEventToApp:&me];
}

-(void)run:(NSObject *)target {
    if(evg == NULL)
        return;
    // Empty the port: one message says the queue has events.
    struct {
        mach_msg_header_t header;
        mach_msg_trailer_t trailer;
    } msg;
    while(mach_msg(&msg.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg), hidPort, 0,
        MACH_PORT_NULL) == MACH_MSG_SUCCESS)
        ;
    // Read from LLEHead up to LLETail, as IOHIDSystem expects.
    while(evg->LLEHead != evg->LLETail) {
        volatile NXEQElement *element = &evg->lleq[evg->LLEHead];
        [self dispatchLowLevelEvent:&element->event target:target];
        evg->LLEHead = element->next;
    }
}

-(void)setLogLevel:(int)level {
    logLevel = level;
}

-(void)setDebugLevel:(int)level {
    logLevel = level;
}

// Call this whenever the cursor moves to a new screen
-(void)setGeometry:(NSRect)geom {
    geometry = geom;
}

-(uint32_t)dragOrMoveCode {
    if(buttonDown[WSInputButtonLeft])
        return NSLeftMouseDragged;
    if(buttonDown[WSInputButtonRight])
        return NSRightMouseDragged;
    return NSMouseMoved;
}

-(void)pointerMovedByX:(double)dx y:(double)dy target:(NSObject *)target {
    struct mach_event me;
    memset(&me, 0, sizeof(me));
    me.code = [self dragOrMoveCode];
    me.dx = dx;
    me.dy = dy;
    me.mods = modifierFlags;
    pointerX = clipTo(pointerX + dx, geometry.origin.x, geometry.size.width);
    pointerY = clipTo(pointerY + dy, geometry.origin.y, geometry.size.height);
    me.x = pointerX;
    me.y = pointerY;
    if(logLevel >= WS_INFO)
        NSLog(@"Input event: type=MOTION dx=%.2f dy=%.2f pos=%.2f,%.2f", dx, dy,
                pointerX, pointerY);
    [target sendEventToApp:&me];
}

-(void)pointerMovedToX:(double)x y:(double)y target:(NSObject *)target {
    struct mach_event me;
    memset(&me, 0, sizeof(me));
    me.code = [self dragOrMoveCode];
    me.mods = modifierFlags;
    me.dx = x - pointerX;
    me.dy = y - pointerY;
    pointerX = clipTo(x, geometry.origin.x, geometry.size.width);
    pointerY = clipTo(y, geometry.origin.y, geometry.size.height);
    me.x = pointerX;
    me.y = pointerY;
    if(logLevel >= WS_INFO)
        NSLog(@"Input event: type=MOTION x=%.2f y=%.2f", pointerX, pointerY);
    [target sendEventToApp:&me];
}

-(void)button:(int)button pressed:(BOOL)pressed target:(NSObject *)target {
    struct mach_event me;
    memset(&me, 0, sizeof(me));
    switch(button) {
        case WSInputButtonLeft:
            me.code = pressed ? NSLeftMouseDown : NSLeftMouseUp;
            break;
        case WSInputButtonRight:
            me.code = pressed ? NSRightMouseDown : NSRightMouseUp;
            break;
        default:
            return;
    }
    if(logLevel >= WS_INFO)
        NSLog(@"Input event: type=BUTTON button=%d state=%d", button, pressed);
    buttonDown[button] = pressed;
    me.mods = modifierFlags;
    me.x = pointerX;
    me.y = pointerY;
    [target sendEventToApp:&me];
}

-(NSPoint)pointerPos {
    return NSMakePoint(pointerX, pointerY);
}

-(NSPoint)setPointerPos:(NSPoint)pos {
    pointerX = pos.x;
    pointerY = pos.y;
    return pos;
}

-(int)fileDescriptor {
    return hidQueue;
}

@end
