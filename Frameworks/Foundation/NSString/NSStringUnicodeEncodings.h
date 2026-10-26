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

// UTF-16 and UTF-32 in a stated byte order, to and from NSString's UTF-16
// characters.
//
// Byte order as on macOS: NSUTF16BigEndian/LittleEndian and
// NSUTF32BigEndian/LittleEndian write no byte order mark and keep one
// they read as a character (U+FEFF); NSUTF32StringEncoding writes one in
// host order and, when reading, uses one if present (host order if not).

#import <Foundation/NSString.h>

// YES when the encoding is one of those handled here.
BOOL NSStringIsUTF16Or32(NSStringEncoding encoding);

// The bytes, malloc'd, or NULL when a character cannot be encoded (an
// unpaired surrogate in UTF-32) and lossy is NO; lossy writes '?' instead.
uint8_t *NSStringEncodeUTF16Or32(const unichar *characters, NSUInteger length,
    NSStringEncoding encoding, BOOL lossy, NSUInteger *byteLength);

// The characters, malloc'd, or NULL for bytes that are not valid in the
// encoding (a length that is not a whole number of units, a UTF-32 value
// that is not a Unicode scalar value).
unichar *NSStringDecodeUTF16Or32(const uint8_t *bytes, NSUInteger byteLength,
    NSStringEncoding encoding, NSUInteger *length);
