/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "CIBoxBlur.h"
#import "CIImagePrivate.h"
#import <CoreImage/CIImage.h>

@implementation CIBoxBlur

/* As the reference's: its categories, and the radius's default and range. */
+ (NSDictionary<NSString *, id> *)customAttributes {
    return @{
        kCIAttributeFilterDisplayName: @"Box Blur",
        kCIAttributeDescription: @"Blurs an image with a square: each pixel the average of those around it.",
        kCIAttributeFilterCategories: @[ kCICategoryBlur, kCICategoryVideo, kCICategoryStillImage, kCICategoryBuiltIn ],
        kCIAttributeFilterAvailable_Mac: @"10.5",
        kCIAttributeFilterAvailable_iOS: @"9",
        @"inputImage": @{ kCIAttributeClass: @"CIImage", kCIAttributeDisplayName: @"Image", kCIAttributeType: kCIAttributeTypeImage },
        @"inputRadius": @{ kCIAttributeClass: @"NSNumber", kCIAttributeDefault: @10.0, kCIAttributeDisplayName: @"Radius",
                           kCIAttributeMin: @1.0, kCIAttributeSliderMin: @1.0, kCIAttributeSliderMax: @100.0,
                           kCIAttributeType: kCIAttributeTypeDistance },
    };
}

- (CIImage *)outputImage {
    if (self.inputImage == nil)
        return nil;
    return CIImageWithFilter(self);
}

@end
