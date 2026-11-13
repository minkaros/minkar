/* Copyright (c) 2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CoreGraphicsExport.h>
#import <CoreFoundation/CFData.h>
#import <CoreFoundation/CFURL.h>

/* CF's naming rules for what the functions return (Create, Copy). */
CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN

typedef struct O2DataProvider *CGDataProviderRef;

typedef void (*CGDataProviderReleaseDataCallback)(void * _Nullable info, const void *data, size_t size);

COREGRAPHICS_EXPORT CFTypeID CGDataProviderGetTypeID(void);
COREGRAPHICS_EXPORT CGDataProviderRef _Nullable CGDataProviderRetain(CGDataProviderRef _Nullable provider);
COREGRAPHICS_EXPORT void CGDataProviderRelease(CGDataProviderRef _Nullable provider);

COREGRAPHICS_EXPORT CGDataProviderRef _Nullable CGDataProviderCreateWithCFData(CFDataRef _Nullable data);
COREGRAPHICS_EXPORT CGDataProviderRef _Nullable CGDataProviderCreateWithURL(CFURLRef _Nullable url);
COREGRAPHICS_EXPORT CGDataProviderRef _Nullable CGDataProviderCreateWithFilename(const char * _Nullable filename);

COREGRAPHICS_EXPORT CGDataProviderRef _Nullable CGDataProviderCreateWithData(void * _Nullable info, const void * _Nullable data, size_t size, CGDataProviderReleaseDataCallback _Nullable releaseCallback);

COREGRAPHICS_EXPORT CFDataRef _Nullable CGDataProviderCopyData(CGDataProviderRef self);


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED