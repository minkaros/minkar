/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <CoreGraphics/CGPDFObject.h>
#import <CoreGraphics/CGPDFArray.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN

COREGRAPHICS_EXPORT size_t CGPDFDictionaryGetCount(CGPDFDictionaryRef _Nullable dict);
COREGRAPHICS_EXPORT bool CGPDFDictionaryGetObject(CGPDFDictionaryRef _Nullable dict, const char *key, CGPDFObjectRef _Nullable * _Nullable value);
COREGRAPHICS_EXPORT bool CGPDFDictionaryGetBoolean(CGPDFDictionaryRef _Nullable dict, const char *key, CGPDFBoolean * _Nullable value);
COREGRAPHICS_EXPORT bool CGPDFDictionaryGetInteger(CGPDFDictionaryRef _Nullable dict, const char *key, CGPDFInteger * _Nullable value);
/* An integer or a real. */
COREGRAPHICS_EXPORT bool CGPDFDictionaryGetNumber(CGPDFDictionaryRef _Nullable dict, const char *key, CGPDFReal * _Nullable value);
COREGRAPHICS_EXPORT bool CGPDFDictionaryGetName(CGPDFDictionaryRef _Nullable dict, const char *key, const char * _Nullable * _Nullable value);
COREGRAPHICS_EXPORT bool CGPDFDictionaryGetString(CGPDFDictionaryRef _Nullable dict, const char *key, CGPDFStringRef _Nullable * _Nullable value);
COREGRAPHICS_EXPORT bool CGPDFDictionaryGetArray(CGPDFDictionaryRef _Nullable dict, const char *key, CGPDFArrayRef _Nullable * _Nullable value);
COREGRAPHICS_EXPORT bool CGPDFDictionaryGetDictionary(CGPDFDictionaryRef _Nullable dict, const char *key, CGPDFDictionaryRef _Nullable * _Nullable value);
COREGRAPHICS_EXPORT bool CGPDFDictionaryGetStream(CGPDFDictionaryRef _Nullable dict, const char *key, CGPDFStreamRef _Nullable * _Nullable value);

typedef void (*CGPDFDictionaryApplierFunction)(const char *key, CGPDFObjectRef value, void * _Nullable info);
/* Every entry, its value looked up through a reference. */
COREGRAPHICS_EXPORT void CGPDFDictionaryApplyFunction(CGPDFDictionaryRef _Nullable dict, CGPDFDictionaryApplierFunction _Nullable function, void * _Nullable info);

CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
