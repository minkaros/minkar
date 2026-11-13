/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <Metal/Metal.h>
#include <float.h>

@implementation MTLTextureDescriptor

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _textureType = MTLTextureType2D;
        _pixelFormat = MTLPixelFormatRGBA8Unorm;
        _width = 1;
        _height = 1;
        _depth = 1;
        _mipmapLevelCount = 1;
        _sampleCount = 1;
        _arrayLength = 1;
        _resourceOptions = 0;
        _cpuCacheMode = MTLCPUCacheModeDefaultCache;
        _storageMode = MTLStorageModeManaged;
        _usage = MTLTextureUsageShaderRead;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    MTLTextureDescriptor *copy = [[[self class] alloc] init];
    copy.textureType = self.textureType;
    copy.pixelFormat = self.pixelFormat;
    copy.width = self.width;
    copy.height = self.height;
    copy.depth = self.depth;
    copy.mipmapLevelCount = self.mipmapLevelCount;
    copy.sampleCount = self.sampleCount;
    copy.arrayLength = self.arrayLength;
    copy.resourceOptions = self.resourceOptions;
    copy.cpuCacheMode = self.cpuCacheMode;
    copy.storageMode = self.storageMode;
    copy.usage = self.usage;
    return copy;
}

@end

@implementation MTLSamplerDescriptor

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _minFilter = MTLSamplerMinMagFilterNearest;
        _magFilter = MTLSamplerMinMagFilterNearest;
        _mipFilter = MTLSamplerMipFilterNotMipmapped;
        _maxAnisotropy = 1;
        _sAddressMode = MTLSamplerAddressModeClampToEdge;
        _tAddressMode = MTLSamplerAddressModeClampToEdge;
        _rAddressMode = MTLSamplerAddressModeClampToEdge;
        _normalizedCoordinates = YES;
        _lodMinClamp = 0;
        _lodMaxClamp = FLT_MAX;
        _compareFunction = MTLCompareFunctionNever;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    MTLSamplerDescriptor *copy = [[[self class] alloc] init];
    copy.minFilter = self.minFilter;
    copy.magFilter = self.magFilter;
    copy.mipFilter = self.mipFilter;
    copy.maxAnisotropy = self.maxAnisotropy;
    copy.sAddressMode = self.sAddressMode;
    copy.tAddressMode = self.tAddressMode;
    copy.rAddressMode = self.rAddressMode;
    copy.normalizedCoordinates = self.normalizedCoordinates;
    copy.lodMinClamp = self.lodMinClamp;
    copy.lodMaxClamp = self.lodMaxClamp;
    copy.compareFunction = self.compareFunction;
    copy.label = self.label;
    return copy;
}

@end

@implementation MTLStencilDescriptor

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _stencilCompareFunction = MTLCompareFunctionAlways;
        _stencilFailureOperation = MTLStencilOperationKeep;
        _depthFailureOperation = MTLStencilOperationKeep;
        _depthStencilPassOperation = MTLStencilOperationKeep;
        _readMask = 0xffffffff;
        _writeMask = 0xffffffff;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    MTLStencilDescriptor *copy = [[[self class] alloc] init];
    copy.stencilCompareFunction = self.stencilCompareFunction;
    copy.stencilFailureOperation = self.stencilFailureOperation;
    copy.depthFailureOperation = self.depthFailureOperation;
    copy.depthStencilPassOperation = self.depthStencilPassOperation;
    copy.readMask = self.readMask;
    copy.writeMask = self.writeMask;
    return copy;
}

@end

@implementation MTLDepthStencilDescriptor

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _depthCompareFunction = MTLCompareFunctionAlways;
        _depthWriteEnabled = NO;
        _frontFaceStencil = [[MTLStencilDescriptor alloc] init];
        _backFaceStencil = [[MTLStencilDescriptor alloc] init];
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    MTLDepthStencilDescriptor *copy = [[[self class] alloc] init];
    copy.depthCompareFunction = self.depthCompareFunction;
    copy.depthWriteEnabled = self.depthWriteEnabled;
    copy.frontFaceStencil = [self.frontFaceStencil copy];
    copy.backFaceStencil = [self.backFaceStencil copy];
    copy.label = self.label;
    return copy;
}

