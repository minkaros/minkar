// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// AppKit's events as WPE's. A key's code is the one its NSEvent carries
// (the virtual key codes AppKit uses), given to WPE as the Linux input
// code for the same key plus 8, as XKB numbers keys (WebKit names a key's
// "code" from it). Its symbol comes from the characters AppKit made with
// the user's layout, or from a table for keys that make none.

#import "WebWPEEvents.h"
#include "WebWPEPlatform.h"

/* Virtual key code (0 to 0x7f) to the Linux input code of the same key. */
static const unsigned short linuxCodes[0x80] = {
    [0x00] = 30,  [0x01] = 31,  [0x02] = 32,  [0x03] = 33,  [0x04] = 35,  [0x05] = 34,  [0x06] = 44,  [0x07] = 45,
    [0x08] = 46,  [0x09] = 47,  [0x0A] = 86,  [0x0B] = 48,  [0x0C] = 16,  [0x0D] = 17,  [0x0E] = 18,  [0x0F] = 19,
    [0x10] = 21,  [0x11] = 20,  [0x12] = 2,   [0x13] = 3,   [0x14] = 4,   [0x15] = 5,   [0x16] = 7,   [0x17] = 6,
    [0x18] = 13,  [0x19] = 10,  [0x1A] = 8,   [0x1B] = 12,  [0x1C] = 9,   [0x1D] = 11,  [0x1E] = 27,  [0x1F] = 24,
    [0x20] = 22,  [0x21] = 26,  [0x22] = 23,  [0x23] = 25,  [0x24] = 28,  [0x25] = 38,  [0x26] = 36,  [0x27] = 40,
    [0x28] = 37,  [0x29] = 39,  [0x2A] = 43,  [0x2B] = 51,  [0x2C] = 53,  [0x2D] = 49,  [0x2E] = 50,  [0x2F] = 52,
    [0x30] = 15,  [0x31] = 57,  [0x32] = 41,  [0x33] = 14,  [0x35] = 1,   [0x36] = 126, [0x37] = 125, [0x38] = 42,
    [0x39] = 58,  [0x3A] = 56,  [0x3B] = 29,  [0x3C] = 54,  [0x3D] = 100, [0x3E] = 97,  [0x40] = 187, [0x41] = 83,
    [0x43] = 55,  [0x45] = 78,  [0x47] = 69,  [0x48] = 115, [0x49] = 114, [0x4A] = 113, [0x4B] = 98,  [0x4C] = 96,
    [0x4E] = 74,  [0x4F] = 188, [0x50] = 189, [0x51] = 117, [0x52] = 82,  [0x53] = 79,  [0x54] = 80,  [0x55] = 81,
    [0x56] = 75,  [0x57] = 76,  [0x58] = 77,  [0x59] = 71,  [0x5A] = 190, [0x5B] = 72,  [0x5C] = 73,  [0x5D] = 124,
    [0x5E] = 89,  [0x5F] = 121, [0x60] = 63,  [0x61] = 64,  [0x62] = 65,  [0x63] = 61,  [0x64] = 66,  [0x65] = 67,
    [0x67] = 87,  [0x69] = 183, [0x6A] = 186, [0x6B] = 184, [0x6D] = 68,  [0x6E] = 127, [0x6F] = 88,  [0x71] = 185,
    [0x72] = 110, [0x73] = 102, [0x74] = 104, [0x75] = 111, [0x76] = 62,  [0x77] = 107, [0x78] = 60,  [0x79] = 109,
    [0x7A] = 59,  [0x7B] = 105, [0x7C] = 106, [0x7D] = 108, [0x7E] = 103,
};

/* Key symbols of the keys that make no character, or whose character is
   a control one (X11's numbering, which WPE uses). */
