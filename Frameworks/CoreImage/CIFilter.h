/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>
#import <CoreImage/CoreImageDefines.h>

@class CIImage;
@protocol CIFilterConstructor;

NS_ASSUME_NONNULL_BEGIN

/* Attributes of a filter (its attributes dictionary). */
CORE_IMAGE_EXPORT NSString *const kCIAttributeFilterName;
CORE_IMAGE_EXPORT NSString *const kCIAttributeFilterDisplayName;
CORE_IMAGE_EXPORT NSString *const kCIAttributeDescription;
CORE_IMAGE_EXPORT NSString *const kCIAttributeFilterAvailable_Mac;
CORE_IMAGE_EXPORT NSString *const kCIAttributeFilterAvailable_iOS;
CORE_IMAGE_EXPORT NSString *const kCIAttributeReferenceDocumentation;
CORE_IMAGE_EXPORT NSString *const kCIAttributeFilterCategories;
CORE_IMAGE_EXPORT NSString *const kCIAttributeClass;
CORE_IMAGE_EXPORT NSString *const kCIAttributeType;
CORE_IMAGE_EXPORT NSString *const kCIAttributeMin;
CORE_IMAGE_EXPORT NSString *const kCIAttributeMax;
CORE_IMAGE_EXPORT NSString *const kCIAttributeSliderMin;
CORE_IMAGE_EXPORT NSString *const kCIAttributeSliderMax;
CORE_IMAGE_EXPORT NSString *const kCIAttributeDefault;
CORE_IMAGE_EXPORT NSString *const kCIAttributeIdentity;
CORE_IMAGE_EXPORT NSString *const kCIAttributeName;
CORE_IMAGE_EXPORT NSString *const kCIAttributeDisplayName;

/* User interface sets. */
CORE_IMAGE_EXPORT NSString *const kCIUIParameterSet;
CORE_IMAGE_EXPORT NSString *const kCIUISetBasic;
CORE_IMAGE_EXPORT NSString *const kCIUISetIntermediate;
CORE_IMAGE_EXPORT NSString *const kCIUISetAdvanced;
CORE_IMAGE_EXPORT NSString *const kCIUISetDevelopment;

/* Attribute types. */
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypeTime;
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypeScalar;
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypeDistance;
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypeAngle;
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypeBoolean;
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypeInteger;
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypeCount;
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypePosition;
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypeOffset;
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypePosition3;
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypeRectangle;
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypeOpaqueColor;
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypeColor;
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypeGradient;
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypeImage;
CORE_IMAGE_EXPORT NSString *const kCIAttributeTypeTransform;

/* Categories. */
CORE_IMAGE_EXPORT NSString *const kCICategoryDistortionEffect;
CORE_IMAGE_EXPORT NSString *const kCICategoryGeometryAdjustment;
CORE_IMAGE_EXPORT NSString *const kCICategoryCompositeOperation;
CORE_IMAGE_EXPORT NSString *const kCICategoryHalftoneEffect;
CORE_IMAGE_EXPORT NSString *const kCICategoryColorAdjustment;
CORE_IMAGE_EXPORT NSString *const kCICategoryColorEffect;
CORE_IMAGE_EXPORT NSString *const kCICategoryTransition;
CORE_IMAGE_EXPORT NSString *const kCICategoryTileEffect;
CORE_IMAGE_EXPORT NSString *const kCICategoryGenerator;
CORE_IMAGE_EXPORT NSString *const kCICategoryReduction;
CORE_IMAGE_EXPORT NSString *const kCICategoryGradient;
CORE_IMAGE_EXPORT NSString *const kCICategoryStylize;
CORE_IMAGE_EXPORT NSString *const kCICategorySharpen;
CORE_IMAGE_EXPORT NSString *const kCICategoryBlur;
CORE_IMAGE_EXPORT NSString *const kCICategoryVideo;
CORE_IMAGE_EXPORT NSString *const kCICategoryStillImage;
CORE_IMAGE_EXPORT NSString *const kCICategoryInterlaced;
CORE_IMAGE_EXPORT NSString *const kCICategoryNonSquarePixels;
CORE_IMAGE_EXPORT NSString *const kCICategoryHighDynamicRange;
CORE_IMAGE_EXPORT NSString *const kCICategoryBuiltIn;
CORE_IMAGE_EXPORT NSString *const kCICategoryFilterGenerator;

/* Options of -apply:. */
CORE_IMAGE_EXPORT NSString *const kCIApplyOptionExtent;
CORE_IMAGE_EXPORT NSString *const kCIApplyOptionDefinition;
CORE_IMAGE_EXPORT NSString *const kCIApplyOptionUserInfo;
CORE_IMAGE_EXPORT NSString *const kCIApplyOptionColorSpace;

