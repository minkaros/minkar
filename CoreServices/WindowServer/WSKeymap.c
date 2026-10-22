/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The ravynOS contributors
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

#include <stdlib.h>
#include <string.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "WSKeymap.h"

/* Linux evdev key codes used below (input-event-codes.h numbering). */
enum {
    EV_ESC = 1, EV_1, EV_2, EV_3, EV_4, EV_5, EV_6, EV_7, EV_8, EV_9, EV_0,
    EV_MINUS, EV_EQUAL, EV_BACKSPACE, EV_TAB,
    EV_Q, EV_W, EV_E, EV_R, EV_T, EV_Y, EV_U, EV_I, EV_O, EV_P,
    EV_LEFTBRACE, EV_RIGHTBRACE, EV_ENTER, EV_LEFTCTRL,
    EV_A, EV_S, EV_D, EV_F, EV_G, EV_H, EV_J, EV_K, EV_L,
    EV_SEMICOLON, EV_APOSTROPHE, EV_GRAVE, EV_LEFTSHIFT, EV_BACKSLASH,
    EV_Z, EV_X, EV_C, EV_V, EV_B, EV_N, EV_M,
    EV_COMMA, EV_DOT, EV_SLASH, EV_RIGHTSHIFT, EV_KPASTERISK, EV_LEFTALT,
    EV_SPACE, EV_CAPSLOCK,
    EV_F1, EV_F2, EV_F3, EV_F4, EV_F5, EV_F6, EV_F7, EV_F8, EV_F9, EV_F10,
    EV_NUMLOCK, EV_SCROLLLOCK,
    EV_KP7, EV_KP8, EV_KP9, EV_KPMINUS, EV_KP4, EV_KP5, EV_KP6, EV_KPPLUS,
    EV_KP1, EV_KP2, EV_KP3, EV_KP0, EV_KPDOT,
    EV_102ND = 86, EV_F11, EV_F12, EV_RO,
    EV_KPENTER = 96, EV_RIGHTCTRL, EV_KPSLASH, EV_SYSRQ, EV_RIGHTALT,
    EV_HOME = 102, EV_UP, EV_PAGEUP, EV_LEFT, EV_RIGHT, EV_END, EV_DOWN,
    EV_PAGEDOWN, EV_INSERT, EV_DELETE,
    EV_MUTE = 113, EV_VOLUMEDOWN, EV_VOLUMEUP, EV_POWER, EV_KPEQUAL,
    EV_KPJPCOMMA = 95,
    EV_HANGEUL = 122, EV_HANJA, EV_YEN, EV_LEFTMETA, EV_RIGHTMETA, EV_COMPOSE,
    EV_F13 = 183, EV_F14, EV_F15, EV_F16, EV_F17, EV_F18, EV_F19, EV_F20,
};

