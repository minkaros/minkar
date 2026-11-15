/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Assets read through the media decoder: the file opened once in a
 * session to learn its tracks, formats and tags, which are then kept.
 * Readers and players open sessions of their own.
 */

#import <AVFoundation/AVAsset.h>
#import <AVFoundation/AVError.h>
#import "AVAssetInternal.h"
#import "MediaDecoderTypes.h"
#import <CoreServices/CoreServices.h>
#include <fcntl.h>
#include <unistd.h>

NSString *const AVURLAssetPreferPreciseDurationAndTimingKey = @"AVURLAssetPreferPreciseDurationAndTimingKey";
NSString *const AVURLAssetReferenceRestrictionsKey = @"AVURLAssetReferenceRestrictionsKey";
NSString *const AVURLAssetHTTPCookiesKey = @"AVURLAssetHTTPCookiesKey";
NSString *const AVURLAssetAllowsCellularAccessKey = @"AVURLAssetAllowsCellularAccessKey";

NSError *_AVErrorForDecoderResult(int32_t result, NSURL *URL) {
    NSInteger code;
    NSString *what;
    switch (result) {
    case kMDErrorCannotOpen:
        code = AVErrorFileFormatNotRecognized;
        what = @"The file's format is not one that can be read.";
        break;
    case kMDErrorDecode:
        code = AVErrorDecodeFailed;
        what = @"The media could not be decoded.";
        break;
    case kMDErrorBadOptions:
        code = AVErrorUnsupportedOutputSettings;
        what = @"The output settings are not supported.";
        break;
    case kMDErrorStalled:
        code = AVErrorFailedToLoadMediaData;
        what = @"The media's data stopped coming.";
        break;
    case kMDErrorNoService:
        code = AVErrorUnknown;
        what = @"The media decoder stopped.";
        break;
    default:
        code = AVErrorUnknown;
        what = @"The operation could not be completed.";
        break;
    }
    NSMutableDictionary *info = [NSMutableDictionary dictionaryWithObject:what forKey:NSLocalizedDescriptionKey];
    if (URL != nil)
        info[NSURLErrorKey] = URL;
    return [NSError errorWithDomain:AVFoundationErrorDomain code:code userInfo:info];
}

/* ---- Formats ------------------------------------------------------------ */

static FourCharCode videoCodecType(NSString *codec) {
    static NSDictionary *map;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        map = @{
            @"h264": @((FourCharCode)'avc1'), @"hevc": @((FourCharCode)'hvc1'), @"vp8": @((FourCharCode)'vp08'), @"vp9": @((FourCharCode)'vp09'), @"av1": @((FourCharCode)'av01'),
            @"mpeg4": @((FourCharCode)'mp4v'), @"mjpeg": @((FourCharCode)'jpeg'), @"prores": @((FourCharCode)'apcn'), @"mpeg2video": @((FourCharCode)'mp2v'),
            @"mpeg1video": @((FourCharCode)'mp1v'), @"h263": @((FourCharCode)'h263'), @"dvvideo": @((FourCharCode)'dvc '), @"png": @((FourCharCode)'png '),
            @"gif": @((FourCharCode)'gif '), @"rawvideo": @((FourCharCode)'raw '),
        };
    });
    NSNumber *n = map[codec];
    return n != nil ? (FourCharCode)n.unsignedIntValue : 0;
}

static void audioFormat(NSString *codec, AudioStreamBasicDescription *asbd) {
    static NSDictionary *map;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        map = @{
            @"aac": @(kAudioFormatMPEG4AAC), @"mp3": @(kAudioFormatMPEGLayer3), @"mp2": @(kAudioFormatMPEGLayer2),
            @"alac": @(kAudioFormatAppleLossless), @"flac": @(kAudioFormatFLAC), @"opus": @(kAudioFormatOpus),
            @"ac3": @(kAudioFormatAC3), @"eac3": @(kAudioFormatEnhancedAC3), @"amr_nb": @(kAudioFormatAMR),
            @"pcm_mulaw": @(kAudioFormatULaw), @"pcm_alaw": @(kAudioFormatALaw), @"vorbis": @((FourCharCode)'vorb'),
        };
    });
    NSNumber *n = map[codec];
    if (n != nil) {
        asbd->mFormatID = n.unsignedIntValue;
        return;
    }
    /* Linear PCM: pcm_[su]BITS[lb]e, pcm_fBITS[lb]e. */
    if ([codec hasPrefix:@"pcm_"] && codec.length >= 8) {
        unichar kind = [codec characterAtIndex:4];
        int bits = [[codec substringFromIndex:5] intValue];
        asbd->mFormatID = kAudioFormatLinearPCM;
        asbd->mBitsPerChannel = bits;
        asbd->mFormatFlags = kAudioFormatFlagIsPacked | (kind == 'f' ? kAudioFormatFlagIsFloat :
                                                         kind == 's' ? kAudioFormatFlagIsSignedInteger : 0);
        if ([codec hasSuffix:@"be"])
            asbd->mFormatFlags |= kAudioFormatFlagIsBigEndian;
        asbd->mFramesPerPacket = 1;
        asbd->mBytesPerFrame = asbd->mBytesPerPacket = asbd->mChannelsPerFrame * bits / 8;
    }
}

/* ---- Metadata ------------------------------------------------------------ */

static AVMetadataKeySpace keySpaceForFormat(NSString *format) {
    if ([format hasPrefix:@"mov,mp4"])
        return AVMetadataKeySpaceQuickTimeMetadata;
    if ([format isEqual:@"mp3"])
        return AVMetadataKeySpaceID3;
    return AVMetadataKeySpaceQuickTimeMetadata;
}

static AVMetadataFormat metadataFormatForKeySpace(AVMetadataKeySpace space) {
    return [space isEqual:AVMetadataKeySpaceID3] ? AVMetadataFormatID3Metadata : AVMetadataFormatQuickTimeMetadata;
}

static NSArray<AVMetadataItem *> *itemsFromTags(NSDictionary *tags, AVMetadataKeySpace space) {
    NSMutableArray *items = [NSMutableArray array];
    for (NSString *key in [tags.allKeys sortedArrayUsingSelector:@selector(compare:)]) {
        id value = tags[key];
        if (![value isKindOfClass:[NSString class]] || [key hasPrefix:@"handler_name"] || [key isEqual:@"vendor_id"] ||
            [key isEqual:@"major_brand"] || [key isEqual:@"minor_version"] || [key isEqual:@"compatible_brands"])
            continue;
        AVMutableMetadataItem *i = [AVMutableMetadataItem metadataItem];
        i.keySpace = space;
        i.key = key;
        i.value = value;
        [items addObject:[i copy]];
    }
    return items;
}

static NSArray<AVMetadataItem *> *commonItems(NSArray<AVMetadataItem *> *items) {
    NSMutableArray *common = [NSMutableArray array];
    NSMutableSet *seen = [NSMutableSet set];
    for (AVMetadataItem *i in items) {
        AVMetadataKey k = i.commonKey;
        if (k == nil || [seen containsObject:k])
            continue;
        [seen addObject:k];
        AVMutableMetadataItem *c = [i mutableCopy];
        c.keySpace = AVMetadataKeySpaceCommon;
        c.key = k;
        if ([k isEqual:AVMetadataCommonKeyCreationDate] && i.dateValue != nil)
            c.value = i.dateValue;
        [common addObject:[c copy]];
    }
    return common;
}

/* ---- Tracks -------------------------------------------------------------- */

@interface AVAssetTrack ()
- (instancetype)_initWithAsset:(AVAsset *)asset description:(NSDictionary *)d keySpace:(AVMetadataKeySpace)space;
@end

@implementation AVAssetTrack {
    NSDictionary *_d;
    NSArray *_formatDescriptions;
    NSArray<AVMetadataItem *> *_metadata;
    AVMetadataKeySpace _keySpace;
}

