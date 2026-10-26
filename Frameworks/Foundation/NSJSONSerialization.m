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

// NSJSONSerialization: JSON (RFC 8259) to Foundation objects and back.
// Built with ARC.
//
// Reading works on UTF-8 bytes; UTF-16 and UTF-32 input is detected from
// its first bytes and converted first. With NSJSONReadingJSON5Allowed the
// parser also takes JSON5: comments, trailing commas, single-quoted
// strings, unquoted keys, hexadecimal numbers, Infinity and NaN.
//
// Writing checks the whole object first: an object that is not valid JSON
// raises NSInvalidArgumentException, as on macOS.

#import <Foundation/NSJSONSerialization.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSError.h>
#import <Foundation/NSException.h>
#import <Foundation/NSInputStream.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSOutputStream.h>
#import <Foundation/NSString.h>
#import <Foundation/FoundationErrors.h>
#import <CoreFoundation/CFNumber.h>

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xlocale.h>

// Deeper nesting is refused rather than risking the stack.
#define JSON_MAX_DEPTH 512

// Numbers are read and written in the C locale whatever the process uses.
static locale_t gCLocale;
static pthread_once_t gCLocaleOnce = PTHREAD_ONCE_INIT;

static void
makeCLocale(void)
{
    gCLocale = newlocale(LC_ALL_MASK, "C", NULL);
}

static locale_t
cLocale(void)
{
    pthread_once(&gCLocaleOnce, makeCLocale);
    return gCLocale;
}

static NSError *
jsonError(NSInteger code, NSString *debugDescription)
{
    NSDictionary *info = @{
        NSLocalizedDescriptionKey: @"The data couldn't be read because it isn't valid JSON.",
        NSDebugDescriptionErrorKey: debugDescription,
    };
    return [NSError errorWithDomain:NSCocoaErrorDomain code:code userInfo:info];
}

/* ---- Reading ----------------------------------------------------------- */

typedef struct {
    const uint8_t *start;
    const uint8_t *p;
    const uint8_t *end;
    NSJSONReadingOptions options;
    BOOL json5;
    unsigned depth;
    // The first error: what went wrong, and where.
    const char *message;
    const uint8_t *errorAt;
} JSONReader;

static BOOL
readerFail(JSONReader *r, const char *message)
{
    if (r->message == NULL) {
        r->message = message;
        r->errorAt = r->p;
    }
    return NO;
}

static NSString *
readerErrorDescription(JSONReader *r)
{
    unsigned long line = 1, column = 1;

    for (const uint8_t *q = r->start; q < r->errorAt && q < r->end; q++) {
        if (*q == '\n') {
            line++;
            column = 1;
        } else if ((*q & 0xc0) != 0x80) {
            column++;
        }
    }
    return [NSString stringWithFormat:@"%s around line %lu, column %lu.", r->message, line, column];
}

static BOOL
startsWith(JSONReader *r, const char *s)
{
    size_t n = strlen(s);
    return (size_t)(r->end - r->p) >= n && memcmp(r->p, s, n) == 0;
}

// JSON whitespace; JSON5 adds more space characters and comments.
static BOOL
skipSpace(JSONReader *r)
{
    while (r->p < r->end) {
        uint8_t c = *r->p;

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            r->p++;
            continue;
        }
        if (!r->json5)
            break;
        if (c == '\v' || c == '\f') {
            r->p++;
        } else if (startsWith(r, "\xc2\xa0")) {                     // U+00A0
            r->p += 2;
        } else if (startsWith(r, "\xef\xbb\xbf") ||                  // U+FEFF
                   startsWith(r, "\xe2\x80\xa8") ||                  // U+2028
                   startsWith(r, "\xe2\x80\xa9")) {                  // U+2029
            r->p += 3;
        } else if (startsWith(r, "//")) {
            while (r->p < r->end && *r->p != '\n' && *r->p != '\r')
                r->p++;
        } else if (startsWith(r, "/*")) {
            const uint8_t *open = r->p;
            r->p += 2;
            while (r->p < r->end && !startsWith(r, "*/"))
                r->p++;
            if (r->p >= r->end) {
                r->p = open;
                return readerFail(r, "Unterminated comment");
            }
            r->p += 2;
        } else {
            break;
        }
    }
    return YES;
}

