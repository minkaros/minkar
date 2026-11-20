// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <CoreGraphics/CoreGraphicsExport.h>
#import <CoreGraphics/CGDataProvider.h>
#import <CoreGraphics/CGDataConsumer.h>
#import <CoreFoundation/CoreFoundation.h>

typedef struct CGPSConverter *CGPSConverterRef;

typedef void (*CGPSConverterBeginDocumentCallback)(void *info);
typedef void (*CGPSConverterEndDocumentCallback)(void *info, bool success);
typedef void (*CGPSConverterBeginPageCallback)(void *info, size_t pageNumber, CFDictionaryRef pageInfo);
typedef void (*CGPSConverterEndPageCallback)(void *info, size_t pageNumber, CFDictionaryRef pageInfo);
typedef void (*CGPSConverterProgressCallback)(void *info);
typedef void (*CGPSConverterMessageCallback)(void *info, CFStringRef message);
typedef void (*CGPSConverterReleaseInfoCallback)(void *info);

struct CGPSConverterCallbacks {
    unsigned int version;
    CGPSConverterBeginDocumentCallback beginDocument;
    CGPSConverterEndDocumentCallback endDocument;
    CGPSConverterBeginPageCallback beginPage;
    CGPSConverterEndPageCallback endPage;
    CGPSConverterProgressCallback noteProgress;
    CGPSConverterMessageCallback noteMessage;
    CGPSConverterReleaseInfoCallback releaseInfo;
};
typedef struct CGPSConverterCallbacks CGPSConverterCallbacks;

/* A converter of PostScript to PDF, telling its callbacks how it goes. */
COREGRAPHICS_EXPORT CGPSConverterRef CGPSConverterCreate(void *info, const CGPSConverterCallbacks *callbacks, CFDictionaryRef options);
COREGRAPHICS_EXPORT bool CGPSConverterConvert(CGPSConverterRef converter, CGDataProviderRef provider, CGDataConsumerRef consumer, CFDictionaryRef options);
COREGRAPHICS_EXPORT bool CGPSConverterAbort(CGPSConverterRef converter);
COREGRAPHICS_EXPORT bool CGPSConverterIsConverting(CGPSConverterRef converter);
COREGRAPHICS_EXPORT CFTypeID CGPSConverterGetTypeID(void);
