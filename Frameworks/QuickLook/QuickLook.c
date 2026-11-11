/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#include "QuickLook.h"
#include <CoreFoundation/CFRuntime.h>
#include <dispatch/dispatch.h>
#include <stdatomic.h>

const CFStringRef kQLPreviewPropertyDisplayNameKey = CFSTR("DisplayName");
const CFStringRef kQLPreviewPropertyWidthKey = CFSTR("Width");
const CFStringRef kQLPreviewPropertyHeightKey = CFSTR("Height");
const CFStringRef kQLPreviewPropertyStringEncodingKey = CFSTR("StringEncoding");
const CFStringRef kQLPreviewPropertyMIMETypeKey = CFSTR("MimeType");
const CFStringRef kQLPreviewPropertyTextEncodingNameKey = CFSTR("TextEncoding");
const CFStringRef kQLPreviewPropertyAttachmentsKey = CFSTR("Attachments");
const CFStringRef kQLPreviewOptionCursorKey = CFSTR("Cursor");
const CFStringRef kQLThumbnailOptionIconModeKey = CFSTR("IconMode");
const CFStringRef kQLThumbnailOptionScaleFactorKey = CFSTR("QLThumbnailOptionScaleFactor");
const CFStringRef kQLThumbnailPropertyExtensionKey = CFSTR("QLThumbnailPropertyExtensionKey");
const CFStringRef kQLPreviewPropertyAttachmentDataKey = CFSTR("AttachmentData");
const CFStringRef kQLPreviewContentIDScheme = CFSTR("cid");
const CFStringRef kQLThumbnailPropertyIconFlavorKey = CFSTR("IconFlavor");

/* Both requests: the item, its type, options, the generator, cancelled,
   and the answer (data or an image) with its type and properties. */
struct __QLRequest {
    CFRuntimeBase base;
    CFURLRef url;
    CFStringRef contentType;
    CFDictionaryRef options;
    CFBundleRef generator;
    CGSize maximumSize;
    _Atomic Boolean cancelled;
    CFDataRef data;
    CFStringRef dataType;
    CFDictionaryRef properties;
    CGImageRef image;
    CGContextRef context;
};
struct __QLPreviewRequest { struct __QLRequest r; };
struct __QLThumbnailRequest { struct __QLRequest r; };

#define RELEASE(x) do { if ((x) != NULL) { CFRelease(x); (x) = NULL; } } while (0)

static void requestFinalize(CFTypeRef cf) {
    struct __QLRequest *r = (struct __QLRequest *)cf;
    RELEASE(r->url); RELEASE(r->contentType); RELEASE(r->options); RELEASE(r->generator);
    RELEASE(r->data); RELEASE(r->dataType); RELEASE(r->properties);
    CGImageRelease(r->image);
    CGContextRelease(r->context);
}

static const CFRuntimeClass previewClass = { 0, "QLPreviewRequest", NULL, NULL, requestFinalize, NULL, NULL, NULL, NULL };
static const CFRuntimeClass thumbnailClass = { 0, "QLThumbnailRequest", NULL, NULL, requestFinalize, NULL, NULL, NULL, NULL };
static CFTypeID previewID, thumbnailID;

CFTypeID QLPreviewRequestGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ previewID = _CFRuntimeRegisterClass(&previewClass); });
    return previewID;
}

CFTypeID QLThumbnailRequestGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ thumbnailID = _CFRuntimeRegisterClass(&thumbnailClass); });
    return thumbnailID;
}

static struct __QLRequest *create(CFTypeID type, CFURLRef url, CFStringRef contentType, CFDictionaryRef options, CFBundleRef generator) {
    struct __QLRequest *r = (struct __QLRequest *)_CFRuntimeCreateInstance(NULL, type,
        sizeof(struct __QLRequest) - sizeof(CFRuntimeBase), NULL);
    if (r == NULL)
        return NULL;
    r->url = url != NULL ? CFRetain(url) : NULL;
    r->contentType = contentType != NULL ? CFRetain(contentType) : NULL;
    r->options = options != NULL ? CFRetain(options) : NULL;
    r->generator = generator != NULL ? (CFBundleRef)CFRetain(generator) : NULL;
    return r;
}

QLPreviewRequestRef _QLPreviewRequestCreate(CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options, CFBundleRef generator) {
    return (QLPreviewRequestRef)create(QLPreviewRequestGetTypeID(), url, contentTypeUTI, options, generator);
}

QLThumbnailRequestRef _QLThumbnailRequestCreate(CFURLRef url, CFStringRef contentTypeUTI, CGSize maximumSize,
                                                CFDictionaryRef options, CFBundleRef generator) {
    struct __QLRequest *r = create(QLThumbnailRequestGetTypeID(), url, contentTypeUTI, options, generator);
    if (r != NULL)
        r->maximumSize = maximumSize;
    return (QLThumbnailRequestRef)r;
}

void _QLRequestCancel(CFTypeRef request) {
    ((struct __QLRequest *)request)->cancelled = true;
}

#define R(x) ((struct __QLRequest *)(x))
#define COPY(x) ((x) != NULL ? CFRetain(x) : NULL)

CFURLRef QLPreviewRequestCopyURL(QLPreviewRequestRef p) { return COPY(R(p)->url); }
CFStringRef QLPreviewRequestCopyContentUTI(QLPreviewRequestRef p) { return COPY(R(p)->contentType); }
CFDictionaryRef QLPreviewRequestCopyOptions(QLPreviewRequestRef p) { return COPY(R(p)->options); }
CFBundleRef QLPreviewRequestGetGeneratorBundle(QLPreviewRequestRef p) { return R(p)->generator; }
Boolean QLPreviewRequestIsCancelled(QLPreviewRequestRef p) { return R(p)->cancelled; }