static int
hexValue(uint8_t c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static BOOL
readHex(JSONReader *r, int digits, uint32_t *value)
{
    uint32_t v = 0;

    if (r->end - r->p < digits)
        return readerFail(r, "Invalid escape sequence");
    for (int i = 0; i < digits; i++) {
        int h = hexValue(r->p[i]);
        if (h < 0)
            return readerFail(r, "Invalid escape sequence");
        v = (v << 4) | (uint32_t)h;
    }
    r->p += digits;
    *value = v;
    return YES;
}

// A growable byte buffer for strings that have escapes.
typedef struct {
    uint8_t *bytes;
    size_t length;
    size_t capacity;
    uint8_t inlineBytes[256];
} ByteBuffer;

static void
bufferInit(ByteBuffer *b)
{
    b->bytes = b->inlineBytes;
    b->length = 0;
    b->capacity = sizeof(b->inlineBytes);
}

static void
bufferFree(ByteBuffer *b)
{
    if (b->bytes != b->inlineBytes)
        free(b->bytes);
}

static BOOL
bufferAppend(ByteBuffer *b, const void *bytes, size_t n)
{
    if (b->length + n > b->capacity) {
        size_t capacity = b->capacity * 2;
        while (capacity < b->length + n)
            capacity *= 2;
        uint8_t *grown = malloc(capacity);
        if (grown == NULL)
            return NO;
        memcpy(grown, b->bytes, b->length);
        bufferFree(b);
        b->bytes = grown;
        b->capacity = capacity;
    }
    memcpy(b->bytes + b->length, bytes, n);
    b->length += n;
    return YES;
}

static BOOL
bufferAppendCodePoint(ByteBuffer *b, uint32_t c)
{
    uint8_t u[4];
    size_t n;

    if (c < 0x80) {
        u[0] = (uint8_t)c;
        n = 1;
    } else if (c < 0x800) {
        u[0] = (uint8_t)(0xc0 | (c >> 6));
        u[1] = (uint8_t)(0x80 | (c & 0x3f));
        n = 2;
    } else if (c < 0x10000) {
        u[0] = (uint8_t)(0xe0 | (c >> 12));
        u[1] = (uint8_t)(0x80 | ((c >> 6) & 0x3f));
        u[2] = (uint8_t)(0x80 | (c & 0x3f));
        n = 3;
    } else {
        u[0] = (uint8_t)(0xf0 | (c >> 18));
        u[1] = (uint8_t)(0x80 | ((c >> 12) & 0x3f));
        u[2] = (uint8_t)(0x80 | ((c >> 6) & 0x3f));
        u[3] = (uint8_t)(0x80 | (c & 0x3f));
        n = 4;
    }
    return bufferAppend(b, u, n);
}

static NSString *
makeString(JSONReader *r, const uint8_t *bytes, size_t length, BOOL mutable)
{
    Class cls = mutable ? [NSMutableString class] : [NSString class];
    NSString *s = [[cls alloc] initWithBytes:bytes length:length encoding:NSUTF8StringEncoding];

    if (s == nil)
        readerFail(r, "Invalid UTF-8 in string");
    return s;
}

// After a backslash: one escape sequence, appended to the buffer.
static BOOL
readEscape(JSONReader *r, ByteBuffer *b)
{
    uint8_t c;
    uint32_t cp, low;

    if (r->p >= r->end)
        return readerFail(r, "Unterminated string");
    c = *r->p++;
    switch (c) {
    case '"': case '\\': case '/':
        return bufferAppend(b, &c, 1);
    case 'b': return bufferAppendCodePoint(b, '\b');
    case 'f': return bufferAppendCodePoint(b, '\f');
    case 'n': return bufferAppendCodePoint(b, '\n');
    case 'r': return bufferAppendCodePoint(b, '\r');
    case 't': return bufferAppendCodePoint(b, '\t');
    case 'u':
        if (!readHex(r, 4, &cp))
            return NO;
        if (cp >= 0xdc00 && cp <= 0xdfff)
            return readerFail(r, "Unpaired low surrogate in string");
        if (cp >= 0xd800 && cp <= 0xdbff) {
            if (!startsWith(r, "\\u"))
                return readerFail(r, "Missing low surrogate in string");
            r->p += 2;
            if (!readHex(r, 4, &low))
                return NO;
            if (low < 0xdc00 || low > 0xdfff)
                return readerFail(r, "Missing low surrogate in string");
            cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
        }
        return bufferAppendCodePoint(b, cp);
    }
    if (r->json5) {
        switch (c) {
        case '\'': return bufferAppend(b, &c, 1);
        case 'v': return bufferAppendCodePoint(b, '\v');
        case '0':
            if (r->p < r->end && *r->p >= '0' && *r->p <= '9')
                break;
            return bufferAppendCodePoint(b, 0);
        case 'x':
            if (!readHex(r, 2, &cp))
                return NO;
            return bufferAppendCodePoint(b, cp);
        case '\n':
            return YES;                                     // line continuation
        case '\r':
            if (r->p < r->end && *r->p == '\n')
                r->p++;
            return YES;
        default:
            if (c == 0xe2 && r->end - r->p >= 2 && r->p[0] == 0x80 &&
                (r->p[1] == 0xa8 || r->p[1] == 0xa9)) {
                r->p += 2;                                  // U+2028, U+2029
                return YES;
            }
            if (c >= '1' && c <= '9')
                break;
            return bufferAppend(b, &c, 1);                  // the character itself
        }
    }
    r->p--;
    return readerFail(r, "Invalid escape sequence");
}

// At the opening quote ('"', or '\'' in JSON5).
static NSString *
readString(JSONReader *r, BOOL mutable)
{
    uint8_t quote = *r->p++;
    const uint8_t *run = r->p;
    ByteBuffer b;
    NSString *s;

    // Most strings have no escapes: make them straight from the input.
    while (r->p < r->end && *r->p != quote && *r->p != '\\' && *r->p >= 0x20)
        r->p++;
    if (r->p < r->end && *r->p == quote) {
        s = makeString(r, run, (size_t)(r->p - run), mutable);
        r->p++;
        return s;
    }

    bufferInit(&b);
    bufferAppend(&b, run, (size_t)(r->p - run));
    for (;;) {
        if (r->p >= r->end) {
            readerFail(r, "Unterminated string");
            break;
        }
        uint8_t c = *r->p;
        if (c == quote) {
            r->p++;
            s = makeString(r, b.bytes, b.length, mutable);
            bufferFree(&b);
            return s;
        }
        if (c < 0x20) {
            readerFail(r, "Unescaped control character in string");
            break;
        }
        if (c == '\\') {
            r->p++;
            if (!readEscape(r, &b))
                break;
            continue;
        }
        run = r->p;
        while (r->p < r->end && *r->p != quote && *r->p != '\\' && *r->p >= 0x20)
            r->p++;
        if (!bufferAppend(&b, run, (size_t)(r->p - run))) {
            readerFail(r, "Out of memory");
            break;
        }
    }
    bufferFree(&b);
    return nil;
}

static BOOL
isDigit(JSONReader *r)
{
    return r->p < r->end && *r->p >= '0' && *r->p <= '9';
}

static BOOL
isIdentifierByte(uint8_t c, BOOL first)
{
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$' || c >= 0x80)
        return YES;
    return !first && c >= '0' && c <= '9';
}

static NSNumber *
readNumber(JSONReader *r)
{
    const uint8_t *s = r->p;
    BOOL negative = NO, integral = YES;
    char small[64];
    char *text;
    size_t length;
    NSNumber *n = nil;

    if (r->p < r->end && (*r->p == '-' || (r->json5 && *r->p == '+'))) {
        negative = (*r->p == '-');
        r->p++;
    }
    if (r->json5) {
        if (startsWith(r, "Infinity")) {
            r->p += 8;
            return [NSNumber numberWithDouble:negative ? -INFINITY : INFINITY];
        }
        if (startsWith(r, "NaN")) {
            r->p += 3;
            return [NSNumber numberWithDouble:NAN];
        }
        if (startsWith(r, "0x") || startsWith(r, "0X")) {
            const uint8_t *digits = r->p + 2;
            unsigned long long v = 0;
            r->p = digits;
            while (r->p < r->end && hexValue(*r->p) >= 0) {
                if (v >> 60)
                    return readerFail(r, "Number out of range"), nil;
                v = (v << 4) | (unsigned long long)hexValue(*r->p++);
            }
            if (r->p == digits)
                return readerFail(r, "Invalid number"), nil;
            if (!negative)
                return [NSNumber numberWithUnsignedLongLong:v];
            return [NSNumber numberWithLongLong:-(long long)v];
        }
    }

    // The integer part: 0, or digits not starting with 0. JSON5 allows
    // none at all before a fraction (".5").
    if (isDigit(r)) {
        if (*r->p == '0')
            r->p++;
        else
            while (isDigit(r))
                r->p++;
    } else if (!(r->json5 && r->p < r->end && *r->p == '.')) {
        readerFail(r, "Invalid number");
        return nil;
    }
    const uint8_t *intEnd = r->p;
    if (r->p < r->end && *r->p == '.') {
        integral = NO;
        r->p++;
        if (!isDigit(r)) {
            // JSON5 allows "5." but not "." alone.
            if (!r->json5 || intEnd == s + (negative || *s == '+' ? 1 : 0))
                return readerFail(r, "Invalid number: no digits after the decimal point"), nil;
        }
        while (isDigit(r))
            r->p++;
    }
    if (r->p < r->end && (*r->p == 'e' || *r->p == 'E')) {
        integral = NO;
        r->p++;
        if (r->p < r->end && (*r->p == '+' || *r->p == '-'))
            r->p++;
        if (!isDigit(r))
            return readerFail(r, "Invalid number: no digits in the exponent"), nil;
        while (isDigit(r))
            r->p++;
    }

    length = (size_t)(r->p - s);
    text = length < sizeof(small) ? small : malloc(length + 1);
    if (text == NULL)
        return readerFail(r, "Out of memory"), nil;
    memcpy(text, s, length);
    text[length] = '\0';
    if (text[0] == '+')
        text[0] = ' ';

    if (integral) {
        errno = 0;
        long long v = strtoll(text, NULL, 10);
        if (errno == 0) {
            n = [NSNumber numberWithLongLong:v];
        } else if (!negative) {
            errno = 0;
            unsigned long long u = strtoull(text, NULL, 10);
            if (errno == 0)
                n = [NSNumber numberWithUnsignedLongLong:u];
        }
    }
    if (n == nil) {
        double d = strtod_l(text, NULL, cLocale());
        if (isinf(d))
            readerFail(r, "Number out of range");
        else
            n = [NSNumber numberWithDouble:d];
    }
    if (text != small)
        free(text);
    return n;
}

static id readValue(JSONReader *r);

static BOOL
enter(JSONReader *r)
{
    if (++r->depth > JSON_MAX_DEPTH)
        return readerFail(r, "Too many nested arrays or dictionaries");
    return YES;
}

static id
finishContainer(JSONReader *r, id container)
{
    r->depth--;
    if (r->options & NSJSONReadingMutableContainers)
        return container;
    return [container copy];
}

// At '['.
static NSArray *
readArray(JSONReader *r)
{
    NSMutableArray *array = [NSMutableArray array];

    if (!enter(r))
        return nil;
    r->p++;
    if (!skipSpace(r))
        return nil;
    if (r->p < r->end && *r->p == ']') {
        r->p++;
        return finishContainer(r, array);
    }
    for (;;) {
        id value = readValue(r);
        if (value == nil)
            return nil;
        [array addObject:value];
        if (!skipSpace(r))
            return nil;
        if (r->p >= r->end)
            return readerFail(r, "Unterminated array"), nil;
        if (*r->p == ']') {
            r->p++;
            return finishContainer(r, array);
        }
        if (*r->p != ',')
            return readerFail(r, "Expected ',' or ']' in array"), nil;
        r->p++;
        if (!skipSpace(r))
            return nil;
        if (r->json5 && r->p < r->end && *r->p == ']') {
            r->p++;
            return finishContainer(r, array);
        }
    }
}

// A dictionary key: a string, or in JSON5 also an identifier.
static NSString *
readKey(JSONReader *r)
{
    if (r->p < r->end && (*r->p == '"' || (r->json5 && *r->p == '\'')))
        return readString(r, NO);
    if (r->json5 && r->p < r->end && isIdentifierByte(*r->p, YES)) {
        const uint8_t *s = r->p;
        while (r->p < r->end && isIdentifierByte(*r->p, NO))
            r->p++;
        return makeString(r, s, (size_t)(r->p - s), NO);
    }
    readerFail(r, "Expected a string key in dictionary");
    return nil;
}

// The members of a dictionary up to `close` ('}', or the end of input
// for NSJSONReadingTopLevelDictionaryAssumed).
static BOOL
readMembers(JSONReader *r, NSMutableDictionary *dict, int close)
{
    if (!skipSpace(r))
        return NO;
    if (close == '}' ? (r->p < r->end && *r->p == '}') : r->p >= r->end)
        return YES;
    for (;;) {
        NSString *key = readKey(r);
        if (key == nil || !skipSpace(r))
            return NO;
        if (r->p >= r->end || *r->p != ':')
            return readerFail(r, "Expected ':' after a dictionary key");
        r->p++;
        id value = readValue(r);
        if (value == nil)
            return NO;
        dict[key] = value;
        if (!skipSpace(r))
            return NO;
        if (r->p >= r->end)
            return close == '}' ? readerFail(r, "Unterminated dictionary") : YES;
        if (*r->p == close)
            return YES;
        if (*r->p != ',')
            return readerFail(r, close == '}' ? "Expected ',' or '}' in dictionary"
                                              : "Expected ',' in dictionary");
        r->p++;
        if (!skipSpace(r))
            return NO;
        if (r->json5 && (close == '}' ? (r->p < r->end && *r->p == '}') : r->p >= r->end))
            return YES;
    }
}

// At '{'.
static NSDictionary *
readDictionary(JSONReader *r)
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];

    if (!enter(r))
        return nil;
    r->p++;
    if (!readMembers(r, dict, '}'))
        return nil;
    r->p++;
    return finishContainer(r, dict);
}

