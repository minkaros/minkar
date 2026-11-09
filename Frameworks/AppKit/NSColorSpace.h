/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <ApplicationServices/ApplicationServices.h>
#import <AppKit/AppKitExport.h>

@class NSArray, NSData, NSString;

typedef NS_ENUM(NSInteger, NSColorSpaceModel) {
    NSColorSpaceModelUnknown = -1,
    NSColorSpaceModelGray = 0,
    NSColorSpaceModelRGB = 1,
    NSColorSpaceModelCMYK = 2,
    NSColorSpaceModelLAB = 3,
    NSColorSpaceModelDeviceN = 4,
    NSColorSpaceModelIndexed = 5,
    NSColorSpaceModelPatterned = 6,
};

/* A color space. Minkar has no color management yet: each space draws as
   the device space of its model, and keeps its name. */
@interface NSColorSpace : NSObject <NSSecureCoding> {
    CGColorSpaceRef _cgColorSpace;
    NSString *_localizedName;
}

+ (NSColorSpace *)sRGBColorSpace;
+ (NSColorSpace *)extendedSRGBColorSpace;
+ (NSColorSpace *)displayP3ColorSpace;
+ (NSColorSpace *)adobeRGB1998ColorSpace;
+ (NSColorSpace *)genericRGBColorSpace;
+ (NSColorSpace *)deviceRGBColorSpace;
+ (NSColorSpace *)genericGrayColorSpace;
+ (NSColorSpace *)genericGamma22GrayColorSpace;
+ (NSColorSpace *)extendedGenericGamma22GrayColorSpace;
+ (NSColorSpace *)deviceGrayColorSpace;
+ (NSColorSpace *)genericCMYKColorSpace;
+ (NSColorSpace *)deviceCMYKColorSpace;

+ (NSArray<NSColorSpace *> *)availableColorSpacesWithModel:(NSColorSpaceModel)model;

- (instancetype)initWithCGColorSpace:(CGColorSpaceRef)cgColorSpace;
- (instancetype)initWithICCProfileData:(NSData *)iccData;

@property (readonly) CGColorSpaceRef CGColorSpace;
@property (readonly) NSColorSpaceModel colorSpaceModel;
@property (readonly) NSInteger numberOfColorComponents;
@property (readonly, copy) NSString *localizedName;
@property (readonly, strong) NSData *ICCProfileData;

@end

/* The color space name of the older API (NSCalibratedRGBColorSpace...)
   that colors in this space report. */
@interface NSColorSpace (Minkar)
- (NSString *)_colorSpaceName;
@end
