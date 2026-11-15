/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The asset reader. Each track output reads through a decoding session of
 * its own, so outputs seek and end independently. Video comes as BGRA or
 * 4:2:0 bi-planar pixel buffers, audio as interleaved linear PCM, 32-bit
 * float or 16-bit integer, at the rate and channels asked for.
 * SHORTCUT(AU5): nil output settings give decoded samples (BGRA, float),
 * not the file's compressed ones; the decoder has no passthrough yet.
 */

#import <AVFoundation/AVAssetReader.h>
#import <AVFoundation/AVAsset.h>
#import <AVFoundation/AVAudioSettings.h>
#import <AVFoundation/AVError.h>
#import <AVFoundation/AVTime.h>
#import <CoreVideo/CVPixelBuffer.h>
#import "AVAssetInternal.h"
#import "MediaDecoderTypes.h"

@class AVAssetReader;

@interface AVAssetReaderOutput ()
@property (weak) AVAssetReader *_reader;
- (BOOL)_startWithAsset:(AVAsset *)asset timeRange:(CMTimeRange)range error:(NSError **)outError;
- (void)_stop;
- (BOOL)_ended;
@end

@interface AVAssetReader ()
- (void)_output:(AVAssetReaderOutput *)output failedWithError:(NSError *)error;
- (void)_outputEnded:(AVAssetReaderOutput *)output;
@end

@implementation AVAssetReaderOutput {
@protected
    BOOL _final;
}

- (AVMediaType)mediaType {
    return nil;
}

- (CMSampleBufferRef)copyNextSampleBuffer {
    return NULL;
}

- (void)resetForReadingTimeRanges:(NSArray<NSValue *> *)timeRanges {
}

- (void)markConfigurationAsFinal {
    _final = YES;
}

- (BOOL)_startWithAsset:(AVAsset *)asset timeRange:(CMTimeRange)range error:(NSError **)outError {
    return YES;
}

- (void)_stop {
}

- (BOOL)_ended {
    return YES;
}

@end

/* The decoder's options for a track output's settings, or nil when they
   are not ones it can give. */
static NSDictionary *decoderOptions(AVAssetTrack *track, NSDictionary *settings) {
    NSMutableDictionary *o = [NSMutableDictionary dictionary];
    if ([track.mediaType isEqual:AVMediaTypeVideo]) {
        id pf = settings[(__bridge NSString *)kCVPixelBufferPixelFormatTypeKey];
        NSArray *formats = pf == nil ? @[ @(kCVPixelFormatType_32BGRA) ] :
                           [pf isKindOfClass:[NSArray class]] ? pf : @[ pf ];
        for (NSNumber *f in formats) {
            OSType t = f.unsignedIntValue;
            if (t == kCVPixelFormatType_32BGRA || t == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ||
                t == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
                o[@kMDKeyOutputPixelFormat] = @(t);
                break;
            }
        }
        if (o[@kMDKeyOutputPixelFormat] == nil)
            return nil;
        NSNumber *w = settings[(__bridge NSString *)kCVPixelBufferWidthKey];
        NSNumber *h = settings[(__bridge NSString *)kCVPixelBufferHeightKey];
        if (w != nil && h != nil) {
            o[@kMDKeyOutputWidth] = w;
            o[@kMDKeyOutputHeight] = h;
        }
        return o;
    }
    if (settings == nil)
        return o;
    NSNumber *format = settings[AVFormatIDKey];
    if (format != nil && format.unsignedIntValue != kAudioFormatLinearPCM)
        return nil;
    if ([settings[AVLinearPCMIsNonInterleaved] boolValue] || [settings[AVLinearPCMIsBigEndianKey] boolValue])
        return nil;
    BOOL isFloat = [settings[AVLinearPCMIsFloatKey] boolValue];
    int bits = settings[AVLinearPCMBitDepthKey] != nil ? [settings[AVLinearPCMBitDepthKey] intValue] : (isFloat ? 32 : 16);
    if (settings[AVLinearPCMIsFloatKey] == nil && settings[AVLinearPCMBitDepthKey] == nil)
        isFloat = YES, bits = 32;
    if ((isFloat && bits != 32) || (!isFloat && bits != 16))
        return nil;
    o[@kMDKeyOutputFloat] = @(isFloat);
    if (settings[AVSampleRateKey] != nil)
        o[@kMDKeyOutputSampleRate] = @([settings[AVSampleRateKey] doubleValue]);
    if (settings[AVNumberOfChannelsKey] != nil)
        o[@kMDKeyOutputChannels] = settings[AVNumberOfChannelsKey];
    return o;
}