CFURLRef QLThumbnailRequestCopyURL(QLThumbnailRequestRef t) { return COPY(R(t)->url); }
CFStringRef QLThumbnailRequestCopyContentUTI(QLThumbnailRequestRef t) { return COPY(R(t)->contentType); }
CFDictionaryRef QLThumbnailRequestCopyOptions(QLThumbnailRequestRef t) { return COPY(R(t)->options); }
CGSize QLThumbnailRequestGetMaximumSize(QLThumbnailRequestRef t) { return R(t)->maximumSize; }
CFBundleRef QLThumbnailRequestGetGeneratorBundle(QLThumbnailRequestRef t) { return R(t)->generator; }
Boolean QLThumbnailRequestIsCancelled(QLThumbnailRequestRef t) { return R(t)->cancelled; }

void QLPreviewRequestSetDataRepresentation(QLPreviewRequestRef p, CFDataRef data, CFStringRef contentTypeUTI, CFDictionaryRef properties) {
    struct __QLRequest *r = R(p);
    RELEASE(r->data); RELEASE(r->dataType); RELEASE(r->properties);
    r->data = COPY(data);
    r->dataType = COPY(contentTypeUTI);
    r->properties = COPY(properties);
}

void QLPreviewRequestSetURLRepresentation(QLPreviewRequestRef p, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef properties) {
    CFDataRef data = NULL;
    CFReadStreamRef stream = url != NULL ? CFReadStreamCreateWithFile(NULL, url) : NULL;
    if (stream != NULL && CFReadStreamOpen(stream)) {
        CFMutableDataRef bytes = CFDataCreateMutable(NULL, 0);
        UInt8 buffer[16384];
        CFIndex n;
        while ((n = CFReadStreamRead(stream, buffer, sizeof(buffer))) > 0)
            CFDataAppendBytes(bytes, buffer, n);
        CFReadStreamClose(stream);
        data = bytes;
    }
    if (stream != NULL)
        CFRelease(stream);
    QLPreviewRequestSetDataRepresentation(p, data, contentTypeUTI, properties);
    if (data != NULL)
        CFRelease(data);
}

static CGContextRef createContext(struct __QLRequest *r, CGSize size) {
    size_t w = (size_t)(size.width > 0 ? size.width : 1), h = (size_t)(size.height > 0 ? size.height : 1);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(NULL, w, h, 8, w * 4, space, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(space);
    CGContextRelease(r->context);
    r->context = CGContextRetain(context);
    return context;
}

CGContextRef QLPreviewRequestCreateContext(QLPreviewRequestRef p, CGSize size, Boolean isBitmap, CFDictionaryRef properties) {
    return createContext(R(p), size);
}

CGContextRef QLThumbnailRequestCreateContext(QLThumbnailRequestRef t, CGSize size, Boolean isBitmap, CFDictionaryRef properties) {
    return createContext(R(t), size);
}

/* What was drawn into the request's context becomes its image. */
static void flush(struct __QLRequest *r, CGContextRef context) {
    if (context == NULL || context != r->context)
        return;
    CGImageRelease(r->image);
    r->image = CGBitmapContextCreateImage(context);
}

void QLPreviewRequestFlushContext(QLPreviewRequestRef p, CGContextRef context) { flush(R(p), context); }
void QLThumbnailRequestFlushContext(QLThumbnailRequestRef t, CGContextRef context) { flush(R(t), context); }

void QLThumbnailRequestSetImage(QLThumbnailRequestRef t, CGImageRef image, CFDictionaryRef properties) {
    struct __QLRequest *r = R(t);
    CGImageRetain(image);
    CGImageRelease(r->image);
    r->image = image;
    RELEASE(r->properties);
    r->properties = COPY(properties);
}

void QLThumbnailRequestSetImageAtURL(QLThumbnailRequestRef t, CFURLRef url, CFDictionaryRef properties) {
    CGImageSourceRef source = url != NULL ? CGImageSourceCreateWithURL(url, NULL) : NULL;
    CGImageRef image = source != NULL ? CGImageSourceCreateImageAtIndex(source, 0, NULL) : NULL;
    QLThumbnailRequestSetImage(t, image, properties);
    CGImageRelease(image);
    if (source != NULL)
        CFRelease(source);
}

CFDataRef _QLPreviewRequestCopyData(QLPreviewRequestRef p, CFStringRef *contentTypeUTI, CFDictionaryRef *properties) {
    struct __QLRequest *r = R(p);
    if (contentTypeUTI != NULL) *contentTypeUTI = COPY(r->dataType);
    if (properties != NULL) *properties = COPY(r->properties);
    return COPY(r->data);
}

CGImageRef _QLThumbnailRequestCopyImage(QLThumbnailRequestRef t) {
    return CGImageRetain(R(t)->image);
}

/* Applications: a document's thumbnail. There is no Quick Look host to
   ask a generator yet. */
CGImageRef QLThumbnailImageCreate(CFAllocatorRef allocator, CFURLRef url, CGSize maxThumbnailSize, CFDictionaryRef options) {
    (void)allocator;
    (void)url;
    (void)maxThumbnailSize;
    (void)options;
    return NULL;
}
