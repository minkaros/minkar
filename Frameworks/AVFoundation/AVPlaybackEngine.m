/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Playback (AVPlaybackEngine.h). Each track reads through a decoding
 * session of its own. A seek raises the generation: whatever a reader
 * brings back from before it is dropped.
 */

#import "AVPlaybackEngine.h"
#import "AVAssetInternal.h"
#import "MediaDecoderTypes.h"
#import <AVFoundation/AVError.h>
#import <AudioToolbox/AudioQueue.h>
#include <mach/mach_time.h>

#define VIDEO_AHEAD 8			/* frames decoded ahead */
#define AUDIO_BUFFERS 3
#define AUDIO_BUFFER_FRAMES 4096

static double hostSeconds(void) {
    static mach_timebase_info_data_t tb;
    if (tb.denom == 0)
        mach_timebase_info(&tb);
    return (double)mach_absolute_time() * tb.numer / tb.denom / 1e9;
}

@implementation AVPlaybackEngine {
    NSCondition *_lock;
    NSUInteger _generation;
    BOOL _invalid;

    /* Video. */
    MDSessionRef _videoSession;
    CMPersistentTrackID _videoTrack;
    NSMutableArray *_frames;		/* CMSampleBuffers, in time order */
    BOOL _videoEnded;
    double _videoEndTime;		/* the end of the last frame decoded */
    NSThread *_videoThread;

    /* Audio. */
    MDSessionRef _audioSession;
    CMPersistentTrackID _audioTrack;
    AudioQueueRef _queue;
    AudioQueueBufferRef _buffers[AUDIO_BUFFERS];
    NSMutableArray *_freeBuffers;	/* NSValues of buffers not queued */
    CMSampleBufferRef _pending;		/* samples not yet copied out */
    size_t _pendingOffset;
    int _sampleRate, _channels;
    BOOL _audioEnded, _queueRunning, _queuePaused, _seeking;
    CMTime _audioStart;			/* the item time of the queue's sample 0 */
    BOOL _audioStartKnown;
    NSUInteger _queuedSinceStart;	/* buffers with samples since the queue started */
    double _audioEndTime;		/* item seconds of the last sample queued */
    double _lastAudioClock, _lastAudioClockHost;
    BOOL _audioClockDone;		/* the last samples heard: the host clock goes on */

    /* Levels, while metering. */
    float _peak[2];
    double _sumSquares[2];
    long _levelFrames;

    /* The clock. */
    float _rate;
    double _anchorTime, _anchorHost;
    float _volume;
    CMTimebaseRef _timebase;
}

- (instancetype)initWithAsset:(AVAsset *)asset error:(NSError **)outError {
    if ((self = [super init]) == nil)
        return nil;
    _lock = [[NSCondition alloc] init];
    _frames = [NSMutableArray array];
    _freeBuffers = [NSMutableArray array];
    _volume = 1;
    if (![asset _loadReturningError:outError])
        return nil;
    AVAssetTrack *video = [asset tracksWithMediaType:AVMediaTypeVideo].firstObject;
    AVAssetTrack *audio = [asset tracksWithMediaType:AVMediaTypeAudio].firstObject;
    _duration = asset.duration;
    if (video != nil) {
        _videoSession = [asset _createSessionReturningError:outError];
        if (_videoSession == NULL)
            return nil;
        _videoTrack = video.trackID;
        NSDictionary *o = @{ @kMDKeyOutputPixelFormat: @(kCVPixelFormatType_32BGRA) };
        if (_MDSessionConfigure(_videoSession, _videoTrack, (__bridge CFDictionaryRef)o) != kMDSuccess) {
            _MDSessionRelease(_videoSession);
            _videoSession = NULL;
        } else {
            _hasVideo = YES;
            _presentationSize = video.naturalSize;
        }
    }
    if (audio != nil && [self _setUpAudio:asset track:audio])
        _hasAudio = YES;
    if (!_hasVideo && !_hasAudio) {
        if (outError != NULL && *outError == nil)
            *outError = _AVErrorForDecoderResult(kMDErrorDecode, nil);
        return nil;
    }
    CMTimebaseCreateWithSourceClock(NULL, CMClockGetHostTimeClock(), &_timebase);
    if (_hasVideo) {
        _videoThread = [[NSThread alloc] initWithTarget:self selector:@selector(_decodeVideo) object:nil];
        _videoThread.name = @"AVPlaybackEngine video";
        [_videoThread start];
    }
    return self;
}