/* kVK_* -> evdev. 0 means no equivalent. */
static const uint16_t kvkToEvdev[128] = {
    [0x00] = EV_A, [0x01] = EV_S, [0x02] = EV_D, [0x03] = EV_F,
    [0x04] = EV_H, [0x05] = EV_G, [0x06] = EV_Z, [0x07] = EV_X,
    [0x08] = EV_C, [0x09] = EV_V, [0x0A] = EV_102ND /* ISO section */,
    [0x0B] = EV_B, [0x0C] = EV_Q, [0x0D] = EV_W, [0x0E] = EV_E,
    [0x0F] = EV_R, [0x10] = EV_Y, [0x11] = EV_T, [0x12] = EV_1,
    [0x13] = EV_2, [0x14] = EV_3, [0x15] = EV_4, [0x16] = EV_6,
    [0x17] = EV_5, [0x18] = EV_EQUAL, [0x19] = EV_9, [0x1A] = EV_7,
    [0x1B] = EV_MINUS, [0x1C] = EV_8, [0x1D] = EV_0,
    [0x1E] = EV_RIGHTBRACE, [0x1F] = EV_O, [0x20] = EV_U,
    [0x21] = EV_LEFTBRACE, [0x22] = EV_I, [0x23] = EV_P,
    [0x24] = EV_ENTER, [0x25] = EV_L, [0x26] = EV_J,
    [0x27] = EV_APOSTROPHE, [0x28] = EV_K, [0x29] = EV_SEMICOLON,
    [0x2A] = EV_BACKSLASH, [0x2B] = EV_COMMA, [0x2C] = EV_SLASH,
    [0x2D] = EV_N, [0x2E] = EV_M, [0x2F] = EV_DOT, [0x30] = EV_TAB,
    [0x31] = EV_SPACE, [0x32] = EV_GRAVE, [0x33] = EV_BACKSPACE,
    [0x35] = EV_ESC,
    [0x36] = EV_RIGHTMETA,  /* right Command */
    [0x37] = EV_LEFTMETA,   /* Command */
    [0x38] = EV_LEFTSHIFT, [0x39] = EV_CAPSLOCK,
    [0x3A] = EV_LEFTALT,    /* Option */
    [0x3B] = EV_LEFTCTRL, [0x3C] = EV_RIGHTSHIFT,
    [0x3D] = EV_RIGHTALT,   /* right Option */
    [0x3E] = EV_RIGHTCTRL,
    /* 0x3F Function has no xkb key; handled separately. */
    [0x40] = EV_F17, [0x41] = EV_KPDOT, [0x43] = EV_KPASTERISK,
    [0x45] = EV_KPPLUS,
    /* 0x47 keypad Clear is handled separately: it is not Num Lock. */
    [0x48] = EV_VOLUMEUP, [0x49] = EV_VOLUMEDOWN, [0x4A] = EV_MUTE,
    [0x4B] = EV_KPSLASH, [0x4C] = EV_KPENTER, [0x4E] = EV_KPMINUS,
    [0x4F] = EV_F18, [0x50] = EV_F19, [0x51] = EV_KPEQUAL,
    [0x52] = EV_KP0, [0x53] = EV_KP1, [0x54] = EV_KP2, [0x55] = EV_KP3,
    [0x56] = EV_KP4, [0x57] = EV_KP5, [0x58] = EV_KP6, [0x59] = EV_KP7,
    [0x5A] = EV_F20, [0x5B] = EV_KP8, [0x5C] = EV_KP9,
    [0x5D] = EV_YEN, [0x5E] = EV_RO, [0x5F] = EV_KPJPCOMMA,
    [0x60] = EV_F5, [0x61] = EV_F6, [0x62] = EV_F7, [0x63] = EV_F3,
    [0x64] = EV_F8, [0x65] = EV_F9,
    [0x66] = EV_HANJA,      /* JIS Eisu */
    [0x67] = EV_F11,
    [0x68] = EV_HANGEUL,    /* JIS Kana */
    [0x69] = EV_F13, [0x6A] = EV_F16, [0x6B] = EV_F14, [0x6D] = EV_F10,
    [0x6E] = EV_COMPOSE,    /* contextual menu */
    [0x6F] = EV_F12, [0x71] = EV_F15,
    [0x72] = EV_INSERT,     /* Help */
    [0x73] = EV_HOME, [0x74] = EV_PAGEUP, [0x75] = EV_DELETE,
    [0x76] = EV_F4, [0x77] = EV_END, [0x78] = EV_F2, [0x79] = EV_PAGEDOWN,
    [0x7A] = EV_F1, [0x7B] = EV_LEFT, [0x7C] = EV_RIGHT, [0x7D] = EV_DOWN,
    [0x7E] = EV_UP,
};

#define kVKFunction 0x3F
#define kVKKeypadClear 0x47

/* AppKit function key characters (NS*FunctionKey). */
enum {
    NSUpArrow = 0xF700, NSDownArrow, NSLeftArrow, NSRightArrow,
    NSF1 = 0xF704,
    NSInsert = 0xF727, NSDelete, NSHome, NSBegin, NSEnd, NSPageUp, NSPageDown,
    NSPrintScreen, NSScrollLock, NSPause, NSSysReq, NSBreak,
    NSMenu = 0xF735, NSClearLine = 0xF739, NSHelp = 0xF746,
};