@implementation AVAssetReaderTrackOutput {
    MDSessionRef _session;
    CMTimeRange _range;
    NSMutableArray<NSValue *> *_pendingRanges;
    BOOL _ended, _started;
}

+ (instancetype)assetReaderTrackOutputWithTrack:(AVAssetTrack *)track outputSettings:(NSDictionary *)outputSettings {
    return [[self alloc] initWithTrack:track outputSettings:outputSettings];
}

- (instancetype)initWithTrack:(AVAssetTrack *)track outputSettings:(NSDictionary *)outputSettings {
    if ((self = [super init]) != nil) {
        _track = track;
        _outputSettings = [outputSettings copy];
        self.alwaysCopiesSampleData = YES;
    }
    return self;
}

- (void)dealloc {
    [self _stop];
}

- (AVMediaType)mediaType {
    return _track.mediaType;
}

- (BOOL)_startWithAsset:(AVAsset *)asset timeRange:(CMTimeRange)range error:(NSError **)outError {
    NSDictionary *options = decoderOptions(_track, _outputSettings);
    if (options == nil) {
        if (outError != NULL)
            *outError = _AVErrorForDecoderResult(kMDErrorBadOptions, nil);
        return NO;
    }
    _session = [asset _createSessionReturningError:outError];
    if (_session == NULL)
        return NO;
    int32_t r = _MDSessionConfigure(_session, _track.trackID, (__bridge CFDictionaryRef)options);
    if (r == kMDSuccess && CMTIME_IS_NUMERIC(range.start) && CMTimeCompare(range.start, kCMTimeZero) > 0)
        r = _MDSessionSeek(_session, range.start);
    if (r != kMDSuccess) {
        if (outError != NULL)
            *outError = _AVErrorForDecoderResult(r, nil);
        [self _stop];
        return NO;
    }
    _range = range;
    _started = YES;
    return YES;
}

- (void)_stop {
    if (_session != NULL)
        _MDSessionRelease(_session);
    _session = NULL;
}

- (BOOL)_ended {
    return _ended;
}

/* Past the range's end? */
- (BOOL)_beyondRange:(CMSampleBufferRef)sb {
    if (!CMTIME_IS_NUMERIC(_range.duration))
        return NO;
    CMTime end = CMTimeRangeGetEnd(_range);
    return CMTimeCompare(CMSampleBufferGetPresentationTimeStamp(sb), end) >= 0;
}

- (CMSampleBufferRef)copyNextSampleBuffer {
    AVAssetReader *reader = self._reader;
    if (_session == NULL || reader == nil || reader.status != AVAssetReaderStatusReading || _ended)
        return NULL;
    for (;;) {
        CMSampleBufferRef sb = NULL;
        Boolean ended = false;
        int32_t r = _MDSessionCopyNextSample(_session, _track.trackID, &sb, &ended);
        if (r != kMDSuccess) {
            [reader _output:self failedWithError:_AVErrorForDecoderResult(r, nil)];
            return NULL;
        }
        if (sb != NULL && [self _beyondRange:sb]) {
            CFRelease(sb);
            sb = NULL;
            ended = true;
        }
        if (sb != NULL)
            return sb;
        if (ended) {
            if (self.supportsRandomAccess)
                return NULL;	/* until the next ranges */
            _ended = YES;
            [reader _outputEnded:self];
            return NULL;
        }
    }
}

/* Random access: the next ranges, read in order, once the app has read
   the last ones to their end. */
