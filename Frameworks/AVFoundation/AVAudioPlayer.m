/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The audio player, on a playback engine (AVPlaybackEngine) of the file's
 * audio. Bytes are played from a file of their own, removed once the
 * decoder has it open. Levels are those of the samples queued to play.
 * SHORTCUT(AU5): rates other than 1 keep time but play no sound; the pan
 * is kept but not applied.
 */

#import <AVFoundation/AVAudioPlayer.h>
#import <AVFoundation/AVAsset.h>
#import <AVFoundation/AVAudioSettings.h>
#import "AVPlaybackEngine.h"
#include <mach/mach_time.h>
#include <math.h>

static NSError *unsupported(void) {
    /* kAudioFileUnsupportedFileTypeError ('typ?'), as audio files report. */
    return [NSError errorWithDomain:NSOSStatusErrorDomain code:0x7479703F userInfo:nil];
}

static double hostSeconds(void) {
    static mach_timebase_info_data_t tb;
    if (tb.denom == 0)
        mach_timebase_info(&tb);
    return (double)mach_absolute_time() * tb.numer / tb.denom / 1e9;
}

@implementation AVAudioPlayer {
    AVURLAsset *_asset;
    AVPlaybackEngine *_engine;
    dispatch_source_t _timer, _fade;
    NSInteger _loopsDone;
    float _levelsPeak[2], _levelsAverage[2];
}

- (instancetype)initWithContentsOfURL:(NSURL *)url error:(NSError **)outError {
    return [self initWithContentsOfURL:url fileTypeHint:nil error:outError];
}

- (instancetype)initWithContentsOfURL:(NSURL *)url fileTypeHint:(NSString *)hint error:(NSError **)outError {
    if ((self = [super init]) == nil)
        return nil;
    _url = [url copy];
    return [self _setUpWithURL:url error:outError] ? self : nil;
}

- (instancetype)initWithData:(NSData *)data error:(NSError **)outError {
    return [self initWithData:data fileTypeHint:nil error:outError];
}

- (instancetype)initWithData:(NSData *)data fileTypeHint:(NSString *)hint error:(NSError **)outError {
    if ((self = [super init]) == nil)
        return nil;
    _data = [data copy];
    NSString *path = [NSTemporaryDirectory() stringByAppendingPathComponent:
        [NSString stringWithFormat:@"AVAudioPlayer-%@", [NSUUID UUID].UUIDString]];
    if (![_data writeToFile:path options:0 error:outError])
        return nil;
    BOOL ok = [self _setUpWithURL:[NSURL fileURLWithPath:path] error:outError];
    /* The decoder holds the file open: its name is no longer needed. */
    [[NSFileManager defaultManager] removeItemAtPath:path error:NULL];
    return ok ? self : nil;
}

- (BOOL)_setUpWithURL:(NSURL *)url error:(NSError **)outError {
    _volume = 1;
    _rate = 1;
    _asset = [AVURLAsset URLAssetWithURL:url options:nil];
    NSError *error = nil;
    if ([_asset tracksWithMediaType:AVMediaTypeAudio].count == 0) {
        if (outError != NULL)
            *outError = unsupported();
        return NO;
    }
    _engine = [[AVPlaybackEngine alloc] initWithAsset:_asset error:&error];
    if (_engine == nil || !_engine.hasAudio) {
        if (outError != NULL)
            *outError = error ?: unsupported();
        return NO;
    }
    return YES;
}

- (void)dealloc {
    if (_timer != nil)
        dispatch_source_cancel(_timer);
    if (_fade != nil)
        dispatch_source_cancel(_fade);
    [_engine invalidate];
}

- (BOOL)prepareToPlay {
    return _engine != nil;
}

- (BOOL)play {
    if (_engine == nil)
        return NO;
    if (_engine.isAtEnd)
        [_engine seekToTime:kCMTimeZero completionHandler:nil];
    _engine.volume = _volume;
    _engine.rate = _enableRate ? _rate : 1;
    [self willChangeValueForKey:@"playing"];
    _playing = YES;
    [self didChangeValueForKey:@"playing"];
    [self _startTimer];
    return YES;
}

- (BOOL)playAtTime:(NSTimeInterval)time {
    NSTimeInterval delay = time - self.deviceCurrentTime;
    if (delay <= 0)
        return [self play];
    __weak AVAudioPlayer *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [weakSelf play];
    });
    return YES;
}

- (void)pause {
    _engine.rate = 0;
    [self _stopped];
}

/* Stopped, the position stays where it was (as play then goes on). */
- (void)stop {
    _engine.rate = 0;
    [self _stopped];
}

- (void)_stopped {
    if (_timer != nil) {
        dispatch_source_cancel(_timer);
        _timer = nil;
    }
    if (_playing) {
        [self willChangeValueForKey:@"playing"];
        _playing = NO;
        [self didChangeValueForKey:@"playing"];
    }
}

/* The end, a loop or an error, watched on the main thread. */
- (void)_startTimer {
    if (_timer != nil)
        return;
    _timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(_timer, DISPATCH_TIME_NOW, NSEC_PER_SEC / 50, NSEC_PER_SEC / 200);
    __weak AVAudioPlayer *weakSelf = self;
    dispatch_source_set_event_handler(_timer, ^{ [weakSelf _check]; });
    dispatch_resume(_timer);
}

