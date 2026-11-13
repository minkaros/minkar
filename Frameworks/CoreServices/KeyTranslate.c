/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * UCKeyTranslate with Minkar's layout data: the window server's keymap
 * (SkyLight's SLSKeyboardTranslate) gives the characters. Its layout is
 * the one in use, the only one the input sources offer. There are no
 * dead key sequences across calls: each key gives its own characters.
 */

#include "UnicodeUtilities.h"
#include "MacErrors.h"
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    char characters[8];
    char charactersIgnoringModifiers[8];
} KeyCharacters;

typedef int (*TranslateFn)(uint16_t keyCode, uint32_t modifierFlags, KeyCharacters *characters);

static TranslateFn translateFunction(void) {
    static TranslateFn fn;
    static int looked;
    if (!looked) {
        looked = 1;
        fn = (TranslateFn)dlsym(RTLD_DEFAULT, "SLSKeyboardTranslate");
        if (fn == NULL) {
            void *h = dlopen("/System/Library/PrivateFrameworks/SkyLight.framework/SkyLight", RTLD_LAZY);
            if (h != NULL)
                fn = (TranslateFn)dlsym(h, "SLSKeyboardTranslate");
        }
    }
    return fn;
}

/* Carbon's modifiers (shifted right by 8) as CGEventFlags. */
static uint32_t eventFlags(UInt32 modifiers) {
    uint32_t flags = 0;
    if (modifiers & 0x01) flags |= 0x00100000;	/* command */
    if (modifiers & 0x02) flags |= 0x00020000;	/* shift */
    if (modifiers & 0x04) flags |= 0x00010000;	/* caps lock */
    if (modifiers & 0x08) flags |= 0x00080000;	/* option */
    if (modifiers & 0x10) flags |= 0x00040000;	/* control */
    return flags;
}

/* UTF-8 into UTF-16; the count of UTF-16 units. */
static UniCharCount toUTF16(const char *s, UniChar *out, UniCharCount max, UniCharCount *needed) {
    UniCharCount n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p != 0) {
        uint32_t c;
        if (*p < 0x80) c = *p++;
        else if ((*p & 0xE0) == 0xC0 && p[1]) { c = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
        else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) { c = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
        else if ((*p & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
            c = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); p += 4;
        } else { p++; continue; }
        if (c >= 0x10000) {
            c -= 0x10000;
            if (n < max) out[n] = (UniChar)(0xD800 + (c >> 10));
            n++;
            if (n < max) out[n] = (UniChar)(0xDC00 + (c & 0x3FF));
            n++;
        } else {
            if (n < max) out[n] = (UniChar)c;
            n++;
        }
    }
    *needed = n;
    return n < max ? n : max;
}

OSStatus UCKeyTranslate(const UCKeyboardLayout *keyLayoutPtr, UInt16 virtualKeyCode, UInt16 keyAction,
    UInt32 modifierKeyState, UInt32 keyboardType, OptionBits keyTranslateOptions, UInt32 *deadKeyState,
    UniCharCount maxStringLength, UniCharCount *actualStringLength, UniChar unicodeString[]) {
    const MinkarKeyboardLayoutData *layout = (const MinkarKeyboardLayoutData *)keyLayoutPtr;
    if (actualStringLength != NULL)
        *actualStringLength = 0;
    if (deadKeyState != NULL)
        *deadKeyState = 0;
    if (layout == NULL || layout->magic != kMinkarKeyboardLayoutMagic || actualStringLength == NULL)
        return paramErr;
    if (keyAction == kUCKeyActionUp)
        return noErr;
    TranslateFn translate = translateFunction();
    KeyCharacters chars;
    memset(&chars, 0, sizeof(chars));
    uint32_t flags = keyAction == kUCKeyActionDisplay ? 0 : eventFlags(modifierKeyState);
    if (translate == NULL || translate(virtualKeyCode, flags, &chars) != 0)
        return noErr;	/* no characters: as a key that types none */
    chars.characters[sizeof(chars.characters) - 1] = '\0';
    UniCharCount needed;
    UniCharCount written = toUTF16(chars.characters, unicodeString, maxStringLength, &needed);
    *actualStringLength = written;
    return needed > maxStringLength ? kUCOutputBufferTooSmall : noErr;
}
