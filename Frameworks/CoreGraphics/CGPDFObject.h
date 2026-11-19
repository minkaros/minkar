/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/* A PDF document's objects, as read: each function looks through a
   reference to the object it names. */

#import <CoreGraphics/CoreGraphicsExport.h>
#import <CoreGraphics/CGGeometry.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN

typedef unsigned char CGPDFBoolean;
typedef long CGPDFInteger;
typedef CGFloat CGPDFReal;

typedef struct O2PDFObject *CGPDFObjectRef;

typedef CF_ENUM(int32_t, CGPDFObjectType) {
    kCGPDFObjectTypeNull = 1,
    kCGPDFObjectTypeBoolean,
    kCGPDFObjectTypeInteger,
    kCGPDFObjectTypeReal,
    kCGPDFObjectTypeName,
    kCGPDFObjectTypeString,
    kCGPDFObjectTypeArray,
    kCGPDFObjectTypeDictionary,
    kCGPDFObjectTypeStream
};

COREGRAPHICS_EXPORT CGPDFObjectType CGPDFObjectGetType(CGPDFObjectRef _Nullable object);
/* value points to the type's value: a CGPDFBoolean, CGPDFInteger,
   CGPDFReal (an integer too), const char * (a name) or the object's ref. */
COREGRAPHICS_EXPORT bool CGPDFObjectGetValue(CGPDFObjectRef _Nullable object, CGPDFObjectType type, void * _Nullable value);

CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
