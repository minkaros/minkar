/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <CoreGraphics/CGPDFObject.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN

typedef struct O2PDFArray *CGPDFArrayRef;
typedef struct O2PDFDictionary *CGPDFDictionaryRef;
typedef struct O2PDFStream *CGPDFStreamRef;
typedef struct O2PDFString *CGPDFStringRef;

COREGRAPHICS_EXPORT size_t CGPDFArrayGetCount(CGPDFArrayRef _Nullable array);
COREGRAPHICS_EXPORT bool CGPDFArrayGetObject(CGPDFArrayRef _Nullable array, size_t index, CGPDFObjectRef _Nullable * _Nullable value);
COREGRAPHICS_EXPORT bool CGPDFArrayGetNull(CGPDFArrayRef _Nullable array, size_t index);
COREGRAPHICS_EXPORT bool CGPDFArrayGetBoolean(CGPDFArrayRef _Nullable array, size_t index, CGPDFBoolean * _Nullable value);
COREGRAPHICS_EXPORT bool CGPDFArrayGetInteger(CGPDFArrayRef _Nullable array, size_t index, CGPDFInteger * _Nullable value);
/* An integer or a real. */
COREGRAPHICS_EXPORT bool CGPDFArrayGetNumber(CGPDFArrayRef _Nullable array, size_t index, CGPDFReal * _Nullable value);
COREGRAPHICS_EXPORT bool CGPDFArrayGetName(CGPDFArrayRef _Nullable array, size_t index, const char * _Nullable * _Nullable value);
COREGRAPHICS_EXPORT bool CGPDFArrayGetString(CGPDFArrayRef _Nullable array, size_t index, CGPDFStringRef _Nullable * _Nullable value);
COREGRAPHICS_EXPORT bool CGPDFArrayGetArray(CGPDFArrayRef _Nullable array, size_t index, CGPDFArrayRef _Nullable * _Nullable value);
COREGRAPHICS_EXPORT bool CGPDFArrayGetDictionary(CGPDFArrayRef _Nullable array, size_t index, CGPDFDictionaryRef _Nullable * _Nullable value);
COREGRAPHICS_EXPORT bool CGPDFArrayGetStream(CGPDFArrayRef _Nullable array, size_t index, CGPDFStreamRef _Nullable * _Nullable value);

CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
