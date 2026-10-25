/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
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

/* Host tests for WSKeymap: kVK codes -> AppKit key events. */

#include <stdio.h>
#include <string.h>

#include "WSKeymap.h"

/* kVK_* codes used here. */
enum {
    kVK_ANSI_A = 0x00, kVK_ANSI_Y = 0x10, kVK_ANSI_Z = 0x06,
    kVK_ANSI_Semicolon = 0x29, kVK_ANSI_2 = 0x13, kVK_ANSI_Keypad1 = 0x53,
    kVK_ANSI_KeypadEnter = 0x4C, kVK_ANSI_KeypadClear = 0x47,
    kVK_Return = 0x24, kVK_Tab = 0x30, kVK_Delete = 0x33, kVK_Escape = 0x35,
    kVK_Command = 0x37, kVK_Shift = 0x38, kVK_CapsLock = 0x39,
    kVK_Option = 0x3A, kVK_Control = 0x3B, kVK_Function = 0x3F,
    kVK_F1 = 0x7A, kVK_F12 = 0x6F, kVK_ForwardDelete = 0x75,
    kVK_Help = 0x72, kVK_UpArrow = 0x7E, kVK_LeftArrow = 0x7B,
};

static int checks, failures;
static const char *xkbRoot;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { failures++; fprintf(stderr, "%s:%d: FAIL %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

static WSKeymap *keymap(const char *layout, const char *variant) {
    WSKeymap *km = WSKeymapCreate(xkbRoot, NULL, NULL, layout, variant, NULL);
    if (km == NULL)
        fprintf(stderr, "cannot compile keymap %s(%s) from %s\n", layout,
                variant ? variant : "", xkbRoot);
    return km;
}

static WSKeyEvent press(WSKeymap *km, uint16_t key) {
    WSKeyEvent ev;
    WSKeymapTranslate(km, key, true, &ev);
    return ev;
}

static WSKeyEvent release(WSKeymap *km, uint16_t key) {
    WSKeyEvent ev;
    WSKeymapTranslate(km, key, false, &ev);
    return ev;
}

/* Press and release; return the key down event. */
static WSKeyEvent tap(WSKeymap *km, uint16_t key) {
    WSKeyEvent ev = press(km, key);
    release(km, key);
    return ev;
}

static void test_us_basic(void) {
    WSKeymap *km = keymap("us", NULL);
    CHECK(km != NULL);
    if (km == NULL)
        return;

    WSKeyEvent ev = press(km, kVK_ANSI_A);
    CHECK(ev.kind == WSKeyDown);
    CHECK(ev.keyCode == kVK_ANSI_A);
    CHECK(strcmp(ev.characters, "a") == 0);
    CHECK(strcmp(ev.charactersIgnoringModifiers, "a") == 0);
    CHECK(ev.modifierFlags == 0);
    ev = release(km, kVK_ANSI_A);
    CHECK(ev.kind == WSKeyUp);
    CHECK(strcmp(ev.characters, "a") == 0);

    CHECK(strcmp(tap(km, kVK_ANSI_2).characters, "2") == 0);
    CHECK(strcmp(tap(km, kVK_Return).characters, "\r") == 0);
    CHECK(strcmp(tap(km, kVK_Tab).characters, "\t") == 0);
    CHECK(strcmp(tap(km, kVK_Escape).characters, "\x1b") == 0);
    CHECK(strcmp(tap(km, kVK_Delete).characters, "\x7f") == 0);      /* NSDeleteCharacter */

    /* Unknown codes are ignored. */
    WSKeyEvent unknown;
    CHECK(!WSKeymapTranslate(km, 0x7F, true, &unknown));
    CHECK(unknown.kind == WSKeyIgnored);
    CHECK(!WSKeymapTranslate(km, 300, true, &unknown));
    WSKeymapDestroy(km);
}

static void test_modifiers(void) {
    WSKeymap *km = keymap("us", NULL);
    if (km == NULL) {
        CHECK(km != NULL);
        return;
    }

    WSKeyEvent ev = press(km, kVK_Shift);
    CHECK(ev.kind == WSKeyFlagsChanged);
    CHECK(ev.modifierFlags == WSModifierShift);
    CHECK(ev.characters[0] == '\0');
    ev = tap(km, kVK_ANSI_A);
    CHECK(strcmp(ev.characters, "A") == 0);
    CHECK(strcmp(ev.charactersIgnoringModifiers, "A") == 0);   /* Shift is kept */
    CHECK(ev.modifierFlags == WSModifierShift);
    ev = release(km, kVK_Shift);
    CHECK(ev.kind == WSKeyFlagsChanged && ev.modifierFlags == 0);

    press(km, kVK_Command);
    ev = tap(km, kVK_ANSI_A);
    CHECK(ev.modifierFlags == WSModifierCommand);
    CHECK(strcmp(ev.charactersIgnoringModifiers, "a") == 0);
    release(km, kVK_Command);

    press(km, kVK_Control);
    ev = tap(km, kVK_ANSI_A);
    CHECK(ev.modifierFlags == WSModifierControl);
    CHECK(strcmp(ev.characters, "\x01") == 0);
    CHECK(strcmp(ev.charactersIgnoringModifiers, "a") == 0);
    release(km, kVK_Control);

    press(km, kVK_Option);
    CHECK(WSKeymapModifierFlags(km) == WSModifierOption);
    release(km, kVK_Option);
    CHECK(WSKeymapModifierFlags(km) == 0);

    /* Caps Lock locks; charactersIgnoringModifiers ignores it. */
    ev = tap(km, kVK_CapsLock);
    CHECK(ev.kind == WSKeyFlagsChanged);
    CHECK(WSKeymapModifierFlags(km) == WSModifierCapsLock);
    ev = tap(km, kVK_ANSI_A);
    CHECK(strcmp(ev.characters, "A") == 0);
    CHECK(strcmp(ev.charactersIgnoringModifiers, "a") == 0);
    tap(km, kVK_CapsLock);
    CHECK(WSKeymapModifierFlags(km) == 0);

    ev = press(km, kVK_Function);
    CHECK(ev.kind == WSKeyFlagsChanged && ev.modifierFlags == WSModifierFunction);
    ev = release(km, kVK_Function);
    CHECK(ev.modifierFlags == 0);
    WSKeymapDestroy(km);
}

/* UTF-8 of an NS*FunctionKey character. */
static int isFunctionChar(const char *s, unsigned code) {
    char expect[4] = {
        (char)(0xE0 | (code >> 12)), (char)(0x80 | ((code >> 6) & 0x3F)),
        (char)(0x80 | (code & 0x3F)), 0
    };
    return strcmp(s, expect) == 0;
}

static void test_function_keys(void) {
    WSKeymap *km = keymap("us", NULL);
    if (km == NULL) {
        CHECK(km != NULL);
        return;
    }

    WSKeyEvent ev = tap(km, kVK_UpArrow);
    CHECK(isFunctionChar(ev.characters, 0xF700));
    CHECK(ev.modifierFlags == (WSModifierNumericPad | WSModifierFunction));
    CHECK(isFunctionChar(tap(km, kVK_LeftArrow).characters, 0xF702));
    ev = tap(km, kVK_F1);
    CHECK(isFunctionChar(ev.characters, 0xF704));
    CHECK(ev.modifierFlags == WSModifierFunction);
    CHECK(isFunctionChar(tap(km, kVK_F12).characters, 0xF70F));
    CHECK(isFunctionChar(tap(km, kVK_ForwardDelete).characters, 0xF728));
    CHECK(isFunctionChar(tap(km, kVK_Help).characters, 0xF746));

    /* The keypad always types digits, like a Mac keypad. */
    ev = tap(km, kVK_ANSI_Keypad1);
    CHECK(strcmp(ev.characters, "1") == 0);
    CHECK(ev.modifierFlags == WSModifierNumericPad);
    ev = tap(km, kVK_ANSI_KeypadEnter);
    CHECK(strcmp(ev.characters, "\x03") == 0);                  /* NSEnterCharacter */
    ev = tap(km, kVK_ANSI_KeypadClear);
    CHECK(isFunctionChar(ev.characters, 0xF739));
    CHECK(strcmp(tap(km, kVK_ANSI_Keypad1).characters, "1") == 0);   /* still digits */
    WSKeymapDestroy(km);
}

static void test_layouts(void) {
    WSKeymap *es = keymap("es", NULL);
    CHECK(es != NULL);
    if (es != NULL) {
        CHECK(strcmp(tap(es, kVK_ANSI_Semicolon).characters, "\xc3\xb1") == 0);   /* ñ */
        WSKeymapDestroy(es);
    }

    WSKeymap *de = keymap("de", NULL);
    CHECK(de != NULL);
    if (de != NULL) {
        CHECK(strcmp(tap(de, kVK_ANSI_Y).characters, "z") == 0);   /* QWERTZ */
        CHECK(strcmp(tap(de, kVK_ANSI_Z).characters, "y") == 0);
        WSKeymapDestroy(de);
    }

    WSKeymap *latam = keymap("latam", NULL);
    CHECK(latam != NULL);
    if (latam != NULL) {
        CHECK(strcmp(tap(latam, kVK_ANSI_Semicolon).characters, "\xc3\xb1") == 0);
        WSKeymapDestroy(latam);
    }

    CHECK(WSKeymapCreate(xkbRoot, NULL, NULL, "no-such-layout", NULL, NULL) == NULL);
}

static void test_evdev_table(void) {
    CHECK(WSKeymapEvdevCode(kVK_ANSI_A) == 30);
    CHECK(WSKeymapEvdevCode(kVK_Command) == 125);
    CHECK(WSKeymapEvdevCode(kVK_UpArrow) == 103);
    CHECK(WSKeymapEvdevCode(kVK_Function) == 0);
    CHECK(WSKeymapEvdevCode(200) == 0);

    /* No two Mac keys map to the same evdev key. */
    int seen[256] = {0};
    int duplicates = 0;
    for (uint16_t k = 0; k < 128; k++) {
        uint16_t e = WSKeymapEvdevCode(k);
        if (e != 0 && seen[e]++)
            duplicates++;
    }
    CHECK(duplicates == 0);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s xkeyboard-config-directory\n", argv[0]);
        return 2;
    }
    xkbRoot = argv[1];

    test_evdev_table();
    test_us_basic();
    test_modifiers();
    test_function_keys();
    test_layouts();

    printf("test_keymap: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
