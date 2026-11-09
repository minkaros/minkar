/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The calls a Quick Look generator makes on the preview and thumbnail
 * requests its host hands it. There is no Quick Look host yet; the
 * requests are made with _QLPreviewRequestCreate and
 * _QLThumbnailRequestCreate (private), which a host will use.
 */

#ifndef __QUICKLOOK__
#define __QUICKLOOK__

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __QLPreviewRequest *QLPreviewRequestRef;
typedef struct __QLThumbnailRequest *QLThumbnailRequestRef;

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

#endif
