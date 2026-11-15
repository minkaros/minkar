/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The asset reader: a track's samples, decoded, one after another.
 */

#import <Foundation/Foundation.h>
#import <CoreMedia/CoreMedia.h>
#import <AVFoundation/AVMediaFormat.h>

NS_ASSUME_NONNULL_BEGIN

@class AVAsset, AVAssetTrack;

typedef NS_ENUM(NSInteger, AVAssetReaderStatus) {
    AVAssetReaderStatusUnknown = 0,
    AVAssetReaderStatusReading,
    AVAssetReaderStatusCompleted,
    AVAssetReaderStatusFailed,
    AVAssetReaderStatusCancelled,
};

@interface AVAssetReaderOutput : NSObject
@property (readonly) AVMediaType mediaType;
@property BOOL alwaysCopiesSampleData;
@property BOOL supportsRandomAccess;
- (nullable CMSampleBufferRef)copyNextSampleBuffer CF_RETURNS_RETAINED;
- (void)resetForReadingTimeRanges:(NSArray<NSValue *> *)timeRanges;
- (void)markConfigurationAsFinal;
@end

@interface AVAssetReaderTrackOutput : AVAssetReaderOutput
+ (instancetype)assetReaderTrackOutputWithTrack:(AVAssetTrack *)track
                                 outputSettings:(nullable NSDictionary<NSString *, id> *)outputSettings;
- (instancetype)initWithTrack:(AVAssetTrack *)track outputSettings:(nullable NSDictionary<NSString *, id> *)outputSettings;
@property (readonly) AVAssetTrack *track;
@property (nullable, readonly) NSDictionary<NSString *, id> *outputSettings;
@end

@interface AVAssetReader : NSObject
+ (nullable instancetype)assetReaderWithAsset:(AVAsset *)asset error:(NSError **)outError;
- (nullable instancetype)initWithAsset:(AVAsset *)asset error:(NSError **)outError;
@property (readonly) AVAsset *asset;
@property (readonly) AVAssetReaderStatus status;
@property (nullable, readonly) NSError *error;
@property CMTimeRange timeRange;
@property (readonly) NSArray<AVAssetReaderOutput *> *outputs;
- (BOOL)canAddOutput:(AVAssetReaderOutput *)output;
- (void)addOutput:(AVAssetReaderOutput *)output;
- (BOOL)startReading;
- (void)cancelReading;
@end

NS_ASSUME_NONNULL_END
