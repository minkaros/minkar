/* Copyright (c) 2008 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import "CoreGraphicsExport.h"
#import <CoreFoundation/CFDictionary.h>
#import <CoreFoundation/CFURL.h>

typedef struct _O2ImageSource *CGImageSourceRef;

#import "CGImage.h"

/* CF's naming rules for what the functions return (Create, Copy). */
CF_IMPLICIT_BRIDGING_ENABLED

COREGRAPHICS_EXPORT CGImageSourceRef CGImageSourceCreateWithData(CFDataRef data, CFDictionaryRef options);
COREGRAPHICS_EXPORT CGImageSourceRef CGImageSourceCreateWithURL(CFURLRef url, CFDictionaryRef options);

COREGRAPHICS_EXPORT CFStringRef CGImageSourceGetType(CGImageSourceRef self);

COREGRAPHICS_EXPORT size_t CGImageSourceGetCount(CGImageSourceRef self);

COREGRAPHICS_EXPORT CGImageRef CGImageSourceCreateImageAtIndex(CGImageSourceRef self, size_t index, CFDictionaryRef options);
COREGRAPHICS_EXPORT CFDictionaryRef CGImageSourceCopyPropertiesAtIndex(CGImageSourceRef self, size_t index, CFDictionaryRef options);

typedef CF_ENUM(int32_t, CGImageSourceStatus) {
    kCGImageStatusUnexpectedEOF = -5,
    kCGImageStatusInvalidData = -4,
    kCGImageStatusUnknownType = -3,
    kCGImageStatusReadingHeader = -2,
    kCGImageStatusIncomplete = -1,
    kCGImageStatusComplete = 0,
};

COREGRAPHICS_EXPORT const CFStringRef kCGImageSourceTypeIdentifierHint;
COREGRAPHICS_EXPORT const CFStringRef kCGImageSourceShouldCache;
COREGRAPHICS_EXPORT const CFStringRef kCGImageSourceShouldCacheImmediately;
COREGRAPHICS_EXPORT const CFStringRef kCGImageSourceShouldAllowFloat;
COREGRAPHICS_EXPORT const CFStringRef kCGImageSourceCreateThumbnailFromImageIfAbsent;
COREGRAPHICS_EXPORT const CFStringRef kCGImageSourceCreateThumbnailFromImageAlways;
COREGRAPHICS_EXPORT const CFStringRef kCGImageSourceThumbnailMaxPixelSize;
COREGRAPHICS_EXPORT const CFStringRef kCGImageSourceCreateThumbnailWithTransform;
COREGRAPHICS_EXPORT const CFStringRef kCGImageSourceSubsampleFactor;

COREGRAPHICS_EXPORT CFTypeID CGImageSourceGetTypeID(void);
/* The image types sources read (UTIs). */
COREGRAPHICS_EXPORT CFArrayRef CGImageSourceCopyTypeIdentifiers(void);
/* The container's properties (its file size). */
COREGRAPHICS_EXPORT CFDictionaryRef CGImageSourceCopyProperties(CGImageSourceRef self, CFDictionaryRef options);
/* The image at index scaled to fit kCGImageSourceThumbnailMaxPixelSize
   (or as it is). */
COREGRAPHICS_EXPORT CGImageRef CGImageSourceCreateThumbnailAtIndex(CGImageSourceRef self, size_t index, CFDictionaryRef options);
COREGRAPHICS_EXPORT CGImageSourceStatus CGImageSourceGetStatus(CGImageSourceRef self);
COREGRAPHICS_EXPORT CGImageSourceStatus CGImageSourceGetStatusAtIndex(CGImageSourceRef self, size_t index);


CF_IMPLICIT_BRIDGING_DISABLED