- (instancetype)_initWithAsset:(AVAsset *)asset description:(NSDictionary *)d keySpace:(AVMetadataKeySpace)space {
    if ((self = [super init]) != nil) {
        _asset = asset;
        _d = [d copy];
        _keySpace = space;
        _metadata = itemsFromTags(d[@kMDKeyMetadata], space);
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

- (CMPersistentTrackID)trackID {
    return [_d[@kMDKeyTrackID] intValue];
}

- (AVMediaType)mediaType {
    return [_d[@kMDKeyMediaType] isEqual:@"vide"] ? AVMediaTypeVideo : AVMediaTypeAudio;
}

- (BOOL)_isVideo {
    return [_d[@kMDKeyMediaType] isEqual:@"vide"];
}

- (NSString *)_codec {
    return _d[@kMDKeyCodec];
}

- (NSArray *)formatDescriptions {
    @synchronized(self) {
        if (_formatDescriptions != nil)
            return _formatDescriptions;
        CMFormatDescriptionRef f = NULL;
        if (self._isVideo) {
            NSDictionary *ext = nil;
            if ([_d[@kMDKeyFullRange] boolValue])
                ext = @{ (__bridge NSString *)kCMFormatDescriptionExtension_FullRangeVideo: @YES };
            CMVideoFormatDescriptionCreate(NULL, videoCodecType(self._codec), [_d[@kMDKeyWidth] intValue],
                                           [_d[@kMDKeyHeight] intValue], (__bridge CFDictionaryRef)ext, &f);
        } else {
            AudioStreamBasicDescription asbd = { 0 };
            asbd.mSampleRate = [_d[@kMDKeySampleRate] doubleValue];
            asbd.mChannelsPerFrame = [_d[@kMDKeyChannels] unsignedIntValue];
            audioFormat(self._codec, &asbd);
            CMAudioFormatDescriptionCreate(NULL, &asbd, 0, NULL, 0, NULL, NULL, &f);
        }
        _formatDescriptions = f != NULL ? @[ (__bridge_transfer id)f ] : @[];
        return _formatDescriptions;
    }
}

- (BOOL)isPlayable {
    return YES;
}

- (BOOL)isDecodable {
    return YES;
}

- (BOOL)isEnabled {
    return YES;
}

- (BOOL)isSelfContained {
    return YES;
}

- (long long)totalSampleDataLength {
    return (long long)([_d[@kMDKeyBitRate] doubleValue] / 8 * CMTimeGetSeconds(self.timeRange.duration));
}

- (BOOL)hasMediaCharacteristic:(AVMediaCharacteristic)c {
    if (self._isVideo)
        return [c isEqual:AVMediaCharacteristicVisual] || [c isEqual:AVMediaCharacteristicFrameBased];
    return [c isEqual:AVMediaCharacteristicAudible];
}

- (CMTimeScale)naturalTimeScale {
    int scale = [_d[@kMDKeyTimescale] intValue];
    if (!self._isVideo && [_d[@kMDKeySampleRate] intValue] > 0)
        scale = [_d[@kMDKeySampleRate] intValue];
    return scale > 0 ? scale : 600;
}

- (CMTimeRange)timeRange {
    NSNumber *d = _d[@kMDKeyTrackDuration];
    if (d == nil)
        return CMTimeRangeMake(kCMTimeZero, kCMTimeIndefinite);
    return CMTimeRangeMake(kCMTimeZero, CMTimeMakeWithSeconds(d.doubleValue, self.naturalTimeScale));
}

- (float)estimatedDataRate {
    return [_d[@kMDKeyBitRate] floatValue];
}

- (NSString *)languageCode {
    return _d[@kMDKeyLanguage];
}

- (NSString *)extendedLanguageTag {
    NSString *code = self.languageCode;
    if (code == nil)
        return nil;
    NSString *tag = [NSLocale canonicalLanguageIdentifierFromString:code];
    return tag.length > 0 ? tag : code;
}

- (CGSize)naturalSize {
    if (!self._isVideo)
        return CGSizeZero;
    double w = [_d[@kMDKeyWidth] doubleValue], h = [_d[@kMDKeyHeight] doubleValue];
    NSArray *sar = _d[@kMDKeySampleAspect];
    if (sar.count == 2 && [sar[1] doubleValue] > 0)
        w = w * [sar[0] doubleValue] / [sar[1] doubleValue];
    return CGSizeMake(w, h);
}

/* The rotation the file asks for, moved back into the first quadrant. */
- (CGAffineTransform)preferredTransform {
    CGSize s = self.naturalSize;
    int r = ((int)lround([_d[@kMDKeyRotation] doubleValue]) % 360 + 360) % 360;
    switch (r) {
    case 90:
        return CGAffineTransformMake(0, 1, -1, 0, s.height, 0);
    case 180:
        return CGAffineTransformMake(-1, 0, 0, -1, s.width, s.height);
    case 270:
        return CGAffineTransformMake(0, -1, 1, 0, 0, s.width);
    default:
        return CGAffineTransformIdentity;
    }
}

- (float)preferredVolume {
    return 1;
}

- (BOOL)hasAudioSampleDependencies {
    return NO;
}

- (float)nominalFrameRate {
    return self._isVideo ? [_d[@kMDKeyFrameRate] floatValue] : 0;
}

- (CMTime)minFrameDuration {
    float rate = self.nominalFrameRate;
    return rate > 0 ? CMTimeMakeWithSeconds(1.0 / rate, self.naturalTimeScale) : kCMTimeInvalid;
}

- (BOOL)requiresFrameReordering {
    NSString *c = self._codec;
    return [c isEqual:@"h264"] || [c isEqual:@"hevc"] || [c isEqual:@"mpeg4"] || [c isEqual:@"mpeg2video"];
}

- (NSArray<AVMetadataItem *> *)metadata {
    return _metadata;
}

- (NSArray<AVMetadataItem *> *)commonMetadata {
    return commonItems(_metadata);
}

- (NSArray<AVMetadataFormat> *)availableMetadataFormats {
    return _metadata.count > 0 ? @[ metadataFormatForKeySpace(_keySpace) ] : @[];
}

- (NSArray<AVMetadataItem *> *)metadataForFormat:(AVMetadataFormat)format {
    return [format isEqual:metadataFormatForKeySpace(_keySpace)] ? _metadata : @[];
}

- (CMTime)samplePresentationTimeForTrackTime:(CMTime)trackTime {
    return trackTime;
}

- (AVKeyValueStatus)statusOfValueForKey:(NSString *)key error:(NSError **)outError {
    return AVKeyValueStatusLoaded;
}

- (void)loadValuesAsynchronouslyForKeys:(NSArray<NSString *> *)keys completionHandler:(void (^)(void))handler {
    if (handler != nil)
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), handler);
}

