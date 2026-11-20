/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The public constants of CIFilter.h, CIContext.h and CIImage.h, with the
 * reference's values.
 */

#import <CoreImage/CIFilter.h>
#import <CoreImage/CIContext.h>
#import <CoreImage/CIImage.h>

#pragma mark CIFilter.h

/* Attributes of a filter (its attributes dictionary). */
NSString *const kCIAttributeFilterName = @"CIAttributeFilterName";
NSString *const kCIAttributeFilterDisplayName = @"CIAttributeFilterDisplayName";
NSString *const kCIAttributeDescription = @"CIAttributeDescription";
NSString *const kCIAttributeFilterAvailable_Mac = @"CIAttributeFilterAvailable_Mac";
NSString *const kCIAttributeFilterAvailable_iOS = @"CIAttributeFilterAvailable_iOS";
NSString *const kCIAttributeReferenceDocumentation = @"CIAttributeReferenceDocumentation";
NSString *const kCIAttributeFilterCategories = @"CIAttributeFilterCategories";
NSString *const kCIAttributeClass = @"CIAttributeClass";
NSString *const kCIAttributeType = @"CIAttributeType";
NSString *const kCIAttributeMin = @"CIAttributeMin";
NSString *const kCIAttributeMax = @"CIAttributeMax";
NSString *const kCIAttributeSliderMin = @"CIAttributeSliderMin";
NSString *const kCIAttributeSliderMax = @"CIAttributeSliderMax";
NSString *const kCIAttributeDefault = @"CIAttributeDefault";
NSString *const kCIAttributeIdentity = @"CIAttributeIdentity";
NSString *const kCIAttributeName = @"CIAttributeName";
NSString *const kCIAttributeDisplayName = @"CIAttributeDisplayName";

/* User interface sets. */
NSString *const kCIUIParameterSet = @"CIUIParameterSet";
NSString *const kCIUISetBasic = @"CIUISetBasic";
NSString *const kCIUISetIntermediate = @"CIUISetIntermediate";
NSString *const kCIUISetAdvanced = @"CIUISetAdvanced";
NSString *const kCIUISetDevelopment = @"CIUISetDevelopment";

/* Attribute types. */
NSString *const kCIAttributeTypeTime = @"CIAttributeTypeTime";
NSString *const kCIAttributeTypeScalar = @"CIAttributeTypeScalar";
NSString *const kCIAttributeTypeDistance = @"CIAttributeTypeDistance";
NSString *const kCIAttributeTypeAngle = @"CIAttributeTypeAngle";
NSString *const kCIAttributeTypeBoolean = @"CIAttributeTypeBoolean";
NSString *const kCIAttributeTypeInteger = @"CIAttributeTypeInteger";
NSString *const kCIAttributeTypeCount = @"CIAttributeTypeCount";
NSString *const kCIAttributeTypePosition = @"CIAttributeTypePosition";
NSString *const kCIAttributeTypeOffset = @"CIAttributeTypeOffset";
NSString *const kCIAttributeTypePosition3 = @"CIAttributeTypePosition3";
NSString *const kCIAttributeTypeRectangle = @"CIAttributeTypeRectangle";
NSString *const kCIAttributeTypeOpaqueColor = @"CIAttributeTypeOpaqueColor";
NSString *const kCIAttributeTypeColor = @"CIAttributeTypeColor";
NSString *const kCIAttributeTypeGradient = @"CIAttributeTypeGradient";
NSString *const kCIAttributeTypeImage = @"CIAttributeTypeImage";
NSString *const kCIAttributeTypeTransform = @"CIAttributeTypeTransform";

