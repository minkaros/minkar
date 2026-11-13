/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Assets: a media file and its tracks, and the reader that gives a track's
 * samples. SHORTCUT(AU5): no demuxer yet, so an asset has no tracks.
 */

#import <Foundation/Foundation.h>
#import <CoreMedia/CoreMedia.h>
#import <AVFoundation/AVMediaFormat.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, AVKeyValueStatus) {
    AVKeyValueStatusUnknown,
    AVKeyValueStatusLoading,
    AVKeyValueStatusLoaded,
    AVKeyValueStatusFailed,
    AVKeyValueStatusCancelled,
};

@class AVAssetTrack;

@interface AVAsset : NSObject <NSCopying>
+ (instancetype)assetWithURL:(NSURL *)URL;
@property (readonly) CMTime duration;
@property (readonly) NSArray<AVAssetTrack *> *tracks;
- (NSArray<AVAssetTrack *> *)tracksWithMediaType:(AVMediaType)mediaType;
@property (readonly, getter=isPlayable) BOOL playable;
@property (readonly, getter=isReadable) BOOL readable;
- (AVKeyValueStatus)statusOfValueForKey:(NSString *)key error:(NSError **)outError;
- (void)loadValuesAsynchronouslyForKeys:(NSArray<NSString *> *)keys completionHandler:(nullable void (^)(void))handler;
@end

@interface AVURLAsset : AVAsset
+ (instancetype)URLAssetWithURL:(NSURL *)URL options:(nullable NSDictionary<NSString *, id> *)options;
- (instancetype)initWithURL:(NSURL *)URL options:(nullable NSDictionary<NSString *, id> *)options;
@property (readonly, copy) NSURL *URL;
@end

@interface AVAssetTrack : NSObject <NSCopying>
@property (readonly, weak) AVAsset *asset;
@property (readonly) CMPersistentTrackID trackID;
@property (readonly) AVMediaType mediaType;
@property (readonly) NSArray *formatDescriptions;
@property (readonly) CMTimeRange timeRange;
@property (readonly) CGSize naturalSize;
@property (readonly) float nominalFrameRate;
@end

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
- (nullable CMSampleBufferRef)copyNextSampleBuffer CF_RETURNS_RETAINED;
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