@end

/* ---- Assets -------------------------------------------------------------- */

@implementation AVAsset {
@protected
    NSDictionary *_info;
    NSError *_loadError;
    BOOL _loaded;
    NSArray<AVAssetTrack *> *_tracks;
    NSArray<AVMetadataItem *> *_metadata;
    AVMetadataKeySpace _keySpace;
}

+ (instancetype)assetWithURL:(NSURL *)URL {
    return [AVURLAsset URLAssetWithURL:URL options:nil];
}

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

/* The description: an AVAsset of its own has none. */
- (NSDictionary *)_copyInfoReturningError:(NSError **)outError {
    if (outError != NULL)
        *outError = _AVErrorForDecoderResult(kMDErrorCannotOpen, nil);
    return nil;
}

- (BOOL)_loadReturningError:(NSError **)outError {
    @synchronized(self) {
        if (!_loaded) {
            NSError *error = nil;
            _info = [self _copyInfoReturningError:&error];
            _loadError = error;
            _keySpace = keySpaceForFormat(_info[@kMDKeyFormat]);
            NSMutableArray *tracks = [NSMutableArray array];
            for (NSDictionary *t in _info[@kMDKeyTracks])
                [tracks addObject:[[AVAssetTrack alloc] _initWithAsset:self description:t keySpace:_keySpace]];
            _tracks = tracks;
            _metadata = itemsFromTags(_info[@kMDKeyMetadata], _keySpace);
            _loaded = YES;
        }
        if (outError != NULL)
            *outError = _loadError;
        return _info != nil;
    }
}

- (MDSessionRef)_createSessionReturningError:(NSError **)outError {
    if (outError != NULL)
        *outError = _AVErrorForDecoderResult(kMDErrorCannotOpen, nil);
    return NULL;
}

- (CMTime)duration {
    if (![self _loadReturningError:NULL])
        return kCMTimeInvalid;
    NSNumber *d = _info[@kMDKeyDuration];
    if (d == nil)
        return kCMTimeIndefinite;
    CMTimeScale scale = _tracks.count > 0 ? _tracks[0].naturalTimeScale : 600;
    return CMTimeMakeWithSeconds(d.doubleValue, scale);
}

- (CMTime)overallDurationHint {
    return self.duration;
}

- (float)preferredRate {
    return 1;
}

- (float)preferredVolume {
    return 1;
}

- (CGAffineTransform)preferredTransform {
    return CGAffineTransformIdentity;
}

- (CGSize)naturalSize {
    return [self tracksWithMediaType:AVMediaTypeVideo].firstObject.naturalSize;
}

/* The containers whose index gives exact times; not raw MPEG audio,
   ADTS or Ogg, whose durations are estimated. */
- (BOOL)providesPreciseDurationAndTiming {
    if (![self _loadReturningError:NULL])
        return NO;
    NSString *f = _info[@kMDKeyFormat];
    return !([f isEqual:@"mp3"] || [f isEqual:@"aac"] || [f isEqual:@"ogg"] || [f isEqual:@"mpeg"] ||
             [f isEqual:@"mpegts"]);
}

- (void)cancelLoading {
}

- (NSArray<AVAssetTrack *> *)tracks {
    [self _loadReturningError:NULL];
    return _tracks != nil ? _tracks : @[];
}

- (AVAssetTrack *)trackWithTrackID:(CMPersistentTrackID)trackID {
    for (AVAssetTrack *t in self.tracks)
        if (t.trackID == trackID)
            return t;
    return nil;
}

- (NSArray<AVAssetTrack *> *)tracksWithMediaType:(AVMediaType)mediaType {
    NSMutableArray *r = [NSMutableArray array];
    for (AVAssetTrack *t in self.tracks)
        if ([t.mediaType isEqual:mediaType])
            [r addObject:t];
    return r;
}

- (NSArray<AVAssetTrack *> *)tracksWithMediaCharacteristic:(AVMediaCharacteristic)c {
    NSMutableArray *r = [NSMutableArray array];
    for (AVAssetTrack *t in self.tracks)
        if ([t hasMediaCharacteristic:c])
            [r addObject:t];
    return r;
}

- (NSArray<AVMetadataItem *> *)metadata {
    [self _loadReturningError:NULL];
    return _metadata != nil ? _metadata : @[];
}

- (NSArray<AVMetadataItem *> *)commonMetadata {
    return commonItems(self.metadata);
}

- (NSArray<AVMetadataFormat> *)availableMetadataFormats {
    return self.metadata.count > 0 ? @[ metadataFormatForKeySpace(_keySpace) ] : @[];
}

- (NSArray<AVMetadataItem *> *)metadataForFormat:(AVMetadataFormat)format {
    return [self.availableMetadataFormats containsObject:format] ? self.metadata : @[];
}

- (AVMetadataItem *)creationDate {
    return [AVMetadataItem metadataItemsFromArray:self.commonMetadata
                                          withKey:AVMetadataCommonKeyCreationDate
                                         keySpace:AVMetadataKeySpaceCommon].firstObject;
}

- (NSString *)lyrics {
    for (AVMetadataItem *i in self.metadata)
        if ([[(id)i.key lowercaseString] isEqual:@"lyrics"])
            return i.stringValue;
    return nil;
}

- (NSArray<NSLocale *> *)availableChapterLocales {
    return @[];
}

- (NSArray<AVMediaCharacteristic> *)availableMediaCharacteristicsWithMediaSelectionOptions {
    return @[];
}

- (BOOL)isPlayable {
    return self.tracks.count > 0;
}

- (BOOL)isReadable {
    return self.tracks.count > 0;
}

- (BOOL)isExportable {
    return NO;
}

- (BOOL)isComposable {
    return NO;
}

- (BOOL)hasProtectedContent {
    return NO;
}

- (BOOL)isCompatibleWithAirPlayVideo {
    return NO;
}

- (BOOL)canContainFragments {
    return NO;
}

- (BOOL)containsFragments {
    return NO;
}

- (AVKeyValueStatus)statusOfValueForKey:(NSString *)key error:(NSError **)outError {
    @synchronized(self) {
        if (!_loaded)
            return AVKeyValueStatusUnknown;
    }
    NSError *error = nil;
    if ([self _loadReturningError:&error])
        return AVKeyValueStatusLoaded;
    if (outError != NULL)
        *outError = error;
    return AVKeyValueStatusFailed;
}

- (void)loadValuesAsynchronouslyForKeys:(NSArray<NSString *> *)keys completionHandler:(void (^)(void))handler {
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        [self _loadReturningError:NULL];
        if (handler != nil)
            handler();
    });
}

- (void)_load:(void (^)(NSError *error))then {
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSError *error = nil;
        [self _loadReturningError:&error];
        then(error);
    });
}

- (void)loadTrackWithTrackID:(CMPersistentTrackID)trackID
           completionHandler:(void (^)(AVAssetTrack *, NSError *))completionHandler {
    [self _load:^(NSError *error) {
        completionHandler(error != nil ? nil : [self trackWithTrackID:trackID], error);
    }];
}

- (void)loadTracksWithMediaType:(AVMediaType)mediaType
              completionHandler:(void (^)(NSArray<AVAssetTrack *> *, NSError *))completionHandler {
    [self _load:^(NSError *error) {
        completionHandler(error != nil ? nil : [self tracksWithMediaType:mediaType], error);
    }];
}

- (void)loadTracksWithMediaCharacteristic:(AVMediaCharacteristic)c
                        completionHandler:(void (^)(NSArray<AVAssetTrack *> *, NSError *))completionHandler {
    [self _load:^(NSError *error) {
        completionHandler(error != nil ? nil : [self tracksWithMediaCharacteristic:c], error);
    }];
}

- (void)loadMetadataForFormat:(AVMetadataFormat)format
            completionHandler:(void (^)(NSArray<AVMetadataItem *> *, NSError *))completionHandler {
    [self _load:^(NSError *error) {
        completionHandler(error != nil ? nil : [self metadataForFormat:format], error);
    }];
}

@end

/* ---- URL assets ---------------------------------------------------------- */

/* The types FFmpeg's demuxers here read (as LaunchServices declares
   them), without protected or playlist files. */
static NSArray<AVFileType> *playableTypes(void) {
    return @[
        AVFileTypeQuickTimeMovie, AVFileTypeMPEG4, AVFileTypeAppleM4V, AVFileTypeAppleM4A, AVFileType3GPP,
        AVFileType3GPP2, AVFileTypeCoreAudioFormat, AVFileTypeWAVE, AVFileTypeAIFF, AVFileTypeAIFC,
        AVFileTypeMPEGLayer3, AVFileTypeAC3, AVFileTypeEnhancedAC3, AVFileTypeSunAU, @"public.mpeg-4-audio",
        @"public.aac-audio", @"org.xiph.ogg-audio", @"org.xiph.flac", @"org.xiph.opus", @"org.matroska.mka",
        @"public.mpeg", @"public.mpeg-2-video", @"public.mpeg-2-transport-stream", @"public.avi",
        @"org.matroska.mkv", @"org.webmproject.webm", @"org.xiph.ogv", @"public.dv-movie",
        @"com.adobe.flash.video", @"com.microsoft.windows-media-wmv", @"com.microsoft.windows-media-wma",
        @"com.microsoft.advanced-systems-format",
    ];
}

@implementation AVURLAsset {
    NSDictionary *_options;
}

+ (NSArray<AVFileType> *)audiovisualTypes {
    return playableTypes();
}

