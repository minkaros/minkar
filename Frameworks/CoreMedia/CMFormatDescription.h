/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef __CMFORMATDESCRIPTION__
#define __CMFORMATDESCRIPTION__

#include <CoreMedia/CMBase.h>

typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;
typedef CMFormatDescriptionRef CMVideoFormatDescriptionRef;
typedef CMFormatDescriptionRef CMAudioFormatDescriptionRef;

typedef FourCharCode CMMediaType;
typedef FourCharCode CMVideoCodecType;

enum {
    kCMMediaType_Video = 'vide',
    kCMMediaType_Audio = 'soun',
    kCMMediaType_Text = 'text',
};

enum {
    kCMVideoCodecType_H264 = 'avc1',
    kCMVideoCodecType_HEVC = 'hvc1',
    kCMVideoCodecType_VP9 = 'vp09',
    kCMVideoCodecType_AV1 = 'av01',
};

typedef struct {
    int32_t width;
    int32_t height;
} CMVideoDimensions;

/* Format description extensions. */
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_ColorPrimaries;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_TransferFunction;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_YCbCrMatrix;
CM_EXPORT const CFStringRef kCMFormatDescriptionExtension_FullRangeVideo;

#endif