static guint specialKeyval(unsigned short code)
{
    switch (code) {
    case 0x24: return WPE_KEY_Return;
    case 0x4C: return WPE_KEY_KP_Enter;
    case 0x30: return WPE_KEY_Tab;
    case 0x33: return WPE_KEY_BackSpace;
    case 0x35: return WPE_KEY_Escape;
    case 0x75: return WPE_KEY_Delete;
    case 0x72: return WPE_KEY_Insert;
    case 0x73: return WPE_KEY_Home;
    case 0x77: return WPE_KEY_End;
    case 0x74: return WPE_KEY_Page_Up;
    case 0x79: return WPE_KEY_Page_Down;
    case 0x7B: return WPE_KEY_Left;
    case 0x7C: return WPE_KEY_Right;
    case 0x7D: return WPE_KEY_Down;
    case 0x7E: return WPE_KEY_Up;
    case 0x6E: return WPE_KEY_Menu;
    case 0x38: return WPE_KEY_Shift_L;
    case 0x3C: return WPE_KEY_Shift_R;
    case 0x3B: return WPE_KEY_Control_L;
    case 0x3E: return WPE_KEY_Control_R;
    case 0x3A: return WPE_KEY_Alt_L;
    case 0x3D: return WPE_KEY_Alt_R;
    case 0x37: return WPE_KEY_Meta_L;
    case 0x36: return WPE_KEY_Meta_R;
    case 0x39: return WPE_KEY_Caps_Lock;
    case 0x7A: return WPE_KEY_F1;
    case 0x78: return WPE_KEY_F2;
    case 0x63: return WPE_KEY_F3;
    case 0x76: return WPE_KEY_F4;
    case 0x60: return WPE_KEY_F5;
    case 0x61: return WPE_KEY_F6;
    case 0x62: return WPE_KEY_F7;
    case 0x64: return WPE_KEY_F8;
    case 0x65: return WPE_KEY_F9;
    case 0x6D: return WPE_KEY_F10;
    case 0x67: return WPE_KEY_F11;
    case 0x6F: return WPE_KEY_F12;
    case 0x69: return WPE_KEY_F13;
    case 0x6B: return WPE_KEY_F14;
    case 0x71: return WPE_KEY_F15;
    case 0x6A: return WPE_KEY_F16;
    case 0x40: return WPE_KEY_F17;
    case 0x4F: return WPE_KEY_F18;
    case 0x50: return WPE_KEY_F19;
    case 0x5A: return WPE_KEY_F20;
    default: return 0;
    }
}

static WPEModifiers modifiersFrom(NSEventModifierFlags flags)
{
    WPEModifiers modifiers = 0;
    if (flags & NSEventModifierFlagShift)
        modifiers |= WPE_MODIFIER_KEYBOARD_SHIFT;
    if (flags & NSEventModifierFlagControl)
        modifiers |= WPE_MODIFIER_KEYBOARD_CONTROL;
    if (flags & NSEventModifierFlagOption)
        modifiers |= WPE_MODIFIER_KEYBOARD_ALT;
    if (flags & NSEventModifierFlagCommand)
        modifiers |= WPE_MODIFIER_KEYBOARD_META;
    if (flags & NSEventModifierFlagCapsLock)
        modifiers |= WPE_MODIFIER_KEYBOARD_CAPS_LOCK;
    NSUInteger buttons = [NSEvent pressedMouseButtons];
    if (buttons & 1)
        modifiers |= WPE_MODIFIER_POINTER_BUTTON1;
    if (buttons & 2)
        modifiers |= WPE_MODIFIER_POINTER_BUTTON3;
    if (buttons & 4)
        modifiers |= WPE_MODIFIER_POINTER_BUTTON2;
    return modifiers;
}

static guint keycodeFor(unsigned short code)
{
    return code < 0x80 && linuxCodes[code] ? (guint)linuxCodes[code] + 8 : 0;
}

/* The first character of string as a code point (surrogates joined). */
static guint32 firstCodePoint(NSString *string)
{
    if (string.length == 0)
        return 0;
    unichar high = [string characterAtIndex:0];
    if (CFStringIsSurrogateHighCharacter(high) && string.length > 1)
        return (guint32)CFStringGetLongCharacterForSurrogatePair(high, [string characterAtIndex:1]);
    return high;
}

static guint keyvalFor(NSEvent *event)
{
    guint special = specialKeyval(event.keyCode);
    if (special)
        return special;
    /* The characters typed; with Control or Command held, the key's own
       (AppKit gives a control character, or none, with them). */
    guint32 c = firstCodePoint(event.characters);
    if (c < 0x20 || c == 0x7f || (c >= 0xF700 && c <= 0xF8FF) ||
        (event.modifierFlags & (NSEventModifierFlagControl | NSEventModifierFlagCommand)))
        c = firstCodePoint(event.charactersIgnoringModifiers);
    if (c < 0x20 || c == 0x7f || (c >= 0xF700 && c <= 0xF8FF))
        return 0;
    return wpe_unicode_to_keyval(c);
}