- (void)dealloc {
    [self invalidate];
    if (_timebase != NULL)
        CFRelease(_timebase);
}

- (void)invalidate {
    [_lock lock];
    if (_invalid) {
        [_lock unlock];
        return;
    }
    _invalid = YES;
    _generation++;
    [_lock broadcast];
    [_lock unlock];
    if (_queue != NULL) {
        AudioQueueStop(_queue, true);
        AudioQueueDispose(_queue, true);
        _queue = NULL;
    }
    /* The video thread ends on its own; its session goes with the last
       reference. */
    [_lock lock];
    [_frames removeAllObjects];
    if (_pending != NULL)
        CFRelease(_pending);
    _pending = NULL;
    [_lock unlock];
    if (_audioSession != NULL)
        _MDSessionRelease(_audioSession);
    _audioSession = NULL;
}

/* ---- Video ---------------------------------------------------------------- */

- (void)_decodeVideo {
    MDSessionRef session = _videoSession;
    for (;;) {
        [_lock lock];
        while (!_invalid && (_seeking || _videoEnded || _frames.count >= VIDEO_AHEAD))
            [_lock wait];
        if (_invalid) {
            [_lock unlock];
            break;
        }
        NSUInteger generation = _generation;
        [_lock unlock];

        CMSampleBufferRef sb = NULL;
        Boolean ended = false;
        int32_t r = _MDSessionCopyNextSample(session, _videoTrack, &sb, &ended);

        [_lock lock];
        if (generation == _generation) {
            if (r != kMDSuccess) {
                _error = _AVErrorForDecoderResult(r, nil);
                _videoEnded = YES;
            } else if (ended)
                _videoEnded = YES;
            else if (sb != NULL) {
                [_frames addObject:(__bridge id)sb];
                CMTime d = CMSampleBufferGetDuration(sb);
                _videoEndTime = CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sb)) +
                                (CMTIME_IS_NUMERIC(d) ? CMTimeGetSeconds(d) : 0);
            }
        }
        [_lock broadcast];
        [_lock unlock];
        if (sb != NULL)
            CFRelease(sb);
        if (r == kMDErrorNoService)
            break;
    }
    _MDSessionRelease(session);
}

static CMTime frameTime(id sb) {
    return CMSampleBufferGetPresentationTimeStamp((__bridge CMSampleBufferRef)sb);
}

- (CVPixelBufferRef)_copyFrameForTime:(CMTime)time frameTime:(CMTime *)outTime drop:(BOOL)drop {
    [_lock lock];
    NSInteger due = -1;
    for (NSUInteger i = 0; i < _frames.count; i++) {
        if (CMTimeCompare(frameTime(_frames[i]), time) <= 0)
            due = i;
        else
            break;
    }
    /* Before the first frame (just after a seek): the first one. */
    if (due < 0 && _frames.count > 0 && CMTimeCompare(time, kCMTimeZero) <= 0)
        due = 0;
    CVPixelBufferRef pb = NULL;
    if (due >= 0) {
        CMSampleBufferRef sb = (__bridge CMSampleBufferRef)_frames[due];
        pb = CMSampleBufferGetImageBuffer(sb);
        if (pb != NULL)
            CVPixelBufferRetain(pb);
        if (outTime != NULL)
            *outTime = CMSampleBufferGetPresentationTimeStamp(sb);
        if (drop && due > 0) {
            [_frames removeObjectsInRange:NSMakeRange(0, due)];
            [_lock broadcast];
        }
    }
    [_lock unlock];
    return pb;
}

- (CVPixelBufferRef)copyFrameForTime:(CMTime)time frameTime:(CMTime *)outTime {
    return [self _copyFrameForTime:time frameTime:outTime drop:YES];
}

- (CVPixelBufferRef)copyFrameAtTime:(CMTime)time frameTime:(CMTime *)outTime {
    return [self _copyFrameForTime:time frameTime:outTime drop:NO];
}

/* ---- Audio ---------------------------------------------------------------- */

static void audioCallback(void *refcon, AudioQueueRef queue, AudioQueueBufferRef buffer) {
    AVPlaybackEngine *engine = (__bridge AVPlaybackEngine *)refcon;
    [engine _fill:buffer];
}

