/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Metal's settings: enums and the descriptor classes.
 */
#import <Foundation/Foundation.h>
NS_ASSUME_NONNULL_BEGIN
typedef NS_ENUM(NSUInteger, MTLPixelFormat) {
    MTLPixelFormatInvalid = 0,
    MTLPixelFormatA8Unorm = 1,
    MTLPixelFormatR8Unorm = 10,
    MTLPixelFormatR16Unorm = 20,
    MTLPixelFormatRG8Unorm = 30,
    MTLPixelFormatRGBA8Unorm = 70,
    MTLPixelFormatRGBA8Unorm_sRGB = 71,
    MTLPixelFormatBGRA8Unorm = 80,
    MTLPixelFormatBGRA8Unorm_sRGB = 81,
    MTLPixelFormatRGB10A2Unorm = 90,
    MTLPixelFormatRGBA16Float = 115,
    MTLPixelFormatRGBA32Float = 125,
    MTLPixelFormatDepth32Float = 252,
    MTLPixelFormatStencil8 = 253,
    MTLPixelFormatDepth24Unorm_Stencil8 = 255,
    MTLPixelFormatDepth32Float_Stencil8 = 260,
};
typedef NS_ENUM(NSUInteger, MTLTextureType) {
    MTLTextureType1D = 0, MTLTextureType1DArray = 1, MTLTextureType2D = 2, MTLTextureType2DArray = 3,
    MTLTextureType2DMultisample = 4, MTLTextureTypeCube = 5, MTLTextureTypeCubeArray = 6, MTLTextureType3D = 7,
    MTLTextureType2DMultisampleArray = 8, MTLTextureTypeTextureBuffer = 9,
};
typedef NS_OPTIONS(NSUInteger, MTLTextureUsage) {
    MTLTextureUsageUnknown = 0, MTLTextureUsageShaderRead = 1, MTLTextureUsageShaderWrite = 2,
    MTLTextureUsageRenderTarget = 4, MTLTextureUsagePixelFormatView = 0x10,
};
typedef NS_ENUM(NSUInteger, MTLCPUCacheMode) { MTLCPUCacheModeDefaultCache = 0, MTLCPUCacheModeWriteCombined = 1 };
typedef NS_ENUM(NSUInteger, MTLStorageMode) {
    MTLStorageModeShared = 0, MTLStorageModeManaged = 1, MTLStorageModePrivate = 2, MTLStorageModeMemoryless = 3,
};
typedef NS_OPTIONS(NSUInteger, MTLResourceOptions) {
    MTLResourceCPUCacheModeDefaultCache = 0, MTLResourceCPUCacheModeWriteCombined = 1,
    MTLResourceStorageModeShared = 0 << 4, MTLResourceStorageModeManaged = 1 << 4, MTLResourceStorageModePrivate = 2 << 4,
};
typedef NS_ENUM(NSUInteger, MTLSamplerMinMagFilter) { MTLSamplerMinMagFilterNearest = 0, MTLSamplerMinMagFilterLinear = 1 };
typedef NS_ENUM(NSUInteger, MTLSamplerMipFilter) {
    MTLSamplerMipFilterNotMipmapped = 0, MTLSamplerMipFilterNearest = 1, MTLSamplerMipFilterLinear = 2,
};
typedef NS_ENUM(NSUInteger, MTLSamplerAddressMode) {
    MTLSamplerAddressModeClampToEdge = 0, MTLSamplerAddressModeMirrorClampToEdge = 1, MTLSamplerAddressModeRepeat = 2,
    MTLSamplerAddressModeMirrorRepeat = 3, MTLSamplerAddressModeClampToZero = 4, MTLSamplerAddressModeClampToBorderColor = 5,
};
typedef NS_ENUM(NSUInteger, MTLCompareFunction) {
    MTLCompareFunctionNever = 0, MTLCompareFunctionLess = 1, MTLCompareFunctionEqual = 2, MTLCompareFunctionLessEqual = 3,
    MTLCompareFunctionGreater = 4, MTLCompareFunctionNotEqual = 5, MTLCompareFunctionGreaterEqual = 6, MTLCompareFunctionAlways = 7,
};
typedef NS_ENUM(NSUInteger, MTLStencilOperation) {
    MTLStencilOperationKeep = 0, MTLStencilOperationZero = 1, MTLStencilOperationReplace = 2,
    MTLStencilOperationIncrementClamp = 3, MTLStencilOperationDecrementClamp = 4, MTLStencilOperationInvert = 5,
    MTLStencilOperationIncrementWrap = 6, MTLStencilOperationDecrementWrap = 7,
};
typedef NS_ENUM(NSUInteger, MTLVertexFormat) {
    MTLVertexFormatInvalid = 0, MTLVertexFormatUChar4 = 3, MTLVertexFormatUChar4Normalized = 9,
    MTLVertexFormatFloat = 28, MTLVertexFormatFloat2 = 29, MTLVertexFormatFloat3 = 30, MTLVertexFormatFloat4 = 31,
};
typedef NS_ENUM(NSUInteger, MTLVertexStepFunction) {
    MTLVertexStepFunctionConstant = 0, MTLVertexStepFunctionPerVertex = 1, MTLVertexStepFunctionPerInstance = 2,
};
typedef NS_ENUM(NSUInteger, MTLBlendFactor) {
    MTLBlendFactorZero = 0, MTLBlendFactorOne = 1, MTLBlendFactorSourceColor = 2, MTLBlendFactorOneMinusSourceColor = 3,
    MTLBlendFactorSourceAlpha = 4, MTLBlendFactorOneMinusSourceAlpha = 5, MTLBlendFactorDestinationColor = 6,
    MTLBlendFactorOneMinusDestinationColor = 7, MTLBlendFactorDestinationAlpha = 8, MTLBlendFactorOneMinusDestinationAlpha = 9,
};
typedef NS_ENUM(NSUInteger, MTLBlendOperation) {
    MTLBlendOperationAdd = 0, MTLBlendOperationSubtract = 1, MTLBlendOperationReverseSubtract = 2,
    MTLBlendOperationMin = 3, MTLBlendOperationMax = 4,
};
typedef NS_OPTIONS(NSUInteger, MTLColorWriteMask) {
    MTLColorWriteMaskNone = 0, MTLColorWriteMaskRed = 8, MTLColorWriteMaskGreen = 4, MTLColorWriteMaskBlue = 2,
    MTLColorWriteMaskAlpha = 1, MTLColorWriteMaskAll = 0xf,
};
typedef NS_ENUM(NSUInteger, MTLLanguageVersion) {
    MTLLanguageVersion1_1 = (1 << 16) + 1, MTLLanguageVersion1_2 = (1 << 16) + 2, MTLLanguageVersion2_0 = (2 << 16),
    MTLLanguageVersion2_1 = (2 << 16) + 1, MTLLanguageVersion2_2 = (2 << 16) + 2,
};
typedef NS_ENUM(NSInteger, MTLCaptureDestination) {
    MTLCaptureDestinationDeveloperTools = 1, MTLCaptureDestinationGPUTraceDocument = 2,
};
typedef NS_ENUM(NSUInteger, MTLLoadAction) { MTLLoadActionDontCare = 0, MTLLoadActionLoad = 1, MTLLoadActionClear = 2 };
typedef NS_ENUM(NSUInteger, MTLStoreAction) { MTLStoreActionDontCare = 0, MTLStoreActionStore = 1 };
typedef struct { double red, green, blue, alpha; } MTLClearColor;

