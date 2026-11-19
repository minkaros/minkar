/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <CoreGraphics/CGPDFObject.h>
#import <CoreGraphics/CGPDFDictionary.h>
#import <CoreFoundation/CFData.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN

typedef CF_ENUM(int32_t, CGPDFDataFormat) {
    CGPDFDataFormatRaw,
    CGPDFDataFormatJPEGEncoded,
    CGPDFDataFormatJPEG2000
};

COREGRAPHICS_EXPORT CGPDFDictionaryRef _Nullable CGPDFStreamGetDictionary(CGPDFStreamRef _Nullable stream);
/* Its data decoded; JPEG and JPEG 2000 images' left encoded, so said. */
COREGRAPHICS_EXPORT CFDataRef _Nullable CGPDFStreamCopyData(CGPDFStreamRef _Nullable stream, CGPDFDataFormat * _Nullable format);

CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
