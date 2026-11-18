/* Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

/* Color matching between the standard color spaces, on Little CMS: each
   space a profile made from its published primaries, white point and tone
   curve; the device spaces are sRGB's. */

#import <Onyx2D/O2ColorSpace.h>

/* The color matching profile a space is known by: one of the
   O2ColorProfile... names below, or nil for one that is not matched
   (CMYK, patterns, DeviceN). */
extern NSString *const O2ColorProfileSRGB;
extern NSString *const O2ColorProfileGenericRGB;         /* gamma 1.8 */
extern NSString *const O2ColorProfileGenericRGBLinear;
extern NSString *const O2ColorProfileDisplayP3;
extern NSString *const O2ColorProfileAdobeRGB1998;
extern NSString *const O2ColorProfileLinearSRGB;
extern NSString *const O2ColorProfileITUR709;
extern NSString *const O2ColorProfileITUR2020;
extern NSString *const O2ColorProfileGenericGray;        /* gamma 1.8 */
extern NSString *const O2ColorProfileGenericGrayGamma22;
extern NSString *const O2ColorProfileSRGBGray;           /* sRGB's curve */
extern NSString *const O2ColorProfileLinearGray;

/* Converts a color's components (no alpha) from one profile to
   another; NO, and out unchanged, when either is unknown or the models
   differ in a way it cannot match. In and out may be the same. */
BOOL O2ColorMatchComponents(NSString *fromProfile, const double *in, NSString *toProfile, double *out);

/* The number of components of a profile's colors (1 or 3); 0 if unknown. */
size_t O2ColorProfileComponentCount(NSString *profile);