@end

@implementation MTLVertexAttributeDescriptor

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _format = MTLVertexFormatInvalid;
        _offset = 0;
        _bufferIndex = 0;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    MTLVertexAttributeDescriptor *copy = [[[self class] alloc] init];
    copy.format = self.format;
    copy.offset = self.offset;
    copy.bufferIndex = self.bufferIndex;
    return copy;
}

@end

@implementation MTLVertexBufferLayoutDescriptor

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _stride = 0;
        _stepFunction = MTLVertexStepFunctionPerVertex;
        _stepRate = 1;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    MTLVertexBufferLayoutDescriptor *copy = [[[self class] alloc] init];
    copy.stride = self.stride;
    copy.stepFunction = self.stepFunction;
    copy.stepRate = self.stepRate;
    return copy;
}

@end

@implementation MTLRenderPipelineColorAttachmentDescriptor

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _pixelFormat = MTLPixelFormatInvalid;
        _blendingEnabled = NO;
        _sourceRGBBlendFactor = MTLBlendFactorOne;
        _destinationRGBBlendFactor = MTLBlendFactorZero;
        _rgbBlendOperation = MTLBlendOperationAdd;
        _sourceAlphaBlendFactor = MTLBlendFactorOne;
        _destinationAlphaBlendFactor = MTLBlendFactorZero;
        _alphaBlendOperation = MTLBlendOperationAdd;
        _writeMask = MTLColorWriteMaskAll;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    MTLRenderPipelineColorAttachmentDescriptor *copy = [[[self class] alloc] init];
    copy.pixelFormat = self.pixelFormat;
    copy.blendingEnabled = self.blendingEnabled;
    copy.sourceRGBBlendFactor = self.sourceRGBBlendFactor;
    copy.destinationRGBBlendFactor = self.destinationRGBBlendFactor;
    copy.rgbBlendOperation = self.rgbBlendOperation;
    copy.sourceAlphaBlendFactor = self.sourceAlphaBlendFactor;
    copy.destinationAlphaBlendFactor = self.destinationAlphaBlendFactor;
    copy.alphaBlendOperation = self.alphaBlendOperation;
    copy.writeMask = self.writeMask;
    return copy;
}

@end

@implementation MTLCompileOptions

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _fastMathEnabled = YES;
        _languageVersion = MTLLanguageVersion2_2;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    MTLCompileOptions *copy = [[[self class] alloc] init];
    copy.preprocessorMacros = self.preprocessorMacros;
    copy.fastMathEnabled = self.fastMathEnabled;
    copy.languageVersion = self.languageVersion;
    return copy;
}

@end

@implementation MTLCaptureDescriptor

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _destination = MTLCaptureDestinationDeveloperTools;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    MTLCaptureDescriptor *copy = [[[self class] alloc] init];
    copy.captureObject = self.captureObject;
    copy.destination = self.destination;
    copy.outputURL = self.outputURL;
    return copy;
}

@end

/* Arrays of descriptors, made on first use as Metal's are: each holds the
   class of its items and the items set or made so far. */
#define DESCRIPTOR_ARRAY(Array, Item) \
@implementation Array { \
    NSMutableDictionary<NSNumber *, Item *> *_items; \
} \
- (instancetype)init { \
    if ((self = [super init]) != nil) \
        _items = [NSMutableDictionary dictionary]; \
    return self; \
} \
- (Item *)objectAtIndexedSubscript:(NSUInteger)index { \
    Item *item = _items[@(index)]; \
    if (item == nil) \
        _items[@(index)] = item = [[Item alloc] init]; \
    return item; \
} \
- (void)setObject:(Item *)item atIndexedSubscript:(NSUInteger)index { \
    if (item != nil) \
        _items[@(index)] = [item copy]; \
    else \
        [_items removeObjectForKey:@(index)]; \
} \
- (void)_copyInto:(Array *)other { \
    for (NSNumber *key in _items) \
        other[[key unsignedIntegerValue]] = _items[key]; \
} \
@end

@interface MTLVertexAttributeDescriptorArray ()
- (void)_copyInto:(MTLVertexAttributeDescriptorArray *)other;
@end
@interface MTLVertexBufferLayoutDescriptorArray ()
- (void)_copyInto:(MTLVertexBufferLayoutDescriptorArray *)other;
@end
@interface MTLRenderPipelineColorAttachmentDescriptorArray ()
- (void)_copyInto:(MTLRenderPipelineColorAttachmentDescriptorArray *)other;
@end
@interface MTLRenderPassColorAttachmentDescriptorArray ()
- (void)_copyInto:(MTLRenderPassColorAttachmentDescriptorArray *)other;
@end

DESCRIPTOR_ARRAY(MTLVertexAttributeDescriptorArray, MTLVertexAttributeDescriptor)
DESCRIPTOR_ARRAY(MTLVertexBufferLayoutDescriptorArray, MTLVertexBufferLayoutDescriptor)
DESCRIPTOR_ARRAY(MTLRenderPipelineColorAttachmentDescriptorArray, MTLRenderPipelineColorAttachmentDescriptor)
DESCRIPTOR_ARRAY(MTLRenderPassColorAttachmentDescriptorArray, MTLRenderPassColorAttachmentDescriptor)

@implementation MTLVertexDescriptor

+ (MTLVertexDescriptor *)vertexDescriptor {
    return [[self alloc] init];
}

- (instancetype)init {
    if ((self = [super init]) != nil)
        [self reset];
    return self;
}

- (void)reset {
    _layouts = [[MTLVertexBufferLayoutDescriptorArray alloc] init];
    _attributes = [[MTLVertexAttributeDescriptorArray alloc] init];
}

- (id)copyWithZone:(NSZone *)zone {
    MTLVertexDescriptor *copy = [[MTLVertexDescriptor alloc] init];
    [_layouts _copyInto:copy->_layouts];
    [_attributes _copyInto:copy->_attributes];
    return copy;
}

@end

@implementation MTLRenderPipelineDescriptor

- (instancetype)init {
    if ((self = [super init]) != nil)
        [self reset];
    return self;
}

- (void)reset {
    _label = nil;
    _vertexFunction = nil;
    _fragmentFunction = nil;
    _vertexDescriptor = nil;
    _sampleCount = 1;
    _rasterSampleCount = 1;
    _alphaToCoverageEnabled = NO;
    _rasterizationEnabled = YES;
    _colorAttachments = [[MTLRenderPipelineColorAttachmentDescriptorArray alloc] init];
    _depthAttachmentPixelFormat = MTLPixelFormatInvalid;
    _stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
}

- (id)copyWithZone:(NSZone *)zone {
    MTLRenderPipelineDescriptor *copy = [[MTLRenderPipelineDescriptor alloc] init];
    copy.label = _label;
    copy.vertexFunction = _vertexFunction;
    copy.fragmentFunction = _fragmentFunction;
    copy.vertexDescriptor = _vertexDescriptor;
    copy.sampleCount = _sampleCount;
    copy.rasterSampleCount = _rasterSampleCount;
    copy.alphaToCoverageEnabled = _alphaToCoverageEnabled;
    copy.rasterizationEnabled = _rasterizationEnabled;
    copy.depthAttachmentPixelFormat = _depthAttachmentPixelFormat;
    copy.stencilAttachmentPixelFormat = _stencilAttachmentPixelFormat;
    [_colorAttachments _copyInto:copy->_colorAttachments];
    return copy;
}

@end

@implementation MTLRenderPassAttachmentDescriptor

- (id)copyWithZone:(NSZone *)zone {
    MTLRenderPassAttachmentDescriptor *copy = [[[self class] alloc] init];
    copy.texture = _texture;
    copy.level = _level;
    copy.slice = _slice;
    copy.loadAction = _loadAction;
    copy.storeAction = _storeAction;
    return copy;
}

@end

@implementation MTLRenderPassColorAttachmentDescriptor

- (instancetype)init {
    if ((self = [super init]) != nil)
        _clearColor = (MTLClearColor){ 0, 0, 0, 1 };
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    MTLRenderPassColorAttachmentDescriptor *copy = [super copyWithZone:zone];
    copy.clearColor = _clearColor;
    return copy;
}

@end

@implementation MTLRenderPassDepthAttachmentDescriptor