- (BOOL)_setUpAudio:(AVAsset *)asset track:(AVAssetTrack *)track {
    NSError *error = nil;
    _audioSession = [asset _createSessionReturningError:&error];
    if (_audioSession == NULL)
        return NO;
    _audioTrack = track.trackID;
    const AudioStreamBasicDescription *in = CMAudioFormatDescriptionGetStreamBasicDescription(
        (__bridge CMAudioFormatDescriptionRef)track.formatDescriptions.firstObject);
    _sampleRate = in != NULL && in->mSampleRate > 0 ? (int)in->mSampleRate : 48000;
    _channels = in != NULL && in->mChannelsPerFrame == 1 ? 1 : 2;
    NSDictionary *o = @{ @kMDKeyOutputFloat: @YES, @kMDKeyOutputSampleRate: @(_sampleRate),
                         @kMDKeyOutputChannels: @(_channels) };
    AudioStreamBasicDescription f = { 0 };
    f.mSampleRate = _sampleRate;
    f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    f.mChannelsPerFrame = _channels;
    f.mBitsPerChannel = 32;
    f.mBytesPerFrame = f.mBytesPerPacket = 4 * _channels;
    f.mFramesPerPacket = 1;
    if (_MDSessionConfigure(_audioSession, _audioTrack, (__bridge CFDictionaryRef)o) != kMDSuccess ||
        AudioQueueNewOutput(&f, audioCallback, (__bridge void *)self, NULL, NULL, 0, &_queue) != noErr) {
        _MDSessionRelease(_audioSession);
        _audioSession = NULL;
        _queue = NULL;
        return NO;
    }
    for (int i = 0; i < AUDIO_BUFFERS; i++) {
        AudioQueueAllocateBuffer(_queue, AUDIO_BUFFER_FRAMES * f.mBytesPerFrame, &_buffers[i]);
        [_freeBuffers addObject:[NSValue valueWithPointer:_buffers[i]]];
    }
    return YES;
}

/* The buffer filled with the next samples and queued; kept free when
   there are none (the end, a seek, or a pause at another rate). */
- (void)_fill:(AudioQueueBufferRef)buffer {
    UInt32 bpf = 4 * _channels, cap = AUDIO_BUFFER_FRAMES * bpf, bytes = 0;
    [_lock lock];
    NSUInteger generation = _generation;
    BOOL usable = !_invalid && !_seeking && _rate == 1 && !_audioEnded;
    [_lock unlock];
    CMTime first = kCMTimeInvalid;
    while (usable && bytes < cap) {
        if (_pending == NULL) {
            CMSampleBufferRef sb = NULL;
            Boolean ended = false;
            int32_t r = _MDSessionCopyNextSample(_audioSession, _audioTrack, &sb, &ended);
            [_lock lock];
            BOOL current = generation == _generation;
            if (current && (r != kMDSuccess || ended))
                _audioEnded = YES;
            if (current && r != kMDSuccess)
                _error = _AVErrorForDecoderResult(r, nil);
            [_lock unlock];
            if (!current) {
                if (sb != NULL)
                    CFRelease(sb);
                bytes = 0;
                break;
            }
            if (sb == NULL)
                break;
            _pending = sb;
            _pendingOffset = 0;
        }
        CMBlockBufferRef block = CMSampleBufferGetDataBuffer(_pending);
        size_t length = CMBlockBufferGetDataLength(block);
        if (!CMTIME_IS_VALID(first))
            first = CMTimeAdd(CMSampleBufferGetPresentationTimeStamp(_pending),
                              CMTimeMake((int64_t)(_pendingOffset / bpf), _sampleRate));
        size_t n = MIN(length - _pendingOffset, (size_t)(cap - bytes));
        CMBlockBufferCopyDataBytes(block, _pendingOffset, n, (char *)buffer->mAudioData + bytes);
        if (_metering)
            [self _measure:(const float *)((char *)buffer->mAudioData + bytes) frames:n / bpf];
        bytes += (UInt32)n;
        _pendingOffset += n;
        if (_pendingOffset >= length) {
            CFRelease(_pending);
            _pending = NULL;
        }
    }
    [_lock lock];
    if (bytes > 0 && generation == _generation && !_invalid) {
        buffer->mAudioDataByteSize = bytes;
        if (!_audioStartKnown && CMTIME_IS_VALID(first)) {
            _audioStart = first;
            _audioStartKnown = YES;
        }
        _audioEndTime = CMTimeGetSeconds(first) + (double)(bytes / bpf) / _sampleRate;
        _queuedSinceStart++;
        [_lock unlock];
        AudioQueueEnqueueBuffer(_queue, buffer, 0, NULL);
        /* The queue starts with its first samples (after a seek, the
           buffers may come back after the play). */
        [_lock lock];
        BOOL start = !_queueRunning && _rate == 1 && !_invalid && !_seeking;
        if (start) {
            _queueRunning = YES;
            _queuePaused = NO;
        }
        [_lock unlock];
        if (start)
            AudioQueueStart(_queue, NULL);
        return;
    }
    [_freeBuffers addObject:[NSValue valueWithPointer:buffer]];
    [_lock unlock];
}