static guint32 timeOf(NSEvent *event)
{
    return (guint32)(event.timestamp * 1000.0);
}

WPEEvent *_WebWPEEventCreate(WPEView *view, NSEvent *event, NSPoint point)
{
    WPEModifiers modifiers = modifiersFrom(event.modifierFlags);
    _WebWPESetModifiers(modifiers);
    guint32 time = timeOf(event);

    switch (event.type) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseUp: {
        BOOL down = event.type == NSEventTypeLeftMouseDown || event.type == NSEventTypeRightMouseDown ||
                    event.type == NSEventTypeOtherMouseDown;
        guint button = WPE_BUTTON_PRIMARY;
        if (event.type == NSEventTypeRightMouseDown || event.type == NSEventTypeRightMouseUp)
            button = WPE_BUTTON_SECONDARY;
        else if (event.type == NSEventTypeOtherMouseDown || event.type == NSEventTypeOtherMouseUp)
            button = event.buttonNumber == 2 ? WPE_BUTTON_MIDDLE : (guint)event.buttonNumber + 1;
        return wpe_event_pointer_button_new(down ? WPE_EVENT_POINTER_DOWN : WPE_EVENT_POINTER_UP, view,
                                            WPE_INPUT_SOURCE_MOUSE, time, modifiers, button, point.x, point.y,
                                            down ? (guint)MAX(event.clickCount, 1) : 0);
    }
    case NSEventTypeMouseMoved:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
        return wpe_event_pointer_move_new(WPE_EVENT_POINTER_MOVE, view, WPE_INPUT_SOURCE_MOUSE, time, modifiers,
                                          point.x, point.y, event.deltaX, event.deltaY);
    case NSEventTypeMouseEntered:
        return wpe_event_pointer_move_new(WPE_EVENT_POINTER_ENTER, view, WPE_INPUT_SOURCE_MOUSE, time, modifiers,
                                          point.x, point.y, 0, 0);
    case NSEventTypeMouseExited:
        return wpe_event_pointer_move_new(WPE_EVENT_POINTER_LEAVE, view, WPE_INPUT_SOURCE_MOUSE, time, modifiers,
                                          point.x, point.y, 0, 0);
    case NSEventTypeScrollWheel: {
        /* WebKit takes AppKit's sense (positive: towards the top or the
           left). Without precise deltas, one is a line. */
        BOOL precise = event.hasPreciseScrollingDeltas;
        BOOL stop = (event.phase & (NSEventPhaseEnded | NSEventPhaseCancelled)) != 0;
        return wpe_event_scroll_new(view, precise ? WPE_INPUT_SOURCE_TOUCHPAD : WPE_INPUT_SOURCE_MOUSE, time,
                                    modifiers, event.scrollingDeltaX, event.scrollingDeltaY, precise, stop,
                                    point.x, point.y);
    }
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
        return wpe_event_keyboard_new(event.type == NSEventTypeKeyDown ? WPE_EVENT_KEYBOARD_KEY_DOWN :
                                      WPE_EVENT_KEYBOARD_KEY_UP, view, WPE_INPUT_SOURCE_KEYBOARD, time, modifiers,
                                      keycodeFor(event.keyCode), keyvalFor(event));
    default:
        return NULL;
    }
}

WPEEvent *_WebWPEModifierEventCreate(WPEView *view, NSEvent *event)
{
    NSEventModifierFlags flag;
    switch (event.keyCode) {
    case 0x38: case 0x3C: flag = NSEventModifierFlagShift; break;
    case 0x3B: case 0x3E: flag = NSEventModifierFlagControl; break;
    case 0x3A: case 0x3D: flag = NSEventModifierFlagOption; break;
    case 0x37: case 0x36: flag = NSEventModifierFlagCommand; break;
    case 0x39: flag = NSEventModifierFlagCapsLock; break;
    default: return NULL;
    }
    WPEModifiers modifiers = modifiersFrom(event.modifierFlags);
    _WebWPESetModifiers(modifiers);
    BOOL down = (event.modifierFlags & flag) != 0;
    return wpe_event_keyboard_new(down ? WPE_EVENT_KEYBOARD_KEY_DOWN : WPE_EVENT_KEYBOARD_KEY_UP, view,
                                  WPE_INPUT_SOURCE_KEYBOARD, timeOf(event), modifiers, keycodeFor(event.keyCode),
                                  specialKeyval(event.keyCode));
}
