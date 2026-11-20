/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "CILinearGradient.h"
#import "CIImagePrivate.h"
#import <CoreImage/CIColor.h>
#import <CoreImage/CIVector.h>

@implementation CILinearGradient

/* As the reference's: its categories, and the inputs' defaults. */
+ (NSDictionary<NSString *, id> *)customAttributes {
    return @{
        kCIAttributeFilterDisplayName: @"Linear Gradient",
        kCIAttributeDescription: @"A gradient between two colors along the line through two points.",
        kCIAttributeFilterCategories: @[ kCICategoryGradient, kCICategoryVideo, kCICategoryStillImage, kCICategoryBuiltIn ],
        kCIAttributeFilterAvailable_Mac: @"10.4",
        kCIAttributeFilterAvailable_iOS: @"5",
        @"inputPoint0": @{ kCIAttributeClass: @"CIVector", kCIAttributeDefault: [CIVector vectorWithX:0 Y:0],
                           kCIAttributeDisplayName: @"Point 0", kCIAttributeType: kCIAttributeTypePosition },
        @"inputPoint1": @{ kCIAttributeClass: @"CIVector", kCIAttributeDefault: [CIVector vectorWithX:200 Y:200],
                           kCIAttributeDisplayName: @"Point 1", kCIAttributeType: kCIAttributeTypePosition },
        @"inputColor0": @{ kCIAttributeClass: @"CIColor", kCIAttributeDefault: [CIColor colorWithRed:1 green:1 blue:1 alpha:1],
                           kCIAttributeDisplayName: @"Color 1", kCIAttributeType: kCIAttributeTypeColor },
        @"inputColor1": @{ kCIAttributeClass: @"CIColor", kCIAttributeDefault: [CIColor colorWithRed:0 green:0 blue:0 alpha:1],
                           kCIAttributeDisplayName: @"Color 2", kCIAttributeType: kCIAttributeTypeColor },
    };
}

- (CIImage *)outputImage {
    return CIImageWithFilter(self);
}

@end