+ (NSArray<NSString *> *)audiovisualMIMETypes {
    static NSArray *types;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSMutableOrderedSet *set = [NSMutableOrderedSet orderedSet];
        for (AVFileType t in playableTypes()) {
            CFArrayRef tags = UTTypeCopyAllTagsWithClass((__bridge CFStringRef)t, kUTTagClassMIMEType);
            if (tags != NULL) {
                [set addObjectsFromArray:(__bridge NSArray *)tags];
                CFRelease(tags);
            }
        }
        types = set.array;
    });
    return types;
}

/* A MIME type with its codecs parameter (RFC 6381): playable when the
   type is and every codec is one the decoder has. */
+ (BOOL)isPlayableExtendedMIMEType:(NSString *)extended {
    NSArray *parts = [extended componentsSeparatedByString:@";"];
    NSString *type = [[parts.firstObject stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]
        lowercaseString];
    if (![[self audiovisualMIMETypes] containsObject:type])
        return NO;
    static NSArray *known;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        known = @[ @"avc1", @"avc3", @"hvc1", @"hev1", @"vp08", @"vp8", @"vp09", @"vp9", @"av01", @"mp4v", @"mp4a",
                   @"opus", @"vorbis", @"flac", @"alac", @"mp3", @".mp3", @"ac-3", @"ec-3", @"ulaw", @"alaw",
                   @"lpcm", @"theora", @"mjpg", @"jpeg", @"apcn", @"apch", @"apcs", @"apco", @"ap4h" ];
    });
    for (NSUInteger i = 1; i < parts.count; i++) {
        NSString *p = [parts[i] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        if (![p.lowercaseString hasPrefix:@"codecs="])
            continue;
        NSString *list = [[p substringFromIndex:7] stringByTrimmingCharactersInSet:
            [NSCharacterSet characterSetWithCharactersInString:@"\"' "]];
        for (NSString *c in [list componentsSeparatedByString:@","]) {
            NSString *codec = [[[c stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]
                componentsSeparatedByString:@"."].firstObject lowercaseString];
            if (codec.length > 0 && ![known containsObject:codec])
                return NO;
        }
    }
    return YES;
}

+ (instancetype)URLAssetWithURL:(NSURL *)URL options:(NSDictionary *)options {
    return [[self alloc] initWithURL:URL options:options];
}

- (instancetype)initWithURL:(NSURL *)URL options:(NSDictionary *)options {
    if ((self = [super init]) != nil) {
        _URL = [URL copy];
        _options = [options copy];
    }
    return self;
}

- (int)_openFileReturningError:(NSError **)outError {
    if (!_URL.isFileURL) {
        if (outError != NULL)
            *outError = [NSError errorWithDomain:AVFoundationErrorDomain code:AVErrorContentIsUnavailable
                userInfo:@{ NSLocalizedDescriptionKey: @"Only files can be read.", NSURLErrorKey: _URL }];
        return -1;
    }
    int fd = open(_URL.fileSystemRepresentation, O_RDONLY | O_CLOEXEC);
    if (fd < 0 && outError != NULL) {
        NSError *posix = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil];
        *outError = [NSError errorWithDomain:AVFoundationErrorDomain code:AVErrorUnknown
            userInfo:@{ NSLocalizedDescriptionKey: @"The file could not be opened.", NSURLErrorKey: _URL,
                        NSUnderlyingErrorKey: posix }];
    }
    return fd;
}

- (MDSessionRef)_createSessionReturningError:(NSError **)outError {
    int fd = [self _openFileReturningError:outError];
    if (fd < 0)
        return NULL;
    int32_t result = 0;
    MDSessionRef s = _MDSessionCreate(fd, _URL.lastPathComponent.fileSystemRepresentation, &result);
    close(fd);
    if (s == NULL && outError != NULL)
        *outError = _AVErrorForDecoderResult(result, _URL);
    return s;
}

- (NSDictionary *)_copyInfoReturningError:(NSError **)outError {
    MDSessionRef s = [self _createSessionReturningError:outError];
    if (s == NULL)
        return nil;
    NSDictionary *info = [(__bridge NSDictionary *)_MDSessionGetInfo(s) copy];
    _MDSessionRelease(s);
    return info;
}

@end
