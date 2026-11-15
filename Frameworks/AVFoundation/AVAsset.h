/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Assets: a media file, its tracks, their formats and the metadata, as
 * the media decoder (mediadecoder) finds them. Files only: no network.
 */

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreMedia/CoreMedia.h>
#import <AVFoundation/AVMediaFormat.h>
#import <AVFoundation/AVAsynchronousKeyValueLoading.h>
#import <AVFoundation/AVMetadataItem.h>

NS_ASSUME_NONNULL_BEGIN

@class AVAssetTrack;

@interface AVAsset : NSObject <NSCopying, AVAsynchronousKeyValueLoading>
+ (instancetype)assetWithURL:(NSURL *)URL;

@property (readonly) CMTime duration;
@property (readonly) float preferredRate;
@property (readonly) float preferredVolume;
@property (readonly) CGAffineTransform preferredTransform;
@property (readonly) CGSize naturalSize;
@property (readonly) BOOL providesPreciseDurationAndTiming;
@property (readonly) CMTime overallDurationHint;
- (void)cancelLoading;

@property (readonly) NSArray<AVAssetTrack *> *tracks;
- (nullable AVAssetTrack *)trackWithTrackID:(CMPersistentTrackID)trackID;
- (NSArray<AVAssetTrack *> *)tracksWithMediaType:(AVMediaType)mediaType;
- (NSArray<AVAssetTrack *> *)tracksWithMediaCharacteristic:(AVMediaCharacteristic)mediaCharacteristic;

@property (readonly, nullable) AVMetadataItem *creationDate;
@property (readonly, nullable) NSString *lyrics;
@property (readonly) NSArray<AVMetadataItem *> *commonMetadata;
@property (readonly) NSArray<AVMetadataItem *> *metadata;
@property (readonly) NSArray<AVMetadataFormat> *availableMetadataFormats;
- (NSArray<AVMetadataItem *> *)metadataForFormat:(AVMetadataFormat)format;
@property (readonly) NSArray<NSLocale *> *availableChapterLocales;
@property (readonly) NSArray<AVMediaCharacteristic> *availableMediaCharacteristicsWithMediaSelectionOptions;

@property (readonly, getter=isPlayable) BOOL playable;
@property (readonly, getter=isExportable) BOOL exportable;
@property (readonly, getter=isReadable) BOOL readable;
@property (readonly, getter=isComposable) BOOL composable;
@property (readonly) BOOL hasProtectedContent;
@property (readonly, getter=isCompatibleWithAirPlayVideo) BOOL compatibleWithAirPlayVideo;
@property (readonly) BOOL canContainFragments;
@property (readonly) BOOL containsFragments;

- (void)loadTrackWithTrackID:(CMPersistentTrackID)trackID
           completionHandler:(void (^)(AVAssetTrack *_Nullable track, NSError *_Nullable error))completionHandler;
- (void)loadTracksWithMediaType:(AVMediaType)mediaType
              completionHandler:(void (^)(NSArray<AVAssetTrack *> *_Nullable tracks, NSError *_Nullable error))completionHandler;
- (void)loadTracksWithMediaCharacteristic:(AVMediaCharacteristic)mediaCharacteristic
                        completionHandler:(void (^)(NSArray<AVAssetTrack *> *_Nullable tracks, NSError *_Nullable error))completionHandler;
- (void)loadMetadataForFormat:(AVMetadataFormat)format
            completionHandler:(void (^)(NSArray<AVMetadataItem *> *_Nullable metadata, NSError *_Nullable error))completionHandler;
@end

typedef NS_OPTIONS(NSUInteger, AVAssetReferenceRestrictions) {
    AVAssetReferenceRestrictionForbidNone = 0UL,
    AVAssetReferenceRestrictionForbidRemoteReferenceToLocal = (1UL << 0),
    AVAssetReferenceRestrictionForbidLocalReferenceToRemote = (1UL << 1),
    AVAssetReferenceRestrictionForbidCrossSiteReference = (1UL << 2),
    AVAssetReferenceRestrictionForbidLocalReferenceToLocal = (1UL << 3),
    AVAssetReferenceRestrictionForbidAll = 0xFFFFUL,
    AVAssetReferenceRestrictionDefaultPolicy = AVAssetReferenceRestrictionForbidLocalReferenceToRemote,
};

FOUNDATION_EXPORT NSString *const AVURLAssetPreferPreciseDurationAndTimingKey;
FOUNDATION_EXPORT NSString *const AVURLAssetReferenceRestrictionsKey;
FOUNDATION_EXPORT NSString *const AVURLAssetHTTPCookiesKey;
FOUNDATION_EXPORT NSString *const AVURLAssetAllowsCellularAccessKey;

@interface AVURLAsset : AVAsset
+ (NSArray<AVFileType> *)audiovisualTypes;
+ (NSArray<NSString *> *)audiovisualMIMETypes;
+ (BOOL)isPlayableExtendedMIMEType:(NSString *)extendedMIMEType;
+ (instancetype)URLAssetWithURL:(NSURL *)URL options:(nullable NSDictionary<NSString *, id> *)options;
- (instancetype)initWithURL:(NSURL *)URL options:(nullable NSDictionary<NSString *, id> *)options;
@property (readonly, copy) NSURL *URL;
@end

@interface AVAssetTrack : NSObject <NSCopying, AVAsynchronousKeyValueLoading>
@property (readonly, weak) AVAsset *asset;
@property (readonly) CMPersistentTrackID trackID;
@property (readonly) AVMediaType mediaType;
@property (readonly) NSArray *formatDescriptions;
@property (readonly, getter=isPlayable) BOOL playable;
@property (readonly, getter=isDecodable) BOOL decodable;
@property (readonly, getter=isEnabled) BOOL enabled;
@property (readonly, getter=isSelfContained) BOOL selfContained;
@property (readonly) long long totalSampleDataLength;
- (BOOL)hasMediaCharacteristic:(AVMediaCharacteristic)mediaCharacteristic;
@property (readonly) CMTimeRange timeRange;
@property (readonly) CMTimeScale naturalTimeScale;
@property (readonly) float estimatedDataRate;
@property (readonly, nullable) NSString *languageCode;
@property (readonly, nullable) NSString *extendedLanguageTag;
@property (readonly) CGSize naturalSize;
@property (readonly) CGAffineTransform preferredTransform;
@property (readonly) float preferredVolume;
@property (readonly) BOOL hasAudioSampleDependencies;
@property (readonly) float nominalFrameRate;
@property (readonly) CMTime minFrameDuration;
@property (readonly) BOOL requiresFrameReordering;
@property (readonly) NSArray<AVMetadataItem *> *commonMetadata;
@property (readonly) NSArray<AVMetadataItem *> *metadata;
@property (readonly) NSArray<AVMetadataFormat> *availableMetadataFormats;
- (NSArray<AVMetadataItem *> *)metadataForFormat:(AVMetadataFormat)format;
- (CMTime)samplePresentationTimeForTrackTime:(CMTime)trackTime;
@end

NS_ASSUME_NONNULL_END

#import <AVFoundation/AVAssetReader.h>