/* Characters macOS uses where xkb returns ASCII control codes or nothing. */
static uint32_t macCharacterForKeysym(xkb_keysym_t sym) {
    if (sym >= XKB_KEY_F1 && sym <= XKB_KEY_F35)
        return NSF1 + (sym - XKB_KEY_F1);
    switch (sym) {
    case XKB_KEY_Up: case XKB_KEY_KP_Up: return NSUpArrow;
    case XKB_KEY_Down: case XKB_KEY_KP_Down: return NSDownArrow;
    case XKB_KEY_Left: case XKB_KEY_KP_Left: return NSLeftArrow;
    case XKB_KEY_Right: case XKB_KEY_KP_Right: return NSRightArrow;
    case XKB_KEY_Home: case XKB_KEY_KP_Home: return NSHome;
    case XKB_KEY_End: case XKB_KEY_KP_End: return NSEnd;
    case XKB_KEY_Begin: case XKB_KEY_KP_Begin: return NSBegin;
    case XKB_KEY_Page_Up: case XKB_KEY_KP_Page_Up: return NSPageUp;
    case XKB_KEY_Page_Down: case XKB_KEY_KP_Page_Down: return NSPageDown;
    case XKB_KEY_Delete: case XKB_KEY_KP_Delete: return NSDelete;
    case XKB_KEY_Insert: case XKB_KEY_KP_Insert: return NSHelp;  /* kVK_Help */
    case XKB_KEY_Print: return NSPrintScreen;
    case XKB_KEY_Scroll_Lock: return NSScrollLock;
    case XKB_KEY_Pause: return NSPause;
    case XKB_KEY_Menu: return NSMenu;
    case XKB_KEY_BackSpace: return 0x7F;        /* NSDeleteCharacter */
    case XKB_KEY_KP_Enter: return 0x03;         /* NSEnterCharacter */
    case XKB_KEY_ISO_Left_Tab: return 0x19;     /* NSBackTabCharacter */
    default: return 0;
    }
}

static void encodeUTF8(uint32_t c, char out[8]) {
    memset(out, 0, 8);
    if (c < 0x80) {
        out[0] = (char)c;
    } else if (c < 0x800) {
        out[0] = (char)(0xC0 | (c >> 6));
        out[1] = (char)(0x80 | (c & 0x3F));
    } else {
        out[0] = (char)(0xE0 | (c >> 12));
        out[1] = (char)(0x80 | ((c >> 6) & 0x3F));
        out[2] = (char)(0x80 | (c & 0x3F));
    }
}

static bool isModifierKey(uint16_t keyCode) {
    return keyCode >= 0x36 && keyCode <= 0x3F;
}

static bool isKeypadKey(uint16_t keyCode) {
    switch (keyCode) {
    case 0x41: case 0x43: case 0x45: case 0x47: case 0x4B: case 0x4C:
    case 0x4E: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55:
    case 0x56: case 0x57: case 0x58: case 0x59: case 0x5B: case 0x5C:
    case 0x7B: case 0x7C: case 0x7D: case 0x7E:    /* arrows, as on macOS */
        return true;
    default:
        return false;
    }
}

struct WSKeymap {
    struct xkb_context *context;
    struct xkb_keymap *keymap;
    struct xkb_state *state;
    struct xkb_state *shiftOnly;   /* for charactersIgnoringModifiers */
    xkb_mod_mask_t shiftMask;
    xkb_mod_mask_t numLockMask;
    bool functionDown;
};

uint16_t WSKeymapEvdevCode(uint16_t keyCode) {
    return keyCode < 128 ? kvkToEvdev[keyCode] : 0;
}

static const char *nonEmpty(const char *s) {
    return (s != NULL && *s != '\0') ? s : NULL;
}

WSKeymap *WSKeymapCreate(const char *include_path, const char *rules,
                         const char *model, const char *layout,
                         const char *variant, const char *options) {
    WSKeymap *km = calloc(1, sizeof(*km));
    if (km == NULL)
        return NULL;

    enum xkb_context_flags flags = XKB_CONTEXT_NO_ENVIRONMENT_NAMES;
    if (include_path != NULL)
        flags |= XKB_CONTEXT_NO_DEFAULT_INCLUDES;
    km->context = xkb_context_new(flags);
    if (km->context == NULL)
        goto fail;
    if (include_path != NULL && !xkb_context_include_path_append(km->context, include_path))
        goto fail;

    struct xkb_rule_names names = {
        .rules = nonEmpty(rules), .model = nonEmpty(model),
        .layout = nonEmpty(layout), .variant = nonEmpty(variant),
        .options = nonEmpty(options),
    };
    km->keymap = xkb_keymap_new_from_names(km->context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (km->keymap == NULL)
        goto fail;
    km->state = xkb_state_new(km->keymap);
    km->shiftOnly = xkb_state_new(km->keymap);
    if (km->state == NULL || km->shiftOnly == NULL)
        goto fail;

    xkb_mod_index_t shift = xkb_keymap_mod_get_index(km->keymap, XKB_MOD_NAME_SHIFT);
    km->shiftMask = (shift == XKB_MOD_INVALID) ? 0 : (1u << shift);

    /* Mac keypads always type digits: keep Num Lock on. */
    xkb_mod_index_t num = xkb_keymap_mod_get_index(km->keymap, XKB_MOD_NAME_NUM);
    km->numLockMask = (num == XKB_MOD_INVALID) ? 0 : (1u << num);
    xkb_state_update_mask(km->state, 0, 0, km->numLockMask, 0, 0, 0);
    return km;

fail:
    WSKeymapDestroy(km);
    return NULL;
}

void WSKeymapDestroy(WSKeymap *km) {
    if (km == NULL)
        return;
    xkb_state_unref(km->shiftOnly);
    xkb_state_unref(km->state);
    xkb_keymap_unref(km->keymap);
    xkb_context_unref(km->context);
    free(km);
}

uint32_t WSKeymapModifierFlags(const WSKeymap *km) {
    static const struct { const char *name; enum xkb_state_component kind; uint32_t flag; } mods[] = {
        { XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE, WSModifierShift },
        { XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE, WSModifierControl },
        { XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE, WSModifierOption },
        { XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE, WSModifierCommand },
        { XKB_MOD_NAME_CAPS, XKB_STATE_MODS_LOCKED, WSModifierCapsLock },
    };
    uint32_t flags = km->functionDown ? WSModifierFunction : 0;
    for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
        if (xkb_state_mod_name_is_active(km->state, mods[i].name, mods[i].kind) > 0)
            flags |= mods[i].flag;
    }
    return flags;
}