- (void)_measure:(const float *)samples frames:(size_t)frames {
    [_lock lock];
    for (size_t i = 0; i < frames; i++)
        for (int c = 0; c < _channels; c++) {
            float v = fabsf(samples[i * _channels + c]);
            if (v > _peak[c])
                _peak[c] = v;
            _sumSquares[c] += (double)v * v;
        }
    _levelFrames += frames;
    [_lock unlock];
}

- (int)audioChannels {
    return _hasAudio ? _channels : 0;
}

- (void)takeLevelsPeak:(float *)peak meanSquare:(float *)meanSquare count:(int)count {
    [_lock lock];
    for (int c = 0; c < count; c++) {
        int k = c < _channels ? c : _channels - 1;
        peak[c] = k >= 0 ? _peak[k] : 0;
        meanSquare[c] = k >= 0 && _levelFrames > 0 ? (float)(_sumSquares[k] / _levelFrames) : 0;
    }
    memset(_peak, 0, sizeof(_peak));
    memset(_sumSquares, 0, sizeof(_sumSquares));
    _levelFrames = 0;
    [_lock unlock];
}

/* Free buffers filled and queued (outside the lock). */
- (void)_fillFreeBuffers {
    for (;;) {
        [_lock lock];
        NSValue *v = _freeBuffers.firstObject;
        if (v != nil)
            [_freeBuffers removeObjectAtIndex:0];
        BOOL more = v != nil && !_audioEnded && !_seeking && _rate == 1;
        [_lock unlock];
        if (v == nil)
            return;
        [self _fill:v.pointerValue];
        if (!more)
            return;
    }
}

/* ---- The clock ------------------------------------------------------------- */

/* The audio queue's time, while it is the clock: the sample being heard,
   from the first sample queued since it started. */
- (BOOL)_audioClockTime:(double *)out {
    if (_queue == NULL || !_queueRunning || _queuePaused || !_audioStartKnown)
        return NO;
    AudioTimeStamp ts;
    if (AudioQueueGetCurrentTime(_queue, NULL, &ts, NULL) != noErr)
        return NO;
    double t = CMTimeGetSeconds(_audioStart) + ts.mSampleTime / _sampleRate, host = hostSeconds();
    /* After the last sample only silence goes out, which the queue's time
       does not count: once it stands still at the end, the host clock
       goes on from there (with what was still on its way). */
    if (_audioClockDone)
        return NO;
    if (t != _lastAudioClock) {
        _lastAudioClock = t;
        _lastAudioClockHost = host;
    } else if (_audioEnded && host - _lastAudioClockHost > 0.05) {
        _audioClockDone = YES;
        return NO;
    }
    *out = t;
    return YES;
}

- (CMTime)currentTime {
    [_lock lock];
    double t = _anchorTime;
    if (_rate != 0) {
        double now = hostSeconds(), audio;
        if (_rate == 1 && [self _audioClockTime:&audio]) {
            _anchorTime = audio;
            _anchorHost = now;
        }
        t = _anchorTime + (now - _anchorHost) * _rate;
    }
    double end = CMTIME_IS_NUMERIC(_duration) ? CMTimeGetSeconds(_duration) : INFINITY;
    if (t > end)
        t = end;
    if (t < 0)
        t = 0;
    [_lock unlock];
    return CMTimeMakeWithSeconds(t, 90000);
}

- (float)rate {
    return _rate;
}

- (void)setRate:(float)rate {
    if (rate < 0)
        rate = 0;
    CMTime now = [self currentTime];
    [_lock lock];
    float was = _rate;
    _anchorTime = CMTimeGetSeconds(now);
    _anchorHost = hostSeconds();
    _rate = rate;
    [_lock unlock];
    CMTimebaseSetRateAndAnchorTime(_timebase, rate, now, CMClockGetTime(CMClockGetHostTimeClock()));
    if (_queue == NULL || was == rate)
        return;
    if (rate == 1) {
        [self _startAudio];
    } else if (was == 1) {
        AudioQueuePause(_queue);
        [_lock lock];
        _queuePaused = YES;
        [_lock unlock];
    }
}

/* Audio from the clock's time: queued afresh after a stop (the queue's
   samples count from 0 again), or resumed after a pause. */
- (void)_startAudio {
    [_lock lock];
    BOOL resume = _queueRunning && _queuePaused;
    _queuePaused = NO;
    [_lock unlock];
    if (resume) {
        AudioQueueStart(_queue, NULL);
        return;
    }
    [self _fillFreeBuffers];
}

- (void)setVolume:(float)volume {
    _volume = volume;
    if (_queue != NULL)
        AudioQueueSetParameter(_queue, kAudioQueueParam_Volume, volume);
}

- (float)volume {
    return _volume;
}

- (CMTimebaseRef)timebase {
    return _timebase;
}

/* Every track's end reached: the last frame shown for its duration, the
   last samples heard. The time then stays at the item's end. */
- (BOOL)isAtEnd {
    CMTime now = [self currentTime];
    [_lock lock];
    double t = CMTimeGetSeconds(now);
    double end = CMTIME_IS_NUMERIC(_duration) ? CMTimeGetSeconds(_duration) : INFINITY;
    BOOL videoDone = !_hasVideo || (_videoEnded && _frames.count <= 1 && t >= _videoEndTime - 0.001);
    BOOL audioDone = !_hasAudio ||
                     (_audioEnded && (_audioClockDone || _audioEndTime == 0 || _queue == NULL ||
                                      t >= _audioEndTime - 0.001));
    BOOL atEnd = (videoDone && audioDone) || t >= end - 0.0005;
    if (atEnd) {
        _anchorTime = isfinite(end) ? end : t;
        _anchorHost = hostSeconds();
    }
    [_lock unlock];
    return atEnd;
}

/* ---- Seeking ----------------------------------------------------------------- */

- (void)seekToTime:(CMTime)time completionHandler:(void (^)(BOOL))handler {
    if (!CMTIME_IS_NUMERIC(time))
        time = kCMTimeZero;
    if (CMTimeCompare(time, kCMTimeZero) < 0)
        time = kCMTimeZero;
    [_lock lock];
    if (_invalid) {
        [_lock unlock];
        if (handler != nil)
            handler(NO);
        return;
    }
    _generation++;
    _seeking = YES;
    float rate = _rate;
    [_frames removeAllObjects];
    _videoEnded = NO;
    _videoEndTime = 0;
    _audioEnded = NO;
    _audioStartKnown = NO;
    _queuedSinceStart = 0;
    _audioEndTime = 0;
    _audioClockDone = NO;
    _lastAudioClock = -1;
    [_lock unlock];

    if (_queue != NULL) {
        AudioQueueStop(_queue, true);	/* its buffers come back free */
        [_lock lock];
        _queueRunning = _queuePaused = NO;
        if (_pending != NULL)
            CFRelease(_pending);
        _pending = NULL;
        [_lock unlock];
    }
    /* The sessions are seeked under their own locks: a read in flight
       finishes first and is dropped (an older generation). */
    if (_videoSession != NULL)
        _MDSessionSeek(_videoSession, time);
    if (_audioSession != NULL)
        _MDSessionSeek(_audioSession, time);

    [_lock lock];
    _anchorTime = CMTimeGetSeconds(time);
    _anchorHost = hostSeconds();
    _seeking = NO;
    [_lock broadcast];
    [_lock unlock];
    CMTimebaseSetTime(_timebase, time);
    if (_queue != NULL && rate == 1)
        [self _startAudio];
    if (handler != nil)
        handler(YES);
}

@end
