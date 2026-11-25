/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * vImage's conversions (vImage.h), row by row.
 */

#include "vImage.h"
#include <math.h>

static vImage_Error check(const vImage_Buffer *src, const vImage_Buffer *dest) {
    if (src == NULL || dest == NULL || src->data == NULL || dest->data == NULL)
        return kvImageNullPointerArgument;
    if (src->width != dest->width || src->height != dest->height)
        return kvImageBufferSizeMismatch;
    return kvImageNoError;
}

#define CONVERT(NAME, TIN, TOUT, EXPR)                                                                     \
    vImage_Error NAME(const vImage_Buffer *src, const vImage_Buffer *dest, vImage_Flags flags) {            \
        vImage_Error e = check(src, dest);                                                                  \
        if (e != kvImageNoError)                                                                            \
            return e;                                                                                       \
        for (vImagePixelCount row = 0; row < src->height; row++) {                                          \
            const TIN *in = (const TIN *)((const char *)src->data + row * src->rowBytes);                   \
            TOUT *out = (TOUT *)((char *)dest->data + row * dest->rowBytes);                                \
            for (vImagePixelCount i = 0; i < src->width; i++) {                                             \
                TIN x = in[i];                                                                              \
                out[i] = (TOUT)(EXPR);                                                                      \
            }                                                                                               \
        }                                                                                                   \
        return kvImageNoError;                                                                              \
    }

static inline uint16_t toUnsigned(float v) {
    v = v * 65535.0f + 0.5f;
    return v <= 0 ? 0 : v >= 65535.0f ? 65535 : (uint16_t)v;
}

CONVERT(vImageConvert_16Uto16F, uint16_t, _Float16, (float)x / 65535.0f)
CONVERT(vImageConvert_16Fto16U, _Float16, uint16_t, toUnsigned((float)x))
CONVERT(vImageConvert_Planar16FtoPlanarF, _Float16, float, x)
CONVERT(vImageConvert_PlanarFtoPlanar16F, float, _Float16, x)