static id
readValue(JSONReader *r)
{
    if (!skipSpace(r))
        return nil;
    if (r->p >= r->end) {
        readerFail(r, "Unexpected end of input, expected a value");
        return nil;
    }
    switch (*r->p) {
    case '{':
        return readDictionary(r);
    case '[':
        return readArray(r);
    case '"':
        return readString(r, (r->options & NSJSONReadingMutableLeaves) != 0);
    case '\'':
        if (r->json5)
            return readString(r, (r->options & NSJSONReadingMutableLeaves) != 0);
        break;
    case 't':
        if (startsWith(r, "true")) {
            r->p += 4;
            return (__bridge NSNumber *)kCFBooleanTrue;
        }
        break;
    case 'f':
        if (startsWith(r, "false")) {
            r->p += 5;
            return (__bridge NSNumber *)kCFBooleanFalse;
        }
        break;
    case 'n':
        if (startsWith(r, "null")) {
            r->p += 4;
            return [NSNull null];
        }
        break;
    default:
        if (*r->p == '-' || (*r->p >= '0' && *r->p <= '9') ||
            (r->json5 && (*r->p == '+' || *r->p == '.' || *r->p == 'I' || *r->p == 'N')))
            return readNumber(r);
        break;
    }
    readerFail(r, "Invalid value");
    return nil;
}

