/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A color space's ICC profile. Minkar draws every RGB space as sRGB and
 * every gray one with sRGB's tone curve, so their profiles are those: a
 * version 4 display profile built from sRGB's primaries (adapted to the
 * D50 white of the profile connection space) and its parametric curve.
 */

#include <CoreGraphics/CGColorSpace.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *bytes;
    size_t length, capacity;
} Buffer;

static void put(Buffer *b, const void *data, size_t n) {
    if (b->length + n > b->capacity) {
        size_t c = b->capacity ? b->capacity * 2 : 1024;
        while (c < b->length + n)
            c *= 2;
        b->bytes = realloc(b->bytes, c);
        b->capacity = c;
    }
    memcpy(b->bytes + b->length, data, n);
    b->length += n;
}

static void put32(Buffer *b, uint32_t v) {
    uint8_t x[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
    put(b, x, 4);
}

static void put16(Buffer *b, uint16_t v) {
    uint8_t x[2] = { (uint8_t)(v >> 8), (uint8_t)v };
    put(b, x, 2);
}

static void putFixed(Buffer *b, double v) {
    put32(b, (uint32_t)(int32_t)(v * 65536.0 + (v < 0 ? -0.5 : 0.5)));
}

static void align4(Buffer *b) {
    while (b->length % 4)
        put(b, "", 1);
}

static void set32(Buffer *b, size_t at, uint32_t v) {
    b->bytes[at] = (uint8_t)(v >> 24);
    b->bytes[at + 1] = (uint8_t)(v >> 16);
    b->bytes[at + 2] = (uint8_t)(v >> 8);
    b->bytes[at + 3] = (uint8_t)v;
}

/* A multiLocalizedUnicode text, in English. */
static void putText(Buffer *b, const char *text) {
    size_t n = strlen(text);
    put32(b, 'mluc');
    put32(b, 0);
    put32(b, 1);
    put32(b, 12);
    put16(b, 'e' << 8 | 'n');
    put16(b, 'U' << 8 | 'S');
    put32(b, (uint32_t)(n * 2));
    put32(b, 28);
    for (size_t i = 0; i < n; i++)
        put16(b, (uint8_t)text[i]);
}

static void putXYZ(Buffer *b, double x, double y, double z) {
    put32(b, 'XYZ ');
    put32(b, 0);
    putFixed(b, x);
    putFixed(b, y);
    putFixed(b, z);
}

/* sRGB's curve: (a x + b)^g above d, c x below. */
static void putCurve(Buffer *b) {
    put32(b, 'para');
    put32(b, 0);
    put16(b, 3);
    put16(b, 0);
    putFixed(b, 2.4);
    putFixed(b, 1 / 1.055);
    putFixed(b, 0.055 / 1.055);
    putFixed(b, 1 / 12.92);
    putFixed(b, 0.04045);
}

typedef struct {
    uint32_t signature;
    int kind;		/* which element: see writeElement */
    double x, y, z;
    const char *text;
} Tag;

enum { kText, kXYZ, kCurve, kMatrix };

static void writeElement(Buffer *b, const Tag *t) {
    switch (t->kind) {
    case kText:
        putText(b, t->text);
        break;
    case kXYZ:
        putXYZ(b, t->x, t->y, t->z);
        break;
    case kCurve:
        putCurve(b);
        break;
    case kMatrix: {
        /* Bradford adaptation from sRGB's D65 white to D50. */
        static const double m[9] = { 1.0478, 0.0229, -0.0502, 0.0295, 0.9905, -0.0171, -0.0092, 0.0151, 0.7521 };
        put32(b, 'sf32');
        put32(b, 0);
        for (int i = 0; i < 9; i++)
            putFixed(b, m[i]);
        break;
    }
    }
}

static CFDataRef createProfile(bool rgb) {
    static const Tag rgbTags[] = {
        { 'desc', kText, 0, 0, 0, "sRGB" },
        { 'cprt', kText, 0, 0, 0, "No copyright, use freely" },
        { 'wtpt', kXYZ, 0.9642, 1.0, 0.8249, NULL },
        { 'chad', kMatrix, 0, 0, 0, NULL },
        { 'rXYZ', kXYZ, 0.4361, 0.2225, 0.0139, NULL },
        { 'gXYZ', kXYZ, 0.3851, 0.7169, 0.0971, NULL },
        { 'bXYZ', kXYZ, 0.1431, 0.0606, 0.7141, NULL },
        { 'rTRC', kCurve, 0, 0, 0, NULL },
        { 'gTRC', kCurve, 0, 0, 0, NULL },
        { 'bTRC', kCurve, 0, 0, 0, NULL },
    };
    static const Tag grayTags[] = {
        { 'desc', kText, 0, 0, 0, "Gray (sRGB curve)" },
        { 'cprt', kText, 0, 0, 0, "No copyright, use freely" },
        { 'wtpt', kXYZ, 0.9642, 1.0, 0.8249, NULL },
        { 'kTRC', kCurve, 0, 0, 0, NULL },
    };
    const Tag *tags = rgb ? rgbTags : grayTags;
    size_t count = rgb ? sizeof(rgbTags) / sizeof(rgbTags[0]) : sizeof(grayTags) / sizeof(grayTags[0]);
    Buffer b = { NULL, 0, 0 };

    /* The header. */
    put32(&b, 0);		/* size, set at the end */
    put32(&b, 0);		/* preferred CMM */
    put32(&b, 0x04300000);	/* version 4.3 */
    put32(&b, 'mntr');
    put32(&b, rgb ? 'RGB ' : 'GRAY');
    put32(&b, 'XYZ ');
    static const uint16_t date[6] = { 2026, 1, 1, 0, 0, 0 };
    for (int i = 0; i < 6; i++)
        put16(&b, date[i]);
    put32(&b, 'acsp');
    put32(&b, 0);		/* platform */
    put32(&b, 0);		/* flags */
    put32(&b, 0);		/* manufacturer */
    put32(&b, 0);		/* model */
    put32(&b, 0);
    put32(&b, 0);		/* attributes */
    put32(&b, 0);		/* perceptual intent */
    putFixed(&b, 0.9642);	/* the D50 illuminant */
    putFixed(&b, 1.0);
    putFixed(&b, 0.8249);
    put32(&b, 0);		/* creator */
    for (int i = 0; i < 16 + 28; i++)
        put(&b, "", 1);		/* profile ID and reserved bytes */

    /* The tag table, then the elements; the same curve is written once
       for the three channels. */
    put32(&b, (uint32_t)count);
    size_t table = b.length;
    for (size_t i = 0; i < count; i++) {
        put32(&b, tags[i].signature);
        put32(&b, 0);
        put32(&b, 0);
    }
    size_t curveOffset = 0, curveSize = 0;
    for (size_t i = 0; i < count; i++) {
        size_t offset, size;
        if (tags[i].kind == kCurve && curveOffset != 0) {
            offset = curveOffset;
            size = curveSize;
        } else {
            align4(&b);
            offset = b.length;
            writeElement(&b, &tags[i]);
            size = b.length - offset;
            if (tags[i].kind == kCurve) {
                curveOffset = offset;
                curveSize = size;
            }
        }
        set32(&b, table + 12 * i + 4, (uint32_t)offset);
        set32(&b, table + 12 * i + 8, (uint32_t)size);
    }
    align4(&b);
    set32(&b, 0, (uint32_t)b.length);
    CFDataRef data = CFDataCreate(NULL, b.bytes, (CFIndex)b.length);
    free(b.bytes);
    return data;
}

CFDataRef CGColorSpaceCopyICCData(CGColorSpaceRef space) {
    if (space == NULL)
        return NULL;
    switch (CGColorSpaceGetModel(space)) {
    case kCGColorSpaceModelRGB:
        return createProfile(true);
    case kCGColorSpaceModelMonochrome:
        return createProfile(false);
    default:
        return NULL;
    }
}