/* Categories. */
NSString *const kCICategoryDistortionEffect = @"CICategoryDistortionEffect";
NSString *const kCICategoryGeometryAdjustment = @"CICategoryGeometryAdjustment";
NSString *const kCICategoryCompositeOperation = @"CICategoryCompositeOperation";
NSString *const kCICategoryHalftoneEffect = @"CICategoryHalftoneEffect";
NSString *const kCICategoryColorAdjustment = @"CICategoryColorAdjustment";
NSString *const kCICategoryColorEffect = @"CICategoryColorEffect";
NSString *const kCICategoryTransition = @"CICategoryTransition";
NSString *const kCICategoryTileEffect = @"CICategoryTileEffect";
NSString *const kCICategoryGenerator = @"CICategoryGenerator";
NSString *const kCICategoryReduction = @"CICategoryReduction";
NSString *const kCICategoryGradient = @"CICategoryGradient";
NSString *const kCICategoryStylize = @"CICategoryStylize";
NSString *const kCICategorySharpen = @"CICategorySharpen";
NSString *const kCICategoryBlur = @"CICategoryBlur";
NSString *const kCICategoryVideo = @"CICategoryVideo";
NSString *const kCICategoryStillImage = @"CICategoryStillImage";
NSString *const kCICategoryInterlaced = @"CICategoryInterlaced";
NSString *const kCICategoryNonSquarePixels = @"CICategoryNonSquarePixels";
NSString *const kCICategoryHighDynamicRange = @"CICategoryHighDynamicRange";
NSString *const kCICategoryBuiltIn = @"CICategoryBuiltIn";
NSString *const kCICategoryFilterGenerator = @"CICategoryFilterGenerator";

/* Options of -apply:. */
NSString *const kCIApplyOptionExtent = @"extent";
NSString *const kCIApplyOptionDefinition = @"definition";
NSString *const kCIApplyOptionUserInfo = @"user_info";
NSString *const kCIApplyOptionColorSpace = @"color_space";

/* Input and output keys. */
NSString *const kCIOutputImageKey = @"outputImage";
NSString *const kCIInputBackgroundImageKey = @"inputBackgroundImage";
NSString *const kCIInputImageKey = @"inputImage";
NSString *const kCIInputDepthImageKey = @"inputDepthImage";
NSString *const kCIInputDisparityImageKey = @"inputDisparityImage";
NSString *const kCIInputAmountKey = @"inputAmount";
NSString *const kCIInputTimeKey = @"inputTime";
NSString *const kCIInputTransformKey = @"inputTransform";
NSString *const kCIInputScaleKey = @"inputScale";
NSString *const kCIInputAspectRatioKey = @"inputAspectRatio";
NSString *const kCIInputCenterKey = @"inputCenter";
NSString *const kCIInputRadiusKey = @"inputRadius";
NSString *const kCIInputAngleKey = @"inputAngle";
NSString *const kCIInputRefractionKey = @"inputRefraction";
NSString *const kCIInputWidthKey = @"inputWidth";
NSString *const kCIInputSharpnessKey = @"inputSharpness";
NSString *const kCIInputIntensityKey = @"inputIntensity";
NSString *const kCIInputEVKey = @"inputEV";
NSString *const kCIInputSaturationKey = @"inputSaturation";
NSString *const kCIInputColorKey = @"inputColor";
NSString *const kCIInputBrightnessKey = @"inputBrightness";
NSString *const kCIInputContrastKey = @"inputContrast";
NSString *const kCIInputBiasKey = @"inputBias";
NSString *const kCIInputWeightsKey = @"inputWeights";
NSString *const kCIInputGradientImageKey = @"inputGradientImage";
NSString *const kCIInputMaskImageKey = @"inputMaskImage";
NSString *const kCIInputMatteImageKey = @"inputMatteImage";
NSString *const kCIInputShadingImageKey = @"inputShadingImage";
NSString *const kCIInputTargetImageKey = @"inputTargetImage";
NSString *const kCIInputExtentKey = @"inputExtent";
NSString *const kCIInputVersionKey = @"__inputVersion";

#pragma mark CIContext.h

/* Options of a context. */
NSString *const kCIContextOutputColorSpace = @"output_color_space";
NSString *const kCIContextWorkingColorSpace = @"working_color_space";
NSString *const kCIContextWorkingFormat = @"working_format";
NSString *const kCIContextHighQualityDownsample = @"high_quality_downsample";
NSString *const kCIContextOutputPremultiplied = @"output_premultiplied";
NSString *const kCIContextCacheIntermediates = @"kCIContextCacheIntermediates";
NSString *const kCIContextUseSoftwareRenderer = @"software_renderer";
NSString *const kCIContextPriorityRequestLow = @"priority_request_low";
NSString *const kCIContextAllowLowPower = @"kCIContextAllowLowPower";
NSString *const kCIContextName = @"kCIContextName";

