#import <Foundation/NSObject.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreImage/CoreImageDefines.h>

@class NSString;

/* A pixel format of an image's data. */
typedef int CIFormat;

/* Pixel formats. */
CORE_IMAGE_EXPORT const CIFormat kCIFormatARGB8;
CORE_IMAGE_EXPORT const CIFormat kCIFormatBGRA8;
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGBA8;
CORE_IMAGE_EXPORT const CIFormat kCIFormatABGR8;
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGBAh;
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGBA16;
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGBAf;
CORE_IMAGE_EXPORT const CIFormat kCIFormatA8;
CORE_IMAGE_EXPORT const CIFormat kCIFormatA16;
CORE_IMAGE_EXPORT const CIFormat kCIFormatAh;
CORE_IMAGE_EXPORT const CIFormat kCIFormatAf;
CORE_IMAGE_EXPORT const CIFormat kCIFormatR8;
CORE_IMAGE_EXPORT const CIFormat kCIFormatR16;
CORE_IMAGE_EXPORT const CIFormat kCIFormatRh;
CORE_IMAGE_EXPORT const CIFormat kCIFormatRf;
CORE_IMAGE_EXPORT const CIFormat kCIFormatRG8;
CORE_IMAGE_EXPORT const CIFormat kCIFormatRG16;
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGh;
CORE_IMAGE_EXPORT const CIFormat kCIFormatRGf;
CORE_IMAGE_EXPORT const CIFormat kCIFormatL8;
CORE_IMAGE_EXPORT const CIFormat kCIFormatL16;
CORE_IMAGE_EXPORT const CIFormat kCIFormatLh;
CORE_IMAGE_EXPORT const CIFormat kCIFormatLf;
CORE_IMAGE_EXPORT const CIFormat kCIFormatLA8;
CORE_IMAGE_EXPORT const CIFormat kCIFormatLA16;
CORE_IMAGE_EXPORT const CIFormat kCIFormatLAh;
CORE_IMAGE_EXPORT const CIFormat kCIFormatLAf;

/* Options of an image. */
CORE_IMAGE_EXPORT NSString *const kCIImageColorSpace;
CORE_IMAGE_EXPORT NSString *const kCIImageNearestSampling;
CORE_IMAGE_EXPORT NSString *const kCIImageProperties;
CORE_IMAGE_EXPORT NSString *const kCIImageApplyOrientationProperty;
CORE_IMAGE_EXPORT NSString *const kCIImageTextureTarget;
CORE_IMAGE_EXPORT NSString *const kCIImageTextureFormat;
CORE_IMAGE_EXPORT NSString *const kCIImageAuxiliaryDepth;
CORE_IMAGE_EXPORT NSString *const kCIImageAuxiliaryDisparity;
CORE_IMAGE_EXPORT NSString *const kCIImageAuxiliaryPortraitEffectsMatte;
CORE_IMAGE_EXPORT NSString *const kCIImageAuxiliarySemanticSegmentationSkinMatte;
CORE_IMAGE_EXPORT NSString *const kCIImageAuxiliarySemanticSegmentationHairMatte;
CORE_IMAGE_EXPORT NSString *const kCIImageAuxiliarySemanticSegmentationTeethMatte;

/* Options of -autoAdjustmentFiltersWithOptions:. */
CORE_IMAGE_EXPORT NSString *const kCIImageAutoAdjustEnhance;
CORE_IMAGE_EXPORT NSString *const kCIImageAutoAdjustRedEye;
CORE_IMAGE_EXPORT NSString *const kCIImageAutoAdjustFeatures;
CORE_IMAGE_EXPORT NSString *const kCIImageAutoAdjustCrop;
CORE_IMAGE_EXPORT NSString *const kCIImageAutoAdjustLevel;

/* Options of an image provider. */
CORE_IMAGE_EXPORT NSString *const kCIImageProviderTileSize;
CORE_IMAGE_EXPORT NSString *const kCIImageProviderUserInfo;


@class CIFilter;

@interface CIImage : NSObject {
    CGImageRef _cgImage;
    CIFilter *_filter;
}

+ (CIImage *)emptyImage;
+ (CIImage *)imageWithCGImage:(CGImageRef)cgImage;

- initWithCGImage:(CGImageRef)cgImage;
- (CGRect)extent;
- (CGImageRef)CGImage;

@end
