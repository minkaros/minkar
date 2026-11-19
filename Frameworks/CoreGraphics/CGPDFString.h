/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <CoreGraphics/CGPDFObject.h>
#import <CoreFoundation/CFString.h>
#import <CoreFoundation/CFDate.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN

typedef struct O2PDFString *CGPDFStringRef;

COREGRAPHICS_EXPORT size_t CGPDFStringGetLength(CGPDFStringRef _Nullable string);
COREGRAPHICS_EXPORT const unsigned char * _Nullable CGPDFStringGetBytePtr(CGPDFStringRef _Nullable string);
/* Text: UTF-16 after its byte order mark, else PDFDocEncoding. */
COREGRAPHICS_EXPORT CFStringRef _Nullable CGPDFStringCopyTextString(CGPDFStringRef _Nullable string);
/* A date, D:YYYYMMDDHHmmSSOHH'mm' (the later parts optional; no zone is
   UTC); NULL if the string is not one. */
COREGRAPHICS_EXPORT CFDateRef _Nullable CGPDFStringCopyDate(CGPDFStringRef _Nullable string);

CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