/* Image representation options. */
NSString *const kCIImageRepresentationAVDepthData = @"kCIImageRepresentationAVDepthData";
NSString *const kCIImageRepresentationDepthImage = @"kCIImageRepresentationDepthImage";
NSString *const kCIImageRepresentationDisparityImage = @"kCIImageRepresentationDisparityImage";
NSString *const kCIImageRepresentationAVPortraitEffectsMatte = @"kCIImageRepresentationAVPortraitEffectsMatte";
NSString *const kCIImageRepresentationPortraitEffectsMatteImage = @"kCIImageRepresentationPortraitEffectsMatteImage";
NSString *const kCIImageRepresentationAVSemanticSegmentationMattes = @"kCIImageRepresentationAVSemanticSegmentationMattes";
NSString *const kCIImageRepresentationSemanticSegmentationSkinMatteImage = @"kCIImageRepresentationSemanticSegmentationSkinMatteImage";
NSString *const kCIImageRepresentationSemanticSegmentationHairMatteImage = @"kCIImageRepresentationSemanticSegmentationHairMatteImage";
NSString *const kCIImageRepresentationSemanticSegmentationTeethMatteImage = @"kCIImageRepresentationSemanticSegmentationTeethMatteImage";

#pragma mark CIImage.h

/* Pixel formats. */
const CIFormat kCIFormatARGB8 = 265;
const CIFormat kCIFormatBGRA8 = 266;
const CIFormat kCIFormatRGBA8 = 264;
const CIFormat kCIFormatABGR8 = 267;
const CIFormat kCIFormatRGBAh = 2056;
const CIFormat kCIFormatRGBA16 = 1800;
const CIFormat kCIFormatRGBAf = 2312;
const CIFormat kCIFormatA8 = 257;
const CIFormat kCIFormatA16 = 1793;
const CIFormat kCIFormatAh = 2049;
const CIFormat kCIFormatAf = 2305;
const CIFormat kCIFormatR8 = 261;
const CIFormat kCIFormatR16 = 1797;
const CIFormat kCIFormatRh = 2053;
const CIFormat kCIFormatRf = 2309;
const CIFormat kCIFormatRG8 = 262;
const CIFormat kCIFormatRG16 = 1798;
const CIFormat kCIFormatRGh = 2054;
const CIFormat kCIFormatRGf = 2310;
const CIFormat kCIFormatL8 = 259;
const CIFormat kCIFormatL16 = 1795;
const CIFormat kCIFormatLh = 2051;
const CIFormat kCIFormatLf = 2307;
const CIFormat kCIFormatLA8 = 260;
const CIFormat kCIFormatLA16 = 1796;
const CIFormat kCIFormatLAh = 2052;
const CIFormat kCIFormatLAf = 2308;

/* Options of an image. */
NSString *const kCIImageColorSpace = @"CIImageColorSpace";
NSString *const kCIImageNearestSampling = @"CIImageNearestSampling";
NSString *const kCIImageProperties = @"CIImageProperties";
NSString *const kCIImageApplyOrientationProperty = @"kCIImageApplyOrientationProperty";
NSString *const kCIImageTextureTarget = @"kCIImageTextureTarget";
NSString *const kCIImageTextureFormat = @"kCIImageTextureFormat";
NSString *const kCIImageAuxiliaryDepth = @"kCIImageAuxiliaryDepth";
NSString *const kCIImageAuxiliaryDisparity = @"kCIImageAuxiliaryDisparity";
NSString *const kCIImageAuxiliaryPortraitEffectsMatte = @"kCIImageAuxiliaryPortraitEffectsMatte";
NSString *const kCIImageAuxiliarySemanticSegmentationSkinMatte = @"kCIImageAuxiliarySemanticSegmentationSkinMatte";
NSString *const kCIImageAuxiliarySemanticSegmentationHairMatte = @"kCIImageAuxiliarySemanticSegmentationHairMatte";
NSString *const kCIImageAuxiliarySemanticSegmentationTeethMatte = @"kCIImageAuxiliarySemanticSegmentationTeethMatte";

/* Options of -autoAdjustmentFiltersWithOptions:. */
NSString *const kCIImageAutoAdjustEnhance = @"kCIImageAutoAdjustEnhance";
NSString *const kCIImageAutoAdjustRedEye = @"kCIImageAutoAdjustRedEye";
NSString *const kCIImageAutoAdjustFeatures = @"kCIImageAutoAdjustFeatures";
NSString *const kCIImageAutoAdjustCrop = @"kCIImageAutoAdjustCrop";
NSString *const kCIImageAutoAdjustLevel = @"kCIImageAutoAdjustLevel";

/* Options of an image provider. */
NSString *const kCIImageProviderTileSize = @"tile_size";
NSString *const kCIImageProviderUserInfo = @"user_info";
