/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AVFoundation/AVAsset.h>

static NSError *noDemuxer(void) {
    return [NSError errorWithDomain:@"AVFoundationErrorDomain" code:-11828 /* AVErrorFileFormatNotRecognized */
        userInfo:@{ NSLocalizedDescriptionKey: @"Media files cannot be read on this system yet." }];
}

@implementation AVAsset

+ (instancetype)assetWithURL:(NSURL *)URL {
    return [AVURLAsset URLAssetWithURL:URL options:nil];
}

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

- (CMTime)duration {
    return kCMTimeInvalid;
}

- (NSArray<AVAssetTrack *> *)tracks {
    return @[];
}

- (NSArray<AVAssetTrack *> *)tracksWithMediaType:(AVMediaType)mediaType {
    return @[];
}

- (BOOL)isPlayable {
    return NO;
}

- (BOOL)isReadable {
    return NO;
}

- (AVKeyValueStatus)statusOfValueForKey:(NSString *)key error:(NSError **)outError {
    if (outError != NULL)
        *outError = noDemuxer();
    return AVKeyValueStatusFailed;
}

- (void)loadValuesAsynchronouslyForKeys:(NSArray<NSString *> *)keys completionHandler:(void (^)(void))handler {
    if (handler != nil)
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), handler);
}

@end

@implementation AVURLAsset

+ (instancetype)URLAssetWithURL:(NSURL *)URL options:(NSDictionary *)options {
    return [[self alloc] initWithURL:URL options:options];
}

- (instancetype)initWithURL:(NSURL *)URL options:(NSDictionary *)options {
    if ((self = [super init]) != nil)
        _URL = [URL copy];
    return self;
}

@end

@implementation AVAssetTrack

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

@end

@implementation AVAssetReaderOutput

- (CMSampleBufferRef)copyNextSampleBuffer {
    return NULL;
}

@end

@implementation AVAssetReaderTrackOutput

+ (instancetype)assetReaderTrackOutputWithTrack:(AVAssetTrack *)track outputSettings:(NSDictionary *)outputSettings {
    return [[self alloc] initWithTrack:track outputSettings:outputSettings];
}

- (instancetype)initWithTrack:(AVAssetTrack *)track outputSettings:(NSDictionary *)outputSettings {
    if ((self = [super init]) != nil) {
        _track = track;
        _outputSettings = [outputSettings copy];
    }
    return self;
}

- (AVMediaType)mediaType {
    return _track.mediaType;
}

@end

@implementation AVAssetReader {
    NSMutableArray *_outputs;
}

+ (instancetype)assetReaderWithAsset:(AVAsset *)asset error:(NSError **)outError {
    return [[self alloc] initWithAsset:asset error:outError];
}

- (instancetype)initWithAsset:(AVAsset *)asset error:(NSError **)outError {
    if ((self = [super init]) != nil) {
        _asset = asset;
        _outputs = [NSMutableArray array];
        _timeRange = CMTimeRangeMake(kCMTimeZero, kCMTimePositiveInfinity);
    }
    return self;
}

- (NSArray<AVAssetReaderOutput *> *)outputs {
    return [_outputs copy];
}

- (BOOL)canAddOutput:(AVAssetReaderOutput *)output {
    return _status == AVAssetReaderStatusUnknown && output != nil;
}

- (void)addOutput:(AVAssetReaderOutput *)output {
    if ([self canAddOutput:output])
        [_outputs addObject:output];
}

/* No demuxer yet: reading fails. */
- (BOOL)startReading {
    _status = AVAssetReaderStatusFailed;
    _error = noDemuxer();
    return NO;
}

- (void)cancelReading {
    if (_status == AVAssetReaderStatusReading || _status == AVAssetReaderStatusUnknown)
        _status = AVAssetReaderStatusCancelled;
}

@end