@protocol MTLDevice;
@protocol MTLTexture;
@protocol MTLFunction;

@interface MTLTextureDescriptor : NSObject <NSCopying>
@property (nonatomic, assign) MTLTextureType textureType;
@property (nonatomic, assign) MTLPixelFormat pixelFormat;
@property (nonatomic, assign) NSUInteger width;
@property (nonatomic, assign) NSUInteger height;
@property (nonatomic, assign) NSUInteger depth;
@property (nonatomic, assign) NSUInteger mipmapLevelCount;
@property (nonatomic, assign) NSUInteger sampleCount;
@property (nonatomic, assign) NSUInteger arrayLength;
@property (nonatomic, assign) MTLResourceOptions resourceOptions;
@property (nonatomic, assign) MTLCPUCacheMode cpuCacheMode;
@property (nonatomic, assign) MTLStorageMode storageMode;
@property (nonatomic, assign) MTLTextureUsage usage;
@end

@interface MTLSamplerDescriptor : NSObject <NSCopying>
@property (nonatomic, assign) MTLSamplerMinMagFilter minFilter;
@property (nonatomic, assign) MTLSamplerMinMagFilter magFilter;
@property (nonatomic, assign) MTLSamplerMipFilter mipFilter;
@property (nonatomic, assign) NSUInteger maxAnisotropy;
@property (nonatomic, assign) MTLSamplerAddressMode sAddressMode;
@property (nonatomic, assign) MTLSamplerAddressMode tAddressMode;
@property (nonatomic, assign) MTLSamplerAddressMode rAddressMode;
@property (nonatomic, assign) BOOL normalizedCoordinates;
@property (nonatomic, assign) float lodMinClamp;
@property (nonatomic, assign) float lodMaxClamp;
@property (nonatomic, assign) MTLCompareFunction compareFunction;
@property (nonatomic, copy, nullable) NSString *label;
@end