- (instancetype)init {
    if ((self = [super init]) != nil)
        _clearDepth = 1.0;
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    MTLRenderPassDepthAttachmentDescriptor *copy = [super copyWithZone:zone];
    copy.clearDepth = _clearDepth;
    return copy;
}

@end

@implementation MTLRenderPassStencilAttachmentDescriptor

- (id)copyWithZone:(NSZone *)zone {
    MTLRenderPassStencilAttachmentDescriptor *copy = [super copyWithZone:zone];
    copy.clearStencil = _clearStencil;
    return copy;
}

@end

@implementation MTLRenderPassDescriptor

+ (MTLRenderPassDescriptor *)renderPassDescriptor {
    return [[self alloc] init];
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _colorAttachments = [[MTLRenderPassColorAttachmentDescriptorArray alloc] init];
        _depthAttachment = [[MTLRenderPassDepthAttachmentDescriptor alloc] init];
        _stencilAttachment = [[MTLRenderPassStencilAttachmentDescriptor alloc] init];
    }
    return self;
}

- (void)setDepthAttachment:(MTLRenderPassDepthAttachmentDescriptor *)attachment {
    _depthAttachment = attachment != nil ? [attachment copy] : [[MTLRenderPassDepthAttachmentDescriptor alloc] init];
}

- (void)setStencilAttachment:(MTLRenderPassStencilAttachmentDescriptor *)attachment {
    _stencilAttachment = attachment != nil ? [attachment copy] : [[MTLRenderPassStencilAttachmentDescriptor alloc] init];
}

- (id)copyWithZone:(NSZone *)zone {
    MTLRenderPassDescriptor *copy = [[MTLRenderPassDescriptor alloc] init];
    [_colorAttachments _copyInto:copy->_colorAttachments];
    copy.depthAttachment = _depthAttachment;
    copy.stencilAttachment = _stencilAttachment;
    copy.renderTargetWidth = _renderTargetWidth;
    copy.renderTargetHeight = _renderTargetHeight;
    return copy;
}

@end

@implementation MTLFunctionConstantValues {
    NSMutableDictionary *_values;
}

- (instancetype)init {
    if ((self = [super init]) != nil)
        _values = [NSMutableDictionary dictionary];
    return self;
}

/* The value's bytes, by index or name; at most 16 bytes (a float4). */
- (void)setConstantValue:(const void *)value type:(NSUInteger)type atIndex:(NSUInteger)index {
    _values[@(index)] = @[ @(type), [NSData dataWithBytes:value length:16] ];
}

- (void)setConstantValue:(const void *)value type:(NSUInteger)type withName:(NSString *)name {
    _values[name] = @[ @(type), [NSData dataWithBytes:value length:16] ];
}

- (void)reset {
    [_values removeAllObjects];
}

- (id)copyWithZone:(NSZone *)zone {
    MTLFunctionConstantValues *copy = [[MTLFunctionConstantValues alloc] init];
    [copy->_values addEntriesFromDictionary:_values];
    return copy;
}

@end

@implementation MTLCaptureManager

+ (MTLCaptureManager *)sharedCaptureManager {
    static MTLCaptureManager *manager;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        manager = [[MTLCaptureManager alloc] init];
    });
    return manager;
}

/* No device, nothing to capture. */
- (BOOL)startCaptureWithDescriptor:(MTLCaptureDescriptor *)descriptor error:(NSError **)error {
    if (error != NULL)
        *error = [NSError errorWithDomain:@"MTLCaptureErrorDomain" code:1 /* MTLCaptureErrorNotSupported */ userInfo:nil];
    return NO;
}

- (void)stopCapture {
}

- (BOOL)isCapturing {
    return NO;
}

@end

@implementation MTLSharedEventListener

- (instancetype)init {
    return [self initWithDispatchQueue:dispatch_queue_create("MTLSharedEventListener", DISPATCH_QUEUE_SERIAL)];
}

- (instancetype)initWithDispatchQueue:(dispatch_queue_t)dispatchQueue {
    if ((self = [super init]) != nil)
        _dispatchQueue = dispatchQueue;
    return self;
}

@end

id<MTLDevice> MTLCreateSystemDefaultDevice(void) {
    return nil;
}

NSArray<id<MTLDevice>> *MTLCopyAllDevices(void) {
    return @[];
}
