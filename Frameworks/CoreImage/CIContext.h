#import <Foundation/NSObject.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreImage/CoreImageDefines.h>

@class NSString;

/* Options of a context. */
CORE_IMAGE_EXPORT NSString *const kCIContextOutputColorSpace;
CORE_IMAGE_EXPORT NSString *const kCIContextWorkingColorSpace;
CORE_IMAGE_EXPORT NSString *const kCIContextWorkingFormat;
CORE_IMAGE_EXPORT NSString *const kCIContextHighQualityDownsample;
CORE_IMAGE_EXPORT NSString *const kCIContextOutputPremultiplied;
CORE_IMAGE_EXPORT NSString *const kCIContextCacheIntermediates;
CORE_IMAGE_EXPORT NSString *const kCIContextUseSoftwareRenderer;
CORE_IMAGE_EXPORT NSString *const kCIContextPriorityRequestLow;
CORE_IMAGE_EXPORT NSString *const kCIContextAllowLowPower;
CORE_IMAGE_EXPORT NSString *const kCIContextName;

/* Image representation options. */
CORE_IMAGE_EXPORT NSString *const kCIImageRepresentationAVDepthData;
CORE_IMAGE_EXPORT NSString *const kCIImageRepresentationDepthImage;
CORE_IMAGE_EXPORT NSString *const kCIImageRepresentationDisparityImage;
CORE_IMAGE_EXPORT NSString *const kCIImageRepresentationAVPortraitEffectsMatte;
CORE_IMAGE_EXPORT NSString *const kCIImageRepresentationPortraitEffectsMatteImage;
CORE_IMAGE_EXPORT NSString *const kCIImageRepresentationAVSemanticSegmentationMattes;
CORE_IMAGE_EXPORT NSString *const kCIImageRepresentationSemanticSegmentationSkinMatteImage;
CORE_IMAGE_EXPORT NSString *const kCIImageRepresentationSemanticSegmentationHairMatteImage;
CORE_IMAGE_EXPORT NSString *const kCIImageRepresentationSemanticSegmentationTeethMatteImage;


@class CIImage, NSDictionary;

@interface CIContext : NSObject {
    CGContextRef _cgContext;
}

+ (CIContext *)contextWithCGContext:(CGContextRef)cgContext options:(NSDictionary *)options;

- (void)drawImage:(CIImage *)image atPoint:(CGPoint)atPoint fromRect:(CGRect)fromRect;
- (void)drawImage:(CIImage *)image inRect:(CGRect)inRect fromRect:(CGRect)fromRect;
- (CGImageRef)createCGImage:(CIImage *)image fromRect:(CGRect)fromRect CF_RETURNS_RETAINED;

@end