@interface MTLStencilDescriptor : NSObject <NSCopying>
@property (nonatomic, assign) MTLCompareFunction stencilCompareFunction;
@property (nonatomic, assign) MTLStencilOperation stencilFailureOperation;
@property (nonatomic, assign) MTLStencilOperation depthFailureOperation;
@property (nonatomic, assign) MTLStencilOperation depthStencilPassOperation;
@property (nonatomic, assign) uint32_t readMask;
@property (nonatomic, assign) uint32_t writeMask;
@end

@interface MTLDepthStencilDescriptor : NSObject <NSCopying>
@property (nonatomic, assign) MTLCompareFunction depthCompareFunction;
@property (nonatomic, getter=isDepthWriteEnabled) BOOL depthWriteEnabled;
@property (nonatomic, copy) MTLStencilDescriptor *frontFaceStencil;
@property (nonatomic, copy) MTLStencilDescriptor *backFaceStencil;
@property (nonatomic, copy, nullable) NSString *label;
@end

@interface MTLVertexAttributeDescriptor : NSObject <NSCopying>
@property (nonatomic, assign) MTLVertexFormat format;
@property (nonatomic, assign) NSUInteger offset;
@property (nonatomic, assign) NSUInteger bufferIndex;
@end

@interface MTLVertexBufferLayoutDescriptor : NSObject <NSCopying>
@property (nonatomic, assign) NSUInteger stride;
@property (nonatomic, assign) MTLVertexStepFunction stepFunction;
@property (nonatomic, assign) NSUInteger stepRate;
@end

@interface MTLRenderPipelineColorAttachmentDescriptor : NSObject <NSCopying>
@property (nonatomic, assign) MTLPixelFormat pixelFormat;
@property (nonatomic, getter=isBlendingEnabled) BOOL blendingEnabled;
@property (nonatomic, assign) MTLBlendFactor sourceRGBBlendFactor;
@property (nonatomic, assign) MTLBlendFactor destinationRGBBlendFactor;
@property (nonatomic, assign) MTLBlendOperation rgbBlendOperation;
@property (nonatomic, assign) MTLBlendFactor sourceAlphaBlendFactor;
@property (nonatomic, assign) MTLBlendFactor destinationAlphaBlendFactor;
@property (nonatomic, assign) MTLBlendOperation alphaBlendOperation;
@property (nonatomic, assign) MTLColorWriteMask writeMask;
@end

@interface MTLCompileOptions : NSObject <NSCopying>
@property (nonatomic, copy, nullable) NSDictionary *preprocessorMacros;
@property (nonatomic, assign) BOOL fastMathEnabled;
@property (nonatomic, assign) MTLLanguageVersion languageVersion;
@end

@interface MTLCaptureDescriptor : NSObject <NSCopying>
@property (nonatomic, strong, nullable) id captureObject;
@property (nonatomic, assign) MTLCaptureDestination destination;
@property (nonatomic, copy, nullable) NSURL *outputURL;
@end

@interface MTLVertexAttributeDescriptorArray : NSObject
- (MTLVertexAttributeDescriptor *)objectAtIndexedSubscript:(NSUInteger)index;
- (void)setObject:(nullable MTLVertexAttributeDescriptor *)attribute atIndexedSubscript:(NSUInteger)index;
@end

@interface MTLVertexBufferLayoutDescriptorArray : NSObject
- (MTLVertexBufferLayoutDescriptor *)objectAtIndexedSubscript:(NSUInteger)index;
- (void)setObject:(nullable MTLVertexBufferLayoutDescriptor *)layout atIndexedSubscript:(NSUInteger)index;
@end

@interface MTLVertexDescriptor : NSObject <NSCopying>
+ (MTLVertexDescriptor *)vertexDescriptor;
@property (readonly) MTLVertexBufferLayoutDescriptorArray *layouts;
@property (readonly) MTLVertexAttributeDescriptorArray *attributes;
- (void)reset;
@end

@interface MTLRenderPipelineColorAttachmentDescriptorArray : NSObject
- (MTLRenderPipelineColorAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)index;
- (void)setObject:(nullable MTLRenderPipelineColorAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)index;
@end

@interface MTLRenderPipelineDescriptor : NSObject <NSCopying>
@property (nullable, copy) NSString *label;
@property (nullable, strong) id<MTLFunction> vertexFunction;
@property (nullable, strong) id<MTLFunction> fragmentFunction;
@property (nullable, copy) MTLVertexDescriptor *vertexDescriptor;
@property NSUInteger sampleCount;
@property NSUInteger rasterSampleCount;
@property (getter=isAlphaToCoverageEnabled) BOOL alphaToCoverageEnabled;
@property (getter=isRasterizationEnabled) BOOL rasterizationEnabled;
@property (readonly) MTLRenderPipelineColorAttachmentDescriptorArray *colorAttachments;
@property MTLPixelFormat depthAttachmentPixelFormat;
@property MTLPixelFormat stencilAttachmentPixelFormat;
- (void)reset;
@end

@interface MTLRenderPassAttachmentDescriptor : NSObject <NSCopying>
@property (nullable, strong) id<MTLTexture> texture;
@property NSUInteger level;
@property NSUInteger slice;
@property MTLLoadAction loadAction;
@property MTLStoreAction storeAction;
@end

@interface MTLRenderPassColorAttachmentDescriptor : MTLRenderPassAttachmentDescriptor
@property MTLClearColor clearColor;
@end

@interface MTLRenderPassDepthAttachmentDescriptor : MTLRenderPassAttachmentDescriptor
@property double clearDepth;
@end

@interface MTLRenderPassStencilAttachmentDescriptor : MTLRenderPassAttachmentDescriptor
@property uint32_t clearStencil;
@end

@interface MTLRenderPassColorAttachmentDescriptorArray : NSObject
- (MTLRenderPassColorAttachmentDescriptor *)objectAtIndexedSubscript:(NSUInteger)index;
- (void)setObject:(nullable MTLRenderPassColorAttachmentDescriptor *)attachment atIndexedSubscript:(NSUInteger)index;
@end

@interface MTLRenderPassDescriptor : NSObject <NSCopying>
+ (MTLRenderPassDescriptor *)renderPassDescriptor;
@property (readonly) MTLRenderPassColorAttachmentDescriptorArray *colorAttachments;
@property (copy, null_resettable) MTLRenderPassDepthAttachmentDescriptor *depthAttachment;
@property (copy, null_resettable) MTLRenderPassStencilAttachmentDescriptor *stencilAttachment;
@property NSUInteger renderTargetWidth;
@property NSUInteger renderTargetHeight;
@end

@interface MTLFunctionConstantValues : NSObject <NSCopying>
- (void)setConstantValue:(const void *)value type:(NSUInteger)type atIndex:(NSUInteger)index;
- (void)setConstantValue:(const void *)value type:(NSUInteger)type withName:(NSString *)name;
- (void)reset;
@end

@interface MTLCaptureManager : NSObject
+ (MTLCaptureManager *)sharedCaptureManager;
- (BOOL)startCaptureWithDescriptor:(MTLCaptureDescriptor *)descriptor error:(NSError **)error;
- (void)stopCapture;
@property (readonly) BOOL isCapturing;
@end

@interface MTLSharedEventListener : NSObject
- (instancetype)initWithDispatchQueue:(dispatch_queue_t)dispatchQueue;
@property (readonly) dispatch_queue_t dispatchQueue;
@end

/* No Metal device is driven yet (the GPU track brings one). */
FOUNDATION_EXPORT id<MTLDevice> _Nullable MTLCreateSystemDefaultDevice(void);
FOUNDATION_EXPORT NSArray<id<MTLDevice>> *MTLCopyAllDevices(void);

NS_ASSUME_NONNULL_END