- (void)resetForReadingTimeRanges:(NSArray<NSValue *> *)timeRanges {
    if (!self.supportsRandomAccess || _final || _session == NULL || timeRanges.count == 0)
        return;
    CMTimeRange first = [timeRanges.firstObject CMTimeRangeValue];
    CMTimeRange last = [timeRanges.lastObject CMTimeRangeValue];
    _range = CMTimeRangeFromTimeToTime(first.start, CMTimeRangeGetEnd(last));
    _MDSessionSeek(_session, first.start);
}

- (void)markConfigurationAsFinal {
    [super markConfigurationAsFinal];
    if (self.supportsRandomAccess && !_ended) {
        _ended = YES;
        [self._reader _outputEnded:self];
    }
}

@end

@implementation AVAssetReader {
    NSMutableArray<AVAssetReaderOutput *> *_outputs;
    NSMutableSet *_ended;
}

+ (instancetype)assetReaderWithAsset:(AVAsset *)asset error:(NSError **)outError {
    return [[self alloc] initWithAsset:asset error:outError];
}

- (instancetype)initWithAsset:(AVAsset *)asset error:(NSError **)outError {
    if ((self = [super init]) != nil) {
        _asset = asset;
        _outputs = [NSMutableArray array];
        _ended = [NSMutableSet set];
        _timeRange = CMTimeRangeMake(kCMTimeZero, kCMTimePositiveInfinity);
    }
    return self;
}

- (void)dealloc {
    for (AVAssetReaderOutput *o in _outputs)
        [o _stop];
}

- (NSArray<AVAssetReaderOutput *> *)outputs {
    @synchronized(self) {
        return [_outputs copy];
    }
}

- (BOOL)canAddOutput:(AVAssetReaderOutput *)output {
    if (_status != AVAssetReaderStatusUnknown || output == nil || [_outputs containsObject:output])
        return NO;
    if ([output isKindOfClass:[AVAssetReaderTrackOutput class]]) {
        AVAssetReaderTrackOutput *t = (AVAssetReaderTrackOutput *)output;
        return t.track.asset == _asset && decoderOptions(t.track, t.outputSettings) != nil;
    }
    return NO;
}

- (void)addOutput:(AVAssetReaderOutput *)output {
    if ([self canAddOutput:output]) {
        output._reader = self;
        [_outputs addObject:output];
    }
}

- (BOOL)startReading {
    if (_status != AVAssetReaderStatusUnknown)
        return NO;
    NSError *error = nil;
    if (![_asset _loadReturningError:&error]) {
        _error = error;
        _status = AVAssetReaderStatusFailed;
        return NO;
    }
    _status = AVAssetReaderStatusReading;
    for (AVAssetReaderOutput *o in _outputs)
        if (![o _startWithAsset:_asset timeRange:_timeRange error:&error]) {
            _error = error;
            _status = AVAssetReaderStatusFailed;
            for (AVAssetReaderOutput *s in _outputs)
                [s _stop];
            return NO;
        }
    if (_outputs.count == 0)
        _status = AVAssetReaderStatusCompleted;
    return YES;
}

- (void)cancelReading {
    @synchronized(self) {
        if (_status == AVAssetReaderStatusReading || _status == AVAssetReaderStatusUnknown)
            _status = AVAssetReaderStatusCancelled;
    }
    for (AVAssetReaderOutput *o in _outputs)
        [o _stop];
}

- (void)_output:(AVAssetReaderOutput *)output failedWithError:(NSError *)error {
    @synchronized(self) {
        if (_status == AVAssetReaderStatusReading) {
            _error = error;
            _status = AVAssetReaderStatusFailed;
        }
    }
}

/* Completed once every output has given its last sample. */
- (void)_outputEnded:(AVAssetReaderOutput *)output {
    @synchronized(self) {
        [_ended addObject:[NSValue valueWithNonretainedObject:output]];
        if (_status == AVAssetReaderStatusReading && _ended.count == _outputs.count)
            _status = AVAssetReaderStatusCompleted;
    }
}

@end
