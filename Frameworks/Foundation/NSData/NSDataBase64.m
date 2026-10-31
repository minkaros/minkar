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

/* Base64 (RFC 4648) for NSData. */

#import <Foundation/NSData.h>
#import <Foundation/NSString.h>
#include <stdlib.h>

static const char alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// The encoded bytes, with line breaks if the options ask for them.
static NSData *encode(const uint8_t *bytes, NSUInteger length, NSDataBase64EncodingOptions options) {
    NSUInteger lineLength = 0;
    if (options & NSDataBase64Encoding64CharacterLineLength)
        lineLength = 64;
    else if (options & NSDataBase64Encoding76CharacterLineLength)
        lineLength = 76;
    BOOL cr = (options & NSDataBase64EncodingEndLineWithCarriageReturn) != 0;
    BOOL lf = (options & NSDataBase64EncodingEndLineWithLineFeed) != 0;
    if (!cr && !lf)
        cr = lf = YES;          // CR LF when no line ending is named
    NSUInteger encoded = (length + 2) / 3 * 4;
    NSUInteger breaks = (lineLength > 0 && encoded > 0) ? (encoded - 1) / lineLength : 0;
    NSUInteger capacity = encoded + breaks * ((cr ? 1 : 0) + (lf ? 1 : 0));
    uint8_t *out = malloc(capacity > 0 ? capacity : 1);
    NSUInteger o = 0, column = 0;

    for (NSUInteger i = 0; i < length; i += 3) {
        uint32_t chunk = (uint32_t)bytes[i] << 16;
        if (i + 1 < length) chunk |= (uint32_t)bytes[i + 1] << 8;
        if (i + 2 < length) chunk |= bytes[i + 2];
        char quad[4] = {
            alphabet[(chunk >> 18) & 63], alphabet[(chunk >> 12) & 63],
            i + 1 < length ? alphabet[(chunk >> 6) & 63] : '=',
            i + 2 < length ? alphabet[chunk & 63] : '=',
        };
        for (int k = 0; k < 4; k++) {
            if (lineLength > 0 && column == lineLength) {
                if (cr) out[o++] = '\r';
                if (lf) out[o++] = '\n';
                column = 0;
            }
            out[o++] = (uint8_t)quad[k];
            column++;
        }
    }
    return [NSData dataWithBytesNoCopy:out length:o freeWhenDone:YES];
}

static int value(uint8_t c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

// nil for input that is not Base64: a character outside the alphabet
// (unless ignored), padding before the end, or a length that is not a
// multiple of four.
static NSData *decode(const uint8_t *bytes, NSUInteger length, NSDataBase64DecodingOptions options) {
    BOOL ignore = (options & NSDataBase64DecodingIgnoreUnknownCharacters) != 0;
    uint8_t *out = malloc(length / 4 * 3 + 3);
    NSUInteger o = 0, count = 0, padding = 0;
    uint32_t chunk = 0;

    for (NSUInteger i = 0; i < length; i++) {
        uint8_t c = bytes[i];
        if (c == '=') {
            padding++;
            count++;
            if (padding > 2)
                goto fail;
            chunk <<= 6;
        } else {
            int v = value(c);
            if (v < 0) {
                if (ignore)
                    continue;
                goto fail;
            }
            if (padding > 0)
                goto fail;
            chunk = (chunk << 6) | (uint32_t)v;
            count++;
        }
        if (count % 4 == 0) {
            out[o++] = (chunk >> 16) & 0xff;
            if (padding < 2) out[o++] = (chunk >> 8) & 0xff;
            if (padding < 1) out[o++] = chunk & 0xff;
            chunk = 0;
        }
    }
    if (count % 4 != 0)
        goto fail;
    return [NSData dataWithBytesNoCopy:out length:o freeWhenDone:YES];
fail:
    free(out);
    return nil;
}

@implementation NSData (NSDataBase64Encoding)

- (instancetype)initWithBase64EncodedData:(NSData *)base64Data options:(NSDataBase64DecodingOptions)options {
    NSData *data = base64Data ? decode([base64Data bytes], [base64Data length], options) : nil;
    if (data == nil)
        return nil;
    return [self initWithData:data];
}

- (instancetype)initWithBase64EncodedString:(NSString *)base64String options:(NSDataBase64DecodingOptions)options {
    NSData *ascii = [base64String dataUsingEncoding:NSUTF8StringEncoding];
    if (ascii == nil)
        return nil;
    return [self initWithBase64EncodedData:ascii options:options];
}

- (NSData *)base64EncodedDataWithOptions:(NSDataBase64EncodingOptions)options {
    return encode([self bytes], [self length], options);
}

- (NSString *)base64EncodedStringWithOptions:(NSDataBase64EncodingOptions)options {
    NSData *data = encode([self bytes], [self length], options);
    return [[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding];
}

@end
