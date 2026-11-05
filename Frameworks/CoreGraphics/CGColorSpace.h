/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGGeometry.h>

/* CF's naming rules for what the functions return (Create, Copy). */
CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN

typedef struct O2ColorSpace *CGColorSpaceRef;

typedef CF_ENUM(int32_t, CGColorRenderingIntent) {
    kCGRenderingIntentDefault,
    kCGRenderingIntentAbsoluteColorimetric,
    kCGRenderingIntentRelativeColorimetric,
    kCGRenderingIntentSaturation,
    kCGRenderingIntentPerceptual,
};

typedef CF_ENUM(int32_t, CGColorSpaceModel) {
    kCGColorSpaceModelUnknown = -1,
    kCGColorSpaceModelMonochrome,
    kCGColorSpaceModelRGB,
    kCGColorSpaceModelCMYK,
    kCGColorSpaceModelLab,
    kCGColorSpaceModelDeviceN,
    kCGColorSpaceModelIndexed,
    kCGColorSpaceModelPattern,
};

COREGRAPHICS_EXPORT CGColorSpaceRef _Nullable CGColorSpaceRetain(CGColorSpaceRef _Nullable colorSpace);
COREGRAPHICS_EXPORT void CGColorSpaceRelease(CGColorSpaceRef _Nullable colorSpace);

COREGRAPHICS_EXPORT CGColorSpaceRef CGColorSpaceCreateDeviceRGB(void);
COREGRAPHICS_EXPORT CGColorSpaceRef CGColorSpaceCreateDeviceGray(void);
COREGRAPHICS_EXPORT CGColorSpaceRef CGColorSpaceCreateDeviceCMYK(void);
COREGRAPHICS_EXPORT CGColorSpaceRef _Nullable CGColorSpaceCreatePattern(CGColorSpaceRef _Nullable baseSpace);

COREGRAPHICS_EXPORT CGColorSpaceModel CGColorSpaceGetModel(CGColorSpaceRef self)
    CF_SWIFT_NAME(getter:CGColorSpace.model(self:));
/* An indexed or pattern space's base space; NULL for the others. */
COREGRAPHICS_EXPORT CGColorSpaceRef _Nullable CGColorSpaceGetBaseColorSpace(CGColorSpaceRef space)
    CF_SWIFT_NAME(getter:CGColorSpace.baseColorSpace(self:));
/* An indexed space's colors: how many, and their bytes (count times the
 * base space's components). */
COREGRAPHICS_EXPORT size_t CGColorSpaceGetColorTableCount(CGColorSpaceRef space);
COREGRAPHICS_EXPORT void CGColorSpaceGetColorTable(CGColorSpaceRef space, uint8_t *table);
COREGRAPHICS_EXPORT size_t CGColorSpaceGetNumberOfComponents(CGColorSpaceRef self)
    CF_SWIFT_NAME(getter:CGColorSpace.numberOfComponents(self:));


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED