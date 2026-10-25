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
 * Keyboard translation for WindowServer, in plain C so it can be tested
 * on the build host.
 *
 * Keyboard drivers report macOS virtual key codes (kVK_*, the ADB codes
 * that -[NSEvent keyCode] returns). Layouts come from xkeyboard-config
 * through libxkbcommon, which numbers keys like Linux evdev, so each key
 * is mapped kVK -> evdev -> xkb and the result is expressed the AppKit way:
 * NSEventModifierFlags and the NS*FunctionKey characters.
 */

#ifndef WSKEYMAP_H
#define WSKEYMAP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NSEventModifierFlags values (AppKit ABI). */
enum {
    WSModifierCapsLock   = 1 << 16,
    WSModifierShift      = 1 << 17,
    WSModifierControl    = 1 << 18,
    WSModifierOption     = 1 << 19,
    WSModifierCommand    = 1 << 20,
    WSModifierNumericPad = 1 << 21,
    WSModifierHelp       = 1 << 22,
    WSModifierFunction   = 1 << 23,
};

/* What a key transition means for applications. */
typedef enum {
    WSKeyIgnored,       /* unknown key */
    WSKeyDown,
    WSKeyUp,
    WSKeyFlagsChanged,  /* a modifier key changed state */
} WSKeyEventKind;

typedef struct {
    WSKeyEventKind kind;
    uint16_t keyCode;           /* kVK_* */
    uint32_t modifierFlags;     /* WSModifier* */
    uint32_t keysym;            /* xkb keysym, for shortcuts */
    char characters[8];         /* UTF-8, NUL terminated */
    char charactersIgnoringModifiers[8];
} WSKeyEvent;

typedef struct WSKeymap WSKeymap;

/*
 * Compile a keymap from RMLVO names; NULL or "" picks the defaults
 * (evdev rules, pc105, us). include_path, when not NULL, replaces the
 * default xkeyboard-config location.
 */
WSKeymap *WSKeymapCreate(const char *include_path, const char *rules,
                         const char *model, const char *layout,
                         const char *variant, const char *options);
void WSKeymapDestroy(WSKeymap *keymap);

/* Feed one key transition and describe it. Returns false if ignored. */
bool WSKeymapTranslate(WSKeymap *keymap, uint16_t keyCode, bool down,
                       WSKeyEvent *event);

/* Current modifier flags. */
uint32_t WSKeymapModifierFlags(const WSKeymap *keymap);

/* The evdev key code for a kVK code, or 0 if there is none. */
uint16_t WSKeymapEvdevCode(uint16_t keyCode);

#ifdef __cplusplus
}
#endif

#endif /* WSKEYMAP_H */
