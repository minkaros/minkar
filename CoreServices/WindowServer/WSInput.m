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
}

-init {
    self = [super init];
    if(self == nil)
        return nil;
    if(![self setKeyboardLayout:nil variant:nil])
        NSLog(@"No keyboard layout: keys will not be delivered");
    return self;
}

-(void)dealloc {
    WSKeymapDestroy(keymap);
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
    /* No device backend yet. */
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
    return -1;
}

@end