/* Input and output keys. */
CORE_IMAGE_EXPORT NSString *const kCIOutputImageKey;
CORE_IMAGE_EXPORT NSString *const kCIInputBackgroundImageKey;
CORE_IMAGE_EXPORT NSString *const kCIInputImageKey;
CORE_IMAGE_EXPORT NSString *const kCIInputDepthImageKey;
CORE_IMAGE_EXPORT NSString *const kCIInputDisparityImageKey;
CORE_IMAGE_EXPORT NSString *const kCIInputAmountKey;
CORE_IMAGE_EXPORT NSString *const kCIInputTimeKey;
CORE_IMAGE_EXPORT NSString *const kCIInputTransformKey;
CORE_IMAGE_EXPORT NSString *const kCIInputScaleKey;
CORE_IMAGE_EXPORT NSString *const kCIInputAspectRatioKey;
CORE_IMAGE_EXPORT NSString *const kCIInputCenterKey;
CORE_IMAGE_EXPORT NSString *const kCIInputRadiusKey;
CORE_IMAGE_EXPORT NSString *const kCIInputAngleKey;
CORE_IMAGE_EXPORT NSString *const kCIInputRefractionKey;
CORE_IMAGE_EXPORT NSString *const kCIInputWidthKey;
CORE_IMAGE_EXPORT NSString *const kCIInputSharpnessKey;
CORE_IMAGE_EXPORT NSString *const kCIInputIntensityKey;
CORE_IMAGE_EXPORT NSString *const kCIInputEVKey;
CORE_IMAGE_EXPORT NSString *const kCIInputSaturationKey;
CORE_IMAGE_EXPORT NSString *const kCIInputColorKey;
CORE_IMAGE_EXPORT NSString *const kCIInputBrightnessKey;
CORE_IMAGE_EXPORT NSString *const kCIInputContrastKey;
CORE_IMAGE_EXPORT NSString *const kCIInputBiasKey;
CORE_IMAGE_EXPORT NSString *const kCIInputWeightsKey;
CORE_IMAGE_EXPORT NSString *const kCIInputGradientImageKey;
CORE_IMAGE_EXPORT NSString *const kCIInputMaskImageKey;
CORE_IMAGE_EXPORT NSString *const kCIInputMatteImageKey;
CORE_IMAGE_EXPORT NSString *const kCIInputShadingImageKey;
CORE_IMAGE_EXPORT NSString *const kCIInputTargetImageKey;
CORE_IMAGE_EXPORT NSString *const kCIInputExtentKey;
CORE_IMAGE_EXPORT NSString *const kCIInputVersionKey;

/* An image processing operation: its inputs and outputs by key, its
   attributes (what each input takes), and a registry of the filters by
   name and category. A subclass declares its inputs as properties named
   input...; one registered by attributes alone keeps them as values. */
CORE_IMAGE_CLASS_EXPORT
@interface CIFilter : NSObject <NSSecureCoding, NSCopying>

@property (readonly, nonatomic, nullable) CIImage *outputImage;
@property (nonatomic, copy) NSString *name;
@property (getter=isEnabled) BOOL enabled;

@property (readonly, nonatomic) NSArray<NSString *> *inputKeys;
@property (readonly, nonatomic) NSArray<NSString *> *outputKeys;
@property (readonly, nonatomic) NSDictionary<NSString *, id> *attributes;

- (void)setDefaults;

+ (nullable CIFilter *)filterWithName:(NSString *)name;
+ (nullable CIFilter *)filterWithName:(NSString *)name keysAndValues:(nullable id)key0, ... NS_REQUIRES_NIL_TERMINATION;
+ (nullable CIFilter *)filterWithName:(NSString *)name withInputParameters:(nullable NSDictionary<NSString *, id> *)params;

+ (NSArray<NSString *> *)filterNamesInCategory:(nullable NSString *)category;
+ (NSArray<NSString *> *)filterNamesInCategories:(nullable NSArray<NSString *> *)categories;

+ (void)registerFilterName:(NSString *)name constructor:(id<CIFilterConstructor>)anObject classAttributes:(NSDictionary<NSString *, id> *)attributes;

+ (nullable NSString *)localizedNameForFilterName:(NSString *)filterName;
+ (NSString *)localizedNameForCategory:(NSString *)category;
+ (nullable NSString *)localizedDescriptionForFilterName:(NSString *)filterName;
+ (nullable NSURL *)localizedReferenceDocumentationForFilterName:(NSString *)filterName;

@end

/* A subclass's attributes of its own, merged into -attributes. */
@interface CIFilter (CIFilterCustomAttributes)
+ (nullable NSDictionary<NSString *, id> *)customAttributes;
@end

NS_ASSUME_NONNULL_END