// RFC 8259 text is UTF-8; older JSON could be UTF-16 or UTF-32, told
// apart by where the zero bytes fall in the first characters.
static NSStringEncoding
detectEncoding(const uint8_t *b, NSUInteger n, NSUInteger *bomLength)
{
    *bomLength = 0;
    if (n >= 3 && b[0] == 0xef && b[1] == 0xbb && b[2] == 0xbf) {
        *bomLength = 3;
        return NSUTF8StringEncoding;
    }
    if (n >= 4 && b[0] == 0 && b[1] == 0 && b[2] == 0xfe && b[3] == 0xff) {
        *bomLength = 4;
        return NSUTF32BigEndianStringEncoding;
    }
    if (n >= 4 && b[0] == 0xff && b[1] == 0xfe && b[2] == 0 && b[3] == 0) {
        *bomLength = 4;
        return NSUTF32LittleEndianStringEncoding;
    }
    if (n >= 2 && b[0] == 0xfe && b[1] == 0xff) {
        *bomLength = 2;
        return NSUTF16BigEndianStringEncoding;
    }
    if (n >= 2 && b[0] == 0xff && b[1] == 0xfe) {
        *bomLength = 2;
        return NSUTF16LittleEndianStringEncoding;
    }
    if (n >= 4 && b[0] == 0 && b[1] == 0 && b[2] == 0 && b[3] != 0)
        return NSUTF32BigEndianStringEncoding;
    if (n >= 4 && b[0] != 0 && b[1] == 0 && b[2] == 0 && b[3] == 0)
        return NSUTF32LittleEndianStringEncoding;
    if (n >= 2 && b[0] == 0 && b[1] != 0)
        return NSUTF16BigEndianStringEncoding;
    if (n >= 2 && b[0] != 0 && b[1] == 0)
        return NSUTF16LittleEndianStringEncoding;
    return NSUTF8StringEncoding;
}