/* Characters for a key: the Mac character when there is one, else xkb's. */
static void charactersForKey(struct xkb_state *state, xkb_keycode_t key, char out[8]) {
    xkb_keysym_t sym = xkb_state_key_get_one_sym(state, key);
    uint32_t mac = macCharacterForKeysym(sym);
    if (mac != 0) {
        encodeUTF8(mac, out);
        return;
    }
    memset(out, 0, 8);
    xkb_state_key_get_utf8(state, key, out, 8);
}

bool WSKeymapTranslate(WSKeymap *km, uint16_t keyCode, bool down, WSKeyEvent *ev) {
    memset(ev, 0, sizeof(*ev));
    ev->keyCode = keyCode;

    if (keyCode == kVKFunction) {
        km->functionDown = down;
        ev->kind = WSKeyFlagsChanged;
        ev->modifierFlags = WSKeymapModifierFlags(km);
        return true;
    }

    if (keyCode == kVKKeypadClear) {
        ev->kind = down ? WSKeyDown : WSKeyUp;
        encodeUTF8(NSClearLine, ev->characters);
        encodeUTF8(NSClearLine, ev->charactersIgnoringModifiers);
        ev->modifierFlags = WSKeymapModifierFlags(km) | WSModifierNumericPad | WSModifierFunction;
        return true;
    }

    uint16_t evdev = WSKeymapEvdevCode(keyCode);
    if (evdev == 0) {
        ev->kind = WSKeyIgnored;
        return false;
    }
    xkb_keycode_t key = evdev + 8;

    /* Characters come from the state before this transition. */
    if (!isModifierKey(keyCode)) {
        ev->keysym = xkb_state_key_get_one_sym(km->state, key);
        charactersForKey(km->state, key, ev->characters);

        xkb_mod_mask_t depressed = xkb_state_serialize_mods(km->state, XKB_STATE_MODS_DEPRESSED);
        xkb_layout_index_t group = xkb_state_serialize_layout(km->state, XKB_STATE_LAYOUT_EFFECTIVE);
        xkb_state_update_mask(km->shiftOnly, depressed & km->shiftMask, 0,
                              km->numLockMask, 0, 0, group);
        charactersForKey(km->shiftOnly, key, ev->charactersIgnoringModifiers);
    }

    xkb_state_update_key(km->state, key, down ? XKB_KEY_DOWN : XKB_KEY_UP);

    ev->modifierFlags = WSKeymapModifierFlags(km);
    if (isModifierKey(keyCode)) {
        ev->kind = WSKeyFlagsChanged;
        return true;
    }

    ev->kind = down ? WSKeyDown : WSKeyUp;
    if (isKeypadKey(keyCode))
        ev->modifierFlags |= WSModifierNumericPad;
    /* Function keys (arrows, F-keys, Home, ...) carry the Function flag. */
    if ((unsigned char)ev->characters[0] == 0xEF &&
        ((unsigned char)ev->characters[1] & 0xFC) == 0x9C)
        ev->modifierFlags |= WSModifierFunction;
    return true;
}
