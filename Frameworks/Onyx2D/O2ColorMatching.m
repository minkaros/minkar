/* Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Onyx2D/O2ColorMatching.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSString.h>
#import <Foundation/NSValue.h>
#include <pthread.h>
#include "lcms2.h"

NSString *const O2ColorProfileSRGB = @"sRGB";
NSString *const O2ColorProfileGenericRGB = @"GenericRGB";
NSString *const O2ColorProfileGenericRGBLinear = @"GenericRGBLinear";
NSString *const O2ColorProfileDisplayP3 = @"DisplayP3";
NSString *const O2ColorProfileAdobeRGB1998 = @"AdobeRGB1998";
NSString *const O2ColorProfileLinearSRGB = @"LinearSRGB";
NSString *const O2ColorProfileITUR709 = @"ITUR709";
NSString *const O2ColorProfileITUR2020 = @"ITUR2020";
NSString *const O2ColorProfileGenericGray = @"GenericGray";
NSString *const O2ColorProfileGenericGrayGamma22 = @"GenericGrayGamma22";
NSString *const O2ColorProfileSRGBGray = @"sRGBGray";
NSString *const O2ColorProfileLinearGray = @"LinearGray";

/* The published primaries (CIE xy) and white points of the spaces. */
static const cmsCIExyY D65 = { 0.3127, 0.3290, 1.0 };
static const cmsCIExyYTRIPLE BT709 = { { 0.640, 0.330, 1.0 }, { 0.300, 0.600, 1.0 }, { 0.150, 0.060, 1.0 } };
static const cmsCIExyYTRIPLE GenericRGBPrimaries = { { 0.630, 0.340, 1.0 }, { 0.295, 0.605, 1.0 }, { 0.155, 0.077, 1.0 } };
static const cmsCIExyYTRIPLE DisplayP3Primaries = { { 0.680, 0.320, 1.0 }, { 0.265, 0.690, 1.0 }, { 0.150, 0.060, 1.0 } };
static const cmsCIExyYTRIPLE AdobeRGBPrimaries = { { 0.640, 0.330, 1.0 }, { 0.210, 0.710, 1.0 }, { 0.150, 0.060, 1.0 } };
static const cmsCIExyYTRIPLE BT2020 = { { 0.708, 0.292, 1.0 }, { 0.170, 0.797, 1.0 }, { 0.131, 0.046, 1.0 } };

static cmsToneCurve *gammaCurve(double gamma) {
    return cmsBuildGamma(NULL, gamma);
}

/* sRGB's curve: linear near black, then a 2.4 power. */
static cmsToneCurve *sRGBCurve(void) {
    const double p[5] = { 2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92, 0.04045 };
    return cmsBuildParametricToneCurve(NULL, 4, p);
}

/* ITU-R BT.709's (and BT.2020's) camera curve. */
static cmsToneCurve *bt709Curve(void) {
    const double p[5] = { 1.0 / 0.45, 1.0 / 1.099, 0.099 / 1.099, 1.0 / 4.5, 0.081 };
    return cmsBuildParametricToneCurve(NULL, 4, p);
}

static cmsHPROFILE rgbProfile(const cmsCIExyYTRIPLE *primaries, cmsToneCurve *curve) {
    cmsToneCurve *curves[3] = { curve, curve, curve };
    cmsHPROFILE profile = cmsCreateRGBProfile(&D65, primaries, curves);

    cmsFreeToneCurve(curve);
    return profile;
}

static cmsHPROFILE grayProfile(cmsToneCurve *curve) {
    cmsHPROFILE profile = cmsCreateGrayProfile(&D65, curve);

    cmsFreeToneCurve(curve);
    return profile;
}

static cmsHPROFILE createProfile(NSString *name) {
    if ([name isEqualToString:O2ColorProfileSRGB])
        return rgbProfile(&BT709, sRGBCurve());
    if ([name isEqualToString:O2ColorProfileGenericRGB])
        return rgbProfile(&GenericRGBPrimaries, gammaCurve(1.8));
    if ([name isEqualToString:O2ColorProfileGenericRGBLinear])
        return rgbProfile(&GenericRGBPrimaries, gammaCurve(1.0));
    if ([name isEqualToString:O2ColorProfileDisplayP3])
        return rgbProfile(&DisplayP3Primaries, sRGBCurve());
    if ([name isEqualToString:O2ColorProfileAdobeRGB1998])
        return rgbProfile(&AdobeRGBPrimaries, gammaCurve(563.0 / 256.0));
    if ([name isEqualToString:O2ColorProfileLinearSRGB])
        return rgbProfile(&BT709, gammaCurve(1.0));
    if ([name isEqualToString:O2ColorProfileITUR709])
        return rgbProfile(&BT709, bt709Curve());
    if ([name isEqualToString:O2ColorProfileITUR2020])
        return rgbProfile(&BT2020, bt709Curve());
    if ([name isEqualToString:O2ColorProfileGenericGray])
        return grayProfile(gammaCurve(1.8));
    /* Gray "gamma 2.2" matches as sRGB's curve on the reference (sRGB red
       is 0.509 in it, 0.505 with a pure 2.2 power; sRGB 0.5 stays 0.5). */
    if ([name isEqualToString:O2ColorProfileGenericGrayGamma22])
        return grayProfile(sRGBCurve());
    if ([name isEqualToString:O2ColorProfileSRGBGray])
        return grayProfile(sRGBCurve());
    if ([name isEqualToString:O2ColorProfileLinearGray])
        return grayProfile(gammaCurve(1.0));
    return NULL;
}

size_t O2ColorProfileComponentCount(NSString *profile) {
    if (profile == nil)
        return 0;
    if ([profile isEqualToString:O2ColorProfileGenericGray] || [profile isEqualToString:O2ColorProfileGenericGrayGamma22] ||
        [profile isEqualToString:O2ColorProfileSRGBGray] || [profile isEqualToString:O2ColorProfileLinearGray])
        return 1;
    return 3;
}

/* Profiles and transforms are made once, and kept. */
static pthread_mutex_t cacheLock = PTHREAD_MUTEX_INITIALIZER;
static NSMutableDictionary *profiles, *transforms;

static cmsHPROFILE profileNamed(NSString *name) {
    NSValue *value = [profiles objectForKey:name];
    cmsHPROFILE profile;

    if (value != nil)
        return [value pointerValue];
    profile = createProfile(name);
    if (profile == NULL)
        return NULL;
    if (profiles == nil)
        profiles = [[NSMutableDictionary alloc] init];
    [profiles setObject:[NSValue valueWithPointer:profile] forKey:name];
    return profile;
}

static cmsHTRANSFORM transformBetween(NSString *from, NSString *to) {
    NSString *key = [NSString stringWithFormat:@"%@>%@", from, to];
    NSValue *value = [transforms objectForKey:key];
    cmsHPROFILE in, out;
    cmsHTRANSFORM transform;

    if (value != nil)
        return [value pointerValue];
    in = profileNamed(from);
    out = profileNamed(to);
    if (in == NULL || out == NULL)
        return NULL;
    transform = cmsCreateTransform(in, O2ColorProfileComponentCount(from) == 1 ? TYPE_GRAY_DBL : TYPE_RGB_DBL,
                                   out, O2ColorProfileComponentCount(to) == 1 ? TYPE_GRAY_DBL : TYPE_RGB_DBL,
                                   INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOCACHE);
    if (transform == NULL)
        return NULL;
    if (transforms == nil)
        transforms = [[NSMutableDictionary alloc] init];
    [transforms setObject:[NSValue valueWithPointer:transform] forKey:key];
    return transform;
}

BOOL O2ColorMatchComponents(NSString *fromProfile, const double *in, NSString *toProfile, double *out) {
    size_t outCount = O2ColorProfileComponentCount(toProfile), i;
    double result[3];
    cmsHTRANSFORM transform;

    if (fromProfile == nil || toProfile == nil)
        return NO;
    if ([fromProfile isEqualToString:toProfile]) {
        if (out != in)
            for (i = 0; i < outCount; i++)
                out[i] = in[i];
        return YES;
    }
    pthread_mutex_lock(&cacheLock);
    transform = transformBetween(fromProfile, toProfile);
    pthread_mutex_unlock(&cacheLock);
    if (transform == NULL)
        return NO;
    cmsDoTransform(transform, in, result, 1);
    /* Out of the destination's gamut, clipped (as the reference's
       matching does for the spaces that are not extended). */
    for (i = 0; i < outCount; i++)
        out[i] = result[i] < 0 ? 0 : (result[i] > 1 ? 1 : result[i]);
    return YES;
}