static id
parseData(NSData *data, NSJSONReadingOptions options, NSError **error)
{
    NSUInteger bomLength;
    NSStringEncoding encoding;
    JSONReader r;
    id result;

    if (data == nil)
        [NSException raise:NSInvalidArgumentException format:@"data parameter is nil"];

    encoding = detectEncoding([data bytes], [data length], &bomLength);
    if (encoding != NSUTF8StringEncoding) {
        NSData *rest = [data subdataWithRange:NSMakeRange(bomLength, [data length] - bomLength)];
        NSString *text = [[NSString alloc] initWithData:rest encoding:encoding];
        if (text == nil) {
            if (error != NULL)
                *error = jsonError(NSPropertyListReadCorruptError,
                    @"Unable to convert data to a string in the encoding it appears to use.");
            return nil;
        }
        data = [text dataUsingEncoding:NSUTF8StringEncoding];
        bomLength = 0;
    }

    memset(&r, 0, sizeof(r));
    r.start = (const uint8_t *)[data bytes] + bomLength;
    r.p = r.start;
    r.end = (const uint8_t *)[data bytes] + [data length];
    r.options = options;
    r.json5 = (options & NSJSONReadingJSON5Allowed) != 0;

    if (!skipSpace(&r)) {
        result = nil;
    } else if (r.p >= r.end && !(r.json5 && (options & NSJSONReadingTopLevelDictionaryAssumed))) {
        readerFail(&r, "No value");
        result = nil;
    } else if (r.json5 && (options & NSJSONReadingTopLevelDictionaryAssumed) &&
               (r.p >= r.end || *r.p != '{')) {
        NSMutableDictionary *dict = [NSMutableDictionary dictionary];
        result = readMembers(&r, dict, -1) ? finishContainer(&r, dict) : nil;
        r.depth = 0;
    } else {
        result = readValue(&r);
        if (result != nil && !(options & NSJSONReadingFragmentsAllowed) &&
            ![result isKindOfClass:[NSArray class]] && ![result isKindOfClass:[NSDictionary class]]) {
            r.p = r.start;
            readerFail(&r, "The top-level value is not an array or a dictionary, and fragments are not allowed");
            result = nil;
        }
    }
    if (result != nil && skipSpace(&r) && r.p < r.end) {
        readerFail(&r, "Unexpected data after the value");
        result = nil;
    }
    if (result == nil && error != NULL)
        *error = jsonError(NSPropertyListReadCorruptError, readerErrorDescription(&r));
    return result;
}

/* ---- Writing ----------------------------------------------------------- */

static BOOL
isBoolean(id obj)
{
    return (__bridge CFBooleanRef)obj == kCFBooleanTrue ||
           (__bridge CFBooleanRef)obj == kCFBooleanFalse;
}

static void
append(NSMutableData *out, const char *s)
{
    [out appendBytes:s length:strlen(s)];
}

static void
newline(NSMutableData *out, NSJSONWritingOptions options, unsigned level)
{
    if (!(options & NSJSONWritingPrettyPrinted))
        return;
    append(out, "\n");
    for (unsigned i = 0; i < level; i++)
        append(out, "  ");
}

static void
writeString(NSMutableData *out, NSString *s, NSJSONWritingOptions options)
{
    NSData *utf8 = [s dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:YES];
    const uint8_t *b = [utf8 bytes];
    NSUInteger n = [utf8 length], run = 0;

    append(out, "\"");
    for (NSUInteger i = 0; i < n; i++) {
        uint8_t c = b[i];
        const char *escape = NULL;
        char hex[8];

        switch (c) {
        case '"':  escape = "\\\""; break;
        case '\\': escape = "\\\\"; break;
        case '/':
            if (!(options & NSJSONWritingWithoutEscapingSlashes))
                escape = "\\/";
            break;
        case '\b': escape = "\\b"; break;
        case '\f': escape = "\\f"; break;
        case '\n': escape = "\\n"; break;
        case '\r': escape = "\\r"; break;
        case '\t': escape = "\\t"; break;
        default:
            if (c < 0x20) {
                snprintf(hex, sizeof(hex), "\\u%04x", c);
                escape = hex;
            }
            break;
        }
        if (escape != NULL) {
            [out appendBytes:b + run length:i - run];
            append(out, escape);
            run = i + 1;
        }
    }
    [out appendBytes:b + run length:n - run];
    append(out, "\"");
}

// The shortest text that reads back as the same double.
static void
writeDouble(NSMutableData *out, double d)
{
    char text[40];
    locale_t loc = cLocale();

    for (int precision = 1; precision <= 17; precision++) {
        snprintf_l(text, sizeof(text), loc, "%.*g", precision, d);
        if (strtod_l(text, NULL, loc) == d)
            break;
    }
    append(out, text);
}

// Checks obj and, when out is not nil, writes it. On an invalid object
// returns NO and says why.
static BOOL
writeValue(NSMutableData *out, id obj, NSJSONWritingOptions options, unsigned level,
    NSString **why)
{
    if (level > JSON_MAX_DEPTH) {
        *why = @"Too many nested arrays or dictionaries in JSON write";
        return NO;
    }
    if ([obj isKindOfClass:[NSString class]]) {
        if (out != nil)
            writeString(out, obj, options);
        return YES;
    }
    if ([obj isKindOfClass:[NSNull class]]) {
        if (out != nil)
            append(out, "null");
        return YES;
    }
    if ([obj isKindOfClass:[NSNumber class]]) {
        const char *type = [obj objCType];
        char text[32];

        if (isBoolean(obj)) {
            if (out != nil)
                append(out, (__bridge CFBooleanRef)obj == kCFBooleanTrue ? "true" : "false");
            return YES;
        }
        if (type[0] == 'f' || type[0] == 'd') {
            double d = [obj doubleValue];
            if (isnan(d) || isinf(d)) {
                *why = @"Invalid number value (NaN or infinity) in JSON write";
                return NO;
            }
            if (out != nil)
                writeDouble(out, d);
            return YES;
        }
        if (out != nil) {
            if (type[0] == 'Q' || type[0] == 'L' || type[0] == 'I' || type[0] == 'S' || type[0] == 'C')
                snprintf(text, sizeof(text), "%llu", [obj unsignedLongLongValue]);
            else
                snprintf(text, sizeof(text), "%lld", [obj longLongValue]);
            append(out, text);
        }
        return YES;
    }
    if ([obj isKindOfClass:[NSArray class]]) {
        NSArray *array = obj;
        NSUInteger count = [array count], i = 0;

        if (out != nil)
            append(out, "[");
        for (id element in array) {
            if (out != nil)
                newline(out, options, level + 1);
            if (!writeValue(out, element, options, level + 1, why))
                return NO;
            if (out != nil && ++i < count)
                append(out, ",");
        }
        if (out != nil) {
            if (count > 0)
                newline(out, options, level);
            append(out, "]");
        }
        return YES;
    }
    if ([obj isKindOfClass:[NSDictionary class]]) {
        NSDictionary *dict = obj;
        NSArray *keys = [dict allKeys];
        NSUInteger count = [keys count], i = 0;

        for (id key in keys) {
            if (![key isKindOfClass:[NSString class]]) {
                *why = @"Invalid (non-string) key in JSON dictionary";
                return NO;
            }
        }
        if (options & NSJSONWritingSortedKeys)
            keys = [keys sortedArrayUsingSelector:@selector(compare:)];
        if (out != nil)
            append(out, "{");
        for (NSString *key in keys) {
            if (out != nil) {
                newline(out, options, level + 1);
                writeString(out, key, options);
                append(out, (options & NSJSONWritingPrettyPrinted) ? " : " : ":");
            }
            if (!writeValue(out, dict[key], options, level + 1, why))
                return NO;
            if (out != nil && ++i < count)
                append(out, ",");
        }
        if (out != nil) {
            if (count > 0)
                newline(out, options, level);
            append(out, "}");
        }
        return YES;
    }
    *why = [NSString stringWithFormat:@"Invalid type in JSON write (%@)", [obj class]];
    return NO;
}

