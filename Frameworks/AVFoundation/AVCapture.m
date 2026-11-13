/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * No camera or microphone is driven yet: there are no capture devices, so
 * no input can be made; sessions run with what they have (nothing).
 */

#import <AVFoundation/AVCapture.h>

#define KEY(name) NSString *const name = @#name
KEY(AVCaptureDeviceTypeBuiltInWideAngleCamera);
KEY(AVCaptureDeviceTypeExternalUnknown);
KEY(AVCaptureDeviceTypeBuiltInMicrophone);
KEY(AVCaptureDeviceWasConnectedNotification);
KEY(AVCaptureDeviceWasDisconnectedNotification);
KEY(AVCaptureSessionDidStartRunningNotification);
KEY(AVCaptureSessionDidStopRunningNotification);
KEY(AVCaptureSessionRuntimeErrorNotification);
KEY(AVCaptureSessionErrorKey);
KEY(AVCaptureSessionPresetHigh);
KEY(AVCaptureSessionPresetMedium);
KEY(AVCaptureSessionPresetLow);
KEY(AVCaptureSessionPreset352x288);
KEY(AVCaptureSessionPreset640x480);
KEY(AVCaptureSessionPreset1280x720);

@implementation AVCaptureDevice

+ (NSArray<AVCaptureDevice *> *)devices {
    return @[];
}

+ (NSArray<AVCaptureDevice *> *)devicesWithMediaType:(AVMediaType)mediaType {
    return @[];
}

+ (AVCaptureDevice *)defaultDeviceWithMediaType:(AVMediaType)mediaType {
    return nil;
}

+ (AVCaptureDevice *)deviceWithUniqueID:(NSString *)deviceUniqueID {
    return nil;
}

/* Nothing to ask permission for: no device can be used. */
+ (AVAuthorizationStatus)authorizationStatusForMediaType:(AVMediaType)mediaType {
    return AVAuthorizationStatusRestricted;
}

+ (void)requestAccessForMediaType:(AVMediaType)mediaType completionHandler:(void (^)(BOOL))handler {
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        handler(NO);
    });
}

- (NSArray *)formats {
    return @[];
}

- (BOOL)hasMediaType:(AVMediaType)mediaType {
    return NO;
}

- (BOOL)lockForConfiguration:(NSError **)outError {
    return NO;
}

- (void)unlockForConfiguration {
}

@end

@implementation AVCaptureDeviceDiscoverySession

+ (instancetype)discoverySessionWithDeviceTypes:(NSArray<AVCaptureDeviceType> *)deviceTypes
                                      mediaType:(AVMediaType)mediaType position:(AVCaptureDevicePosition)position {
    return [[self alloc] init];
}

- (NSArray<AVCaptureDevice *> *)devices {
    return @[];
}

@end

@implementation AVCaptureInput
@end

@implementation AVCaptureDeviceInput

+ (instancetype)deviceInputWithDevice:(AVCaptureDevice *)device error:(NSError **)outError {
    return [[self alloc] initWithDevice:device error:outError];
}

- (instancetype)initWithDevice:(AVCaptureDevice *)device error:(NSError **)outError {
    if (outError != NULL)
        *outError = [NSError errorWithDomain:@"AVFoundationErrorDomain" code:-11814 /* AVErrorDeviceNotConnected */
                                    userInfo:nil];
    return nil;
}

@end

@implementation AVCaptureOutput
@end

@implementation AVCaptureVideoDataOutput {
    __weak id _delegate;
    dispatch_queue_t _queue;
}

- (void)setSampleBufferDelegate:(id<AVCaptureVideoDataOutputSampleBufferDelegate>)delegate queue:(dispatch_queue_t)queue {
    _delegate = delegate;
    _queue = queue;
}

- (id<AVCaptureVideoDataOutputSampleBufferDelegate>)sampleBufferDelegate {
    return _delegate;
}

- (dispatch_queue_t)sampleBufferCallbackQueue {
    return _queue;
}

- (NSArray<NSNumber *> *)availableVideoCVPixelFormatTypes {
    return @[];
}

@end

@implementation AVCaptureSession {
    NSMutableArray *_inputs;
    NSMutableArray *_outputs;
    BOOL _running;
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _inputs = [NSMutableArray array];
        _outputs = [NSMutableArray array];
        _sessionPreset = AVCaptureSessionPresetHigh;
    }
    return self;
}

- (NSArray *)inputs {
    return [_inputs copy];
}

- (NSArray *)outputs {
    return [_outputs copy];
}

- (BOOL)isRunning {
    return _running;
}

- (BOOL)canSetSessionPreset:(AVCaptureSessionPreset)preset {
    return YES;
}

- (BOOL)canAddInput:(AVCaptureInput *)input {
    return input != nil && ![_inputs containsObject:input];
}

- (void)addInput:(AVCaptureInput *)input {
    if ([self canAddInput:input])
        [_inputs addObject:input];
}

- (void)removeInput:(AVCaptureInput *)input {
    [_inputs removeObject:input];
}

- (BOOL)canAddOutput:(AVCaptureOutput *)output {
    return output != nil && ![_outputs containsObject:output];
}

- (void)addOutput:(AVCaptureOutput *)output {
    if ([self canAddOutput:output])
        [_outputs addObject:output];
}

- (void)removeOutput:(AVCaptureOutput *)output {
    [_outputs removeObject:output];
}

- (void)beginConfiguration {
}

- (void)commitConfiguration {
}

- (void)startRunning {
    if (_running)
        return;
    _running = YES;
    [[NSNotificationCenter defaultCenter] postNotificationName:AVCaptureSessionDidStartRunningNotification object:self];
}

- (void)stopRunning {
    if (!_running)
        return;
    _running = NO;
    [[NSNotificationCenter defaultCenter] postNotificationName:AVCaptureSessionDidStopRunningNotification object:self];
}

@end
