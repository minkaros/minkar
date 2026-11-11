/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Quick Look generators: plug-ins (CFPlugIn, of type kQLGeneratorTypeID)
 * that make a document's thumbnail and preview, and the calls they make
 * on the requests their host hands them. There is no Quick Look host
 * yet; the requests are made with _QLPreviewRequestCreate and
 * _QLThumbnailRequestCreate (private), which a host will use.
 */

#ifndef __QLGENERATOR__
#define __QLGENERATOR__

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPlugInCOM.h>
#include <CoreGraphics/CoreGraphics.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __QLPreviewRequest *QLPreviewRequestRef;
typedef struct __QLThumbnailRequest *QLThumbnailRequestRef;

/* The plug-in type and its interface (the identifiers generators use). */
#define kQLGeneratorTypeID CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault, 0x5E, 0x2D, 0x96, 0x80, 0x50, 0x22, \
    0x40, 0xFA, 0xB8, 0x06, 0x43, 0x34, 0x96, 0x22, 0xE5, 0xB9)
#define kQLGeneratorCallbacksInterfaceID CFUUIDGetConstantUUIDWithBytes(kCFAllocatorDefault, 0x86, 0x5A, 0xF5, 0xE0, \
    0x6D, 0x30, 0x43, 0x45, 0x95, 0x1B, 0xD3, 0x71, 0x05, 0x75, 0x4F, 0x2D)

/* A generator's interface: IUnknown's calls, then the generator's. */
typedef struct {
    IUNKNOWN_C_GUTS;
    OSStatus (*GenerateThumbnailForURL)(void *thisInterface, QLThumbnailRequestRef thumbnail, CFURLRef url,
                                        CFStringRef contentTypeUTI, CFDictionaryRef options, CGSize maxSize);
    void (*CancelThumbnailGeneration)(void *thisInterface, QLThumbnailRequestRef thumbnail);
    OSStatus (*GeneratePreviewForURL)(void *thisInterface, QLPreviewRequestRef preview, CFURLRef url,
                                      CFStringRef contentTypeUTI, CFDictionaryRef options);
    void (*CancelPreviewGeneration)(void *thisInterface, QLPreviewRequestRef preview);
} QLGeneratorInterfaceStruct;

CF_EXPORT const CFStringRef kQLPreviewPropertyDisplayNameKey;
CF_EXPORT const CFStringRef kQLPreviewPropertyWidthKey;
CF_EXPORT const CFStringRef kQLPreviewPropertyHeightKey;
CF_EXPORT const CFStringRef kQLPreviewPropertyStringEncodingKey;
CF_EXPORT const CFStringRef kQLPreviewPropertyMIMETypeKey;
CF_EXPORT const CFStringRef kQLPreviewPropertyTextEncodingNameKey;
CF_EXPORT const CFStringRef kQLPreviewPropertyAttachmentsKey;
CF_EXPORT const CFStringRef kQLPreviewOptionCursorKey;
CF_EXPORT const CFStringRef kQLThumbnailOptionIconModeKey;
CF_EXPORT const CFStringRef kQLThumbnailOptionScaleFactorKey;
CF_EXPORT const CFStringRef kQLThumbnailPropertyExtensionKey;
CF_EXPORT const CFStringRef kQLPreviewPropertyAttachmentDataKey;
CF_EXPORT const CFStringRef kQLPreviewContentIDScheme;
CF_EXPORT const CFStringRef kQLThumbnailPropertyIconFlavorKey;

CFTypeID QLPreviewRequestGetTypeID(void);
CFURLRef QLPreviewRequestCopyURL(QLPreviewRequestRef preview);
CFStringRef QLPreviewRequestCopyContentUTI(QLPreviewRequestRef preview);
CFDictionaryRef QLPreviewRequestCopyOptions(QLPreviewRequestRef preview);
CFBundleRef QLPreviewRequestGetGeneratorBundle(QLPreviewRequestRef preview);
Boolean QLPreviewRequestIsCancelled(QLPreviewRequestRef preview);
void QLPreviewRequestSetDataRepresentation(QLPreviewRequestRef preview, CFDataRef data, CFStringRef contentTypeUTI,
                                           CFDictionaryRef properties);
void QLPreviewRequestSetURLRepresentation(QLPreviewRequestRef preview, CFURLRef url, CFStringRef contentTypeUTI,
                                          CFDictionaryRef properties);
CGContextRef QLPreviewRequestCreateContext(QLPreviewRequestRef preview, CGSize size, Boolean isBitmap, CFDictionaryRef properties);
void QLPreviewRequestFlushContext(QLPreviewRequestRef preview, CGContextRef context);

CFTypeID QLThumbnailRequestGetTypeID(void);
CFURLRef QLThumbnailRequestCopyURL(QLThumbnailRequestRef thumbnail);
CFStringRef QLThumbnailRequestCopyContentUTI(QLThumbnailRequestRef thumbnail);
CFDictionaryRef QLThumbnailRequestCopyOptions(QLThumbnailRequestRef thumbnail);
CGSize QLThumbnailRequestGetMaximumSize(QLThumbnailRequestRef thumbnail);
CFBundleRef QLThumbnailRequestGetGeneratorBundle(QLThumbnailRequestRef thumbnail);
Boolean QLThumbnailRequestIsCancelled(QLThumbnailRequestRef thumbnail);
CGContextRef QLThumbnailRequestCreateContext(QLThumbnailRequestRef thumbnail, CGSize size, Boolean isBitmap,
                                             CFDictionaryRef properties);
void QLThumbnailRequestFlushContext(QLThumbnailRequestRef thumbnail, CGContextRef context);
void QLThumbnailRequestSetImage(QLThumbnailRequestRef thumbnail, CGImageRef image, CFDictionaryRef properties);
void QLThumbnailRequestSetImageAtURL(QLThumbnailRequestRef thumbnail, CFURLRef url, CFDictionaryRef properties);

/* The host's side (private): requests for a file, cancelling them, and
   what the generator gave back. */
QLPreviewRequestRef _QLPreviewRequestCreate(CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options, CFBundleRef generator);
CFDataRef _QLPreviewRequestCopyData(QLPreviewRequestRef preview, CFStringRef *contentTypeUTI, CFDictionaryRef *properties);
QLThumbnailRequestRef _QLThumbnailRequestCreate(CFURLRef url, CFStringRef contentTypeUTI, CGSize maximumSize,
                                                CFDictionaryRef options, CFBundleRef generator);
CGImageRef _QLThumbnailRequestCopyImage(QLThumbnailRequestRef thumbnail);
void _QLRequestCancel(CFTypeRef request);

#ifdef __cplusplus
}
#endif

#endif /* __QLGENERATOR__ */