- (void)_check {
    if (!_playing)
        return;
    NSError *error = _engine.error;
    if (error != nil) {
        [self stop];
        id<AVAudioPlayerDelegate> d = _delegate;
        if ([d respondsToSelector:@selector(audioPlayerDecodeErrorDidOccur:error:)])
            [d audioPlayerDecodeErrorDidOccur:self error:error];
        return;
    }
    if (!_engine.isAtEnd)
        return;
    if (_numberOfLoops < 0 || _loopsDone < _numberOfLoops) {
        _loopsDone++;
        [_engine seekToTime:kCMTimeZero completionHandler:nil];
        return;
    }
    _loopsDone = 0;
    _engine.rate = 0;
    [self _stopped];
    /* Finished: back at the start, as the next play begins there. */
    [_engine seekToTime:kCMTimeZero completionHandler:nil];
    id<AVAudioPlayerDelegate> d = _delegate;
    if ([d respondsToSelector:@selector(audioPlayerDidFinishPlaying:successfully:)])
        [d audioPlayerDidFinishPlaying:self successfully:YES];
}

- (NSUInteger)numberOfChannels {
    const AudioStreamBasicDescription *asbd = CMAudioFormatDescriptionGetStreamBasicDescription(
        (__bridge CMAudioFormatDescriptionRef)[_asset tracksWithMediaType:AVMediaTypeAudio].firstObject
            .formatDescriptions.firstObject);
    return asbd != NULL ? asbd->mChannelsPerFrame : 0;
}

- (NSTimeInterval)duration {
    CMTime d = _asset.duration;
    return CMTIME_IS_NUMERIC(d) ? CMTimeGetSeconds(d) : 0;
}

- (NSDictionary<NSString *, id> *)settings {
    const AudioStreamBasicDescription *asbd = CMAudioFormatDescriptionGetStreamBasicDescription(
        (__bridge CMAudioFormatDescriptionRef)[_asset tracksWithMediaType:AVMediaTypeAudio].firstObject
            .formatDescriptions.firstObject);
    if (asbd == NULL)
        return @{};
    NSMutableDictionary *s = [@{ AVFormatIDKey: @(asbd->mFormatID), AVSampleRateKey: @(asbd->mSampleRate),
                                 AVNumberOfChannelsKey: @(asbd->mChannelsPerFrame) } mutableCopy];
    if (asbd->mFormatID == kAudioFormatLinearPCM) {
        s[AVLinearPCMBitDepthKey] = @(asbd->mBitsPerChannel);
        s[AVLinearPCMIsFloatKey] = @((asbd->mFormatFlags & kAudioFormatFlagIsFloat) != 0);
        s[AVLinearPCMIsBigEndianKey] = @((asbd->mFormatFlags & kAudioFormatFlagIsBigEndian) != 0);
        s[AVLinearPCMIsNonInterleaved] = @NO;
    }
    return s;
}

- (void)setVolume:(float)volume {
    if (_fade != nil) {
        dispatch_source_cancel(_fade);
        _fade = nil;
    }
    _volume = volume < 0 ? 0 : volume > 1 ? 1 : volume;
    _engine.volume = _volume;
}

/* The volume moved there in steps over the duration. */
- (void)setVolume:(float)volume fadeDuration:(NSTimeInterval)duration {
    if (duration <= 0) {
        self.volume = volume;
        return;
    }
    float from = _volume, to = volume < 0 ? 0 : volume > 1 ? 1 : volume;
    double start = hostSeconds();
    if (_fade != nil)
        dispatch_source_cancel(_fade);
    _fade = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(_fade, DISPATCH_TIME_NOW, NSEC_PER_SEC / 50, NSEC_PER_SEC / 200);
    __weak AVAudioPlayer *weakSelf = self;
    dispatch_source_set_event_handler(_fade, ^{
        AVAudioPlayer *p = weakSelf;
        double f = (hostSeconds() - start) / duration;
        if (f >= 1)
            f = 1;
        p->_volume = from + (to - from) * f;
        p->_engine.volume = p->_volume;
        if (f >= 1) {
            dispatch_source_cancel(p->_fade);
            p->_fade = nil;
        }
    });
    dispatch_resume(_fade);
}

- (void)setRate:(float)rate {
    _rate = rate < 0.5 ? 0.5 : rate > 2 ? 2 : rate;
    if (_enableRate && _playing)
        _engine.rate = _rate;
}

- (NSTimeInterval)currentTime {
    return CMTimeGetSeconds(_engine.currentTime);
}

- (void)setCurrentTime:(NSTimeInterval)time {
    if (time < 0)
        time = 0;
    [_engine seekToTime:CMTimeMakeWithSeconds(time, 90000) completionHandler:nil];
}

- (NSTimeInterval)deviceCurrentTime {
    return hostSeconds();
}

- (void)setMeteringEnabled:(BOOL)enabled {
    _meteringEnabled = enabled;
    _engine.metering = enabled;
}

/* Levels in decibels from full scale: the peak, and the root mean square
   (the average power); -160 for silence. */
- (void)updateMeters {
    float peak[2], ms[2];
    [_engine takeLevelsPeak:peak meanSquare:ms count:2];
    for (int c = 0; c < 2; c++) {
        _levelsPeak[c] = peak[c] > 0 ? fmaxf(20 * log10f(peak[c]), -160) : -160;
        _levelsAverage[c] = ms[c] > 0 ? fmaxf(10 * log10f(ms[c]), -160) : -160;
    }
}

- (float)peakPowerForChannel:(NSUInteger)channel {
    return channel < 2 ? _levelsPeak[channel] : -160;
}

- (float)averagePowerForChannel:(NSUInteger)channel {
    return channel < 2 ? _levelsAverage[channel] : -160;
}

@end