static BOOL
isContainer(id obj)
{
    return [obj isKindOfClass:[NSArray class]] || [obj isKindOfClass:[NSDictionary class]];
}

static NSData *
serialize(id obj, NSJSONWritingOptions options)
{
    NSMutableData *out;
    NSString *why = nil;

    if (obj == nil)
        [NSException raise:NSInvalidArgumentException format:@"JSON write of a nil object"];
    if (!(options & NSJSONWritingFragmentsAllowed) && !isContainer(obj))
        [NSException raise:NSInvalidArgumentException format:@"Invalid top-level type in JSON write"];
    // Check the whole object before writing any of it.
    if (!writeValue(nil, obj, options, 0, &why))
        [NSException raise:NSInvalidArgumentException format:@"%@", why];
    out = [NSMutableData data];
    writeValue(out, obj, options, 0, &why);
    return out;
}

@implementation NSJSONSerialization

+ (BOOL)isValidJSONObject:(id)obj
{
    NSString *why = nil;

    return obj != nil && isContainer(obj) && writeValue(nil, obj, 0, 0, &why);
}

+ (NSData *)dataWithJSONObject:(id)obj options:(NSJSONWritingOptions)opt error:(NSError **)error
{
    (void)error;    // invalid objects raise; there is nothing else to fail
    return [serialize(obj, opt) copy];
}

+ (id)JSONObjectWithData:(NSData *)data options:(NSJSONReadingOptions)opt error:(NSError **)error
{
    return parseData(data, opt, error);
}

+ (NSInteger)writeJSONObject:(id)obj toStream:(NSOutputStream *)stream
                     options:(NSJSONWritingOptions)opt error:(NSError **)error
{
    NSData *data = serialize(obj, opt);
    const uint8_t *bytes = [data bytes];
    NSUInteger length = [data length], written = 0;

    while (written < length) {
        NSInteger n = [stream write:bytes + written maxLength:length - written];
        if (n <= 0) {
            if (error != NULL) {
                NSError *streamError = [stream streamError];
                *error = streamError != nil ? streamError
                    : [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileWriteUnknownError userInfo:nil];
            }
            return 0;
        }
        written += (NSUInteger)n;
    }
    return (NSInteger)written;
}

+ (id)JSONObjectWithStream:(NSInputStream *)stream options:(NSJSONReadingOptions)opt
                     error:(NSError **)error
{
    NSMutableData *data = [NSMutableData data];
    uint8_t chunk[4096];

    for (;;) {
        NSInteger n = [stream read:chunk maxLength:sizeof(chunk)];
        if (n == 0)
            break;
        if (n < 0) {
            if (error != NULL) {
                NSError *streamError = [stream streamError];
                *error = streamError != nil ? streamError
                    : jsonError(NSPropertyListReadCorruptError, @"The stream could not be read.");
            }
            return nil;
        }
        [data appendBytes:chunk length:(NSUInteger)n];
    }
    return parseData(data, opt, error);
}

@end
