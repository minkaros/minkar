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

#import "NSStringUnicodeEncodings.h"

#include <stdlib.h>
#include <string.h>

BOOL
NSStringIsUTF16Or32(NSStringEncoding encoding)
{
    switch (encoding) {
    case NSUTF16BigEndianStringEncoding:
    case NSUTF16LittleEndianStringEncoding:
    case NSUTF32StringEncoding:
    case NSUTF32BigEndianStringEncoding:
    case NSUTF32LittleEndianStringEncoding:
        return YES;
    default:
        return NO;
    }
}

static BOOL
hostIsBigEndian(void)
{
    const uint16_t one = 1;
    return *(const uint8_t *)&one == 0;
}

static void
put16(uint8_t *p, uint16_t v, BOOL big)
{
    p[big ? 0 : 1] = (uint8_t)(v >> 8);
    p[big ? 1 : 0] = (uint8_t)v;
}

static void
put32(uint8_t *p, uint32_t v, BOOL big)
{
    for (int i = 0; i < 4; i++)
        p[big ? 3 - i : i] = (uint8_t)(v >> (8 * i));
}

static uint32_t
get32(const uint8_t *p, BOOL big)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i++)
        v |= (uint32_t)p[big ? 3 - i : i] << (8 * i);
    return v;
}

uint8_t *
NSStringEncodeUTF16Or32(const unichar *characters, NSUInteger length,
    NSStringEncoding encoding, BOOL lossy, NSUInteger *byteLength)
{
    uint8_t *bytes, *p;
    BOOL big;

    if (encoding == NSUTF16BigEndianStringEncoding || encoding == NSUTF16LittleEndianStringEncoding) {
        big = (encoding == NSUTF16BigEndianStringEncoding);
        bytes = malloc(length * 2 + 1);
        if (bytes == NULL)
            return NULL;
        for (NSUInteger i = 0; i < length; i++)
            put16(bytes + 2 * i, characters[i], big);
        *byteLength = length * 2;
        return bytes;
    }

    // UTF-32: surrogate pairs become one value.
    BOOL bom = (encoding == NSUTF32StringEncoding);
    big = encoding == NSUTF32BigEndianStringEncoding || (bom && hostIsBigEndian());
    bytes = malloc((length + 1) * 4);
    if (bytes == NULL)
        return NULL;
    p = bytes;
    if (bom) {
        put32(p, 0xfeff, big);
        p += 4;
    }
    for (NSUInteger i = 0; i < length; i++) {
        uint32_t c = characters[i];
        if (c >= 0xd800 && c <= 0xdbff && i + 1 < length &&
            characters[i + 1] >= 0xdc00 && characters[i + 1] <= 0xdfff) {
            c = 0x10000 + ((c - 0xd800) << 10) + (characters[i + 1] - 0xdc00);
            i++;
        } else if (c >= 0xd800 && c <= 0xdfff) {
            if (!lossy) {
                free(bytes);
                return NULL;
            }
            c = '?';
        }
        put32(p, c, big);
        p += 4;
    }
    *byteLength = (NSUInteger)(p - bytes);
    return bytes;
}

unichar *
NSStringDecodeUTF16Or32(const uint8_t *bytes, NSUInteger byteLength,
    NSStringEncoding encoding, NSUInteger *length)
{
    unichar *characters, *q;
    BOOL big;

    if (encoding == NSUTF16BigEndianStringEncoding || encoding == NSUTF16LittleEndianStringEncoding) {
        if (byteLength % 2 != 0)
            return NULL;
        big = (encoding == NSUTF16BigEndianStringEncoding);
        characters = malloc((byteLength / 2 + 1) * sizeof(unichar));
        if (characters == NULL)
            return NULL;
        for (NSUInteger i = 0; i < byteLength / 2; i++) {
            const uint8_t *p = bytes + 2 * i;
            characters[i] = big ? (unichar)((p[0] << 8) | p[1]) : (unichar)((p[1] << 8) | p[0]);
        }
        *length = byteLength / 2;
        return characters;
    }

    if (byteLength % 4 != 0)
        return NULL;
    big = encoding == NSUTF32BigEndianStringEncoding;
    if (encoding == NSUTF32StringEncoding) {
        big = hostIsBigEndian();
        if (byteLength >= 4 && get32(bytes, YES) == 0xfeff) {
            big = YES;
            bytes += 4;
            byteLength -= 4;
        } else if (byteLength >= 4 && get32(bytes, NO) == 0xfeff) {
            big = NO;
            bytes += 4;
            byteLength -= 4;
        }
    }
    // At most two UTF-16 units per value.
    characters = malloc((byteLength / 4 * 2 + 1) * sizeof(unichar));
    if (characters == NULL)
        return NULL;
    q = characters;
    for (NSUInteger i = 0; i < byteLength; i += 4) {
        uint32_t c = get32(bytes + i, big);
        if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) {
            free(characters);
            return NULL;
        }
        if (c >= 0x10000) {
            c -= 0x10000;
            *q++ = (unichar)(0xd800 + (c >> 10));
            *q++ = (unichar)(0xdc00 + (c & 0x3ff));
        } else {
            *q++ = (unichar)c;
        }
    }
    *length = (NSUInteger)(q - characters);
    return characters;
}
