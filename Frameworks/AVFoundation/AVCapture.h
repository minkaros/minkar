/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <AVFoundation/AVMediaFormat.h>

NS_ASSUME_NONNULL_BEGIN

typedef NSString *AVCaptureDeviceType;
typedef NSString *AVCaptureSessionPreset;

FOUNDATION_EXPORT AVCaptureDeviceType const AVCaptureDeviceTypeBuiltInWideAngleCamera;
FOUNDATION_EXPORT AVCaptureDeviceType const AVCaptureDeviceTypeExternalUnknown;
FOUNDATION_EXPORT AVCaptureDeviceType const AVCaptureDeviceTypeBuiltInMicrophone;

FOUNDATION_EXPORT NSNotificationName const AVCaptureDeviceWasConnectedNotification;
FOUNDATION_EXPORT NSNotificationName const AVCaptureDeviceWasDisconnectedNotification;
FOUNDATION_EXPORT NSNotificationName const AVCaptureSessionDidStartRunningNotification;
FOUNDATION_EXPORT NSNotificationName const AVCaptureSessionDidStopRunningNotification;
FOUNDATION_EXPORT NSNotificationName const AVCaptureSessionRuntimeErrorNotification;
FOUNDATION_EXPORT NSString *const AVCaptureSessionErrorKey;

FOUNDATION_EXPORT AVCaptureSessionPreset const AVCaptureSessionPresetHigh;
FOUNDATION_EXPORT AVCaptureSessionPreset const AVCaptureSessionPresetMedium;
FOUNDATION_EXPORT AVCaptureSessionPreset const AVCaptureSessionPresetLow;
FOUNDATION_EXPORT AVCaptureSessionPreset const AVCaptureSessionPreset352x288;
FOUNDATION_EXPORT AVCaptureSessionPreset const AVCaptureSessionPreset640x480;
FOUNDATION_EXPORT AVCaptureSessionPreset const AVCaptureSessionPreset1280x720;

typedef NS_ENUM(NSInteger, AVCaptureDevicePosition) {
    AVCaptureDevicePositionUnspecified = 0,
    AVCaptureDevicePositionBack = 1,
    AVCaptureDevicePositionFront = 2,
};

@interface AVCaptureDevice : NSObject
+ (NSArray<AVCaptureDevice *> *)devices;
+ (NSArray<AVCaptureDevice *> *)devicesWithMediaType:(AVMediaType)mediaType;
+ (nullable AVCaptureDevice *)defaultDeviceWithMediaType:(AVMediaType)mediaType;
+ (nullable AVCaptureDevice *)deviceWithUniqueID:(NSString *)deviceUniqueID;
+ (AVAuthorizationStatus)authorizationStatusForMediaType:(AVMediaType)mediaType;
+ (void)requestAccessForMediaType:(AVMediaType)mediaType completionHandler:(void (^)(BOOL granted))handler;
@property (readonly) NSString *uniqueID;
@property (readonly) NSString *modelID;
@property (readonly) NSString *localizedName;
@property (readonly) AVCaptureDeviceType deviceType;
@property (readonly) AVCaptureDevicePosition position;
@property (readonly, getter=isConnected) BOOL connected;
@property (readonly) NSArray *formats;
- (BOOL)hasMediaType:(AVMediaType)mediaType;
- (BOOL)lockForConfiguration:(NSError **)outError;
- (void)unlockForConfiguration;
@end

@interface AVCaptureDeviceDiscoverySession : NSObject
+ (instancetype)discoverySessionWithDeviceTypes:(NSArray<AVCaptureDeviceType> *)deviceTypes
                                      mediaType:(nullable AVMediaType)mediaType
                                       position:(AVCaptureDevicePosition)position;
@property (readonly) NSArray<AVCaptureDevice *> *devices;
@end

@interface AVCaptureInput : NSObject
@end

@interface AVCaptureDeviceInput : AVCaptureInput
+ (nullable instancetype)deviceInputWithDevice:(AVCaptureDevice *)device error:(NSError **)outError;
- (nullable instancetype)initWithDevice:(AVCaptureDevice *)device error:(NSError **)outError;
@property (readonly) AVCaptureDevice *device;
@end

@interface AVCaptureOutput : NSObject
@end

@protocol AVCaptureVideoDataOutputSampleBufferDelegate;

@interface AVCaptureVideoDataOutput : AVCaptureOutput
@property (nullable, copy) NSDictionary<NSString *, id> *videoSettings;
@property BOOL alwaysDiscardsLateVideoFrames;
@property (nullable, readonly) id<AVCaptureVideoDataOutputSampleBufferDelegate> sampleBufferDelegate;
@property (nullable, readonly) dispatch_queue_t sampleBufferCallbackQueue;
- (void)setSampleBufferDelegate:(nullable id<AVCaptureVideoDataOutputSampleBufferDelegate>)delegate
                          queue:(nullable dispatch_queue_t)queue;
@property (readonly) NSArray<NSNumber *> *availableVideoCVPixelFormatTypes;
@end

@interface AVCaptureSession : NSObject
@property (copy) AVCaptureSessionPreset sessionPreset;
@property (readonly) NSArray<AVCaptureInput *> *inputs;
@property (readonly) NSArray<AVCaptureOutput *> *outputs;
@property (readonly, getter=isRunning) BOOL running;
- (BOOL)canSetSessionPreset:(AVCaptureSessionPreset)preset;
- (BOOL)canAddInput:(AVCaptureInput *)input;
- (void)addInput:(AVCaptureInput *)input;
- (void)removeInput:(AVCaptureInput *)input;
- (BOOL)canAddOutput:(AVCaptureOutput *)output;
- (void)addOutput:(AVCaptureOutput *)output;
- (void)removeOutput:(AVCaptureOutput *)output;
- (void)beginConfiguration;
- (void)commitConfiguration;
- (void)startRunning;
- (void)stopRunning;
@end

@protocol AVCaptureVideoDataOutputSampleBufferDelegate <NSObject>
@optional
- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(id)connection;
@end

NS_ASSUME_NONNULL_END
