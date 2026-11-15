/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Playback for WebKit (AVMinkarPlayback.h). A stream is a temporary file
 * (removed when the last reference goes) and, for each decoding session
 * reading it, a pipe whose write end is closed when the download is over:
 * the decoder waits for data until then.
 */

#import "AVMinkarPlayback.h"
#import "AVAssetInternal.h"
#import "AVPlaybackEngine.h"
#import <AVFoundation/AVAssetReader.h>
#import <AVFoundation/AVAudioSettings.h>
#import "MediaDecoderTypes.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#include <unistd.h>

/* ---- Streams ---------------------------------------------------------------- */

struct AVMinkarStream {
    atomic_int references;
    pthread_mutex_t lock;
    char *path;
    int fd;
    int64_t expected;
    _Atomic int64_t length;
    bool finished;
    int *pipes;			/* the write ends still open */
    int pipeCount;
};

AVMinkarStreamRef AVMinkarStreamCreate(int64_t expectedLength) {
    NSString *t = [NSTemporaryDirectory() stringByAppendingPathComponent:@"AVMinkarStream-XXXXXX"];
    char *path = strdup(t.fileSystemRepresentation);
    int fd = mkstemp(path);
    if (fd >= 0)
        fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (fd < 0) {
        free(path);
        return NULL;
    }
    struct AVMinkarStream *s = calloc(1, sizeof(*s));
    atomic_init(&s->references, 1);
    pthread_mutex_init(&s->lock, NULL);
    s->path = path;
    s->fd = fd;
    s->expected = expectedLength;
    return s;
}

static AVMinkarStreamRef streamRetain(AVMinkarStreamRef s) {
    atomic_fetch_add(&s->references, 1);
    return s;
}

bool AVMinkarStreamAppend(AVMinkarStreamRef s, const void *bytes, size_t length) {
    const char *p = bytes;
    while (length > 0) {
        ssize_t n = write(s->fd, p, length);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        p += n;
        length -= n;
        atomic_fetch_add(&s->length, n);
    }
    return true;
}

void AVMinkarStreamFinish(AVMinkarStreamRef s) {
    pthread_mutex_lock(&s->lock);
    s->finished = true;
    for (int i = 0; i < s->pipeCount; i++)
        close(s->pipes[i]);
    s->pipeCount = 0;
    pthread_mutex_unlock(&s->lock);
}

int64_t AVMinkarStreamGetLength(AVMinkarStreamRef s) {
    return atomic_load(&s->length);
}

int64_t AVMinkarStreamGetExpectedLength(AVMinkarStreamRef s) {
    return s->expected;
}

bool AVMinkarStreamIsFinished(AVMinkarStreamRef s) {
    pthread_mutex_lock(&s->lock);
    bool f = s->finished;
    pthread_mutex_unlock(&s->lock);
    return f;
}

/* A session's pipe: its read end, for the decoder; already at its end when
   the download is over. */
static int streamSessionPipe(AVMinkarStreamRef s) {
    int p[2];
    if (pipe(p) != 0)
        return -1;
    fcntl(p[0], F_SETFD, FD_CLOEXEC);
    fcntl(p[1], F_SETFD, FD_CLOEXEC);
    pthread_mutex_lock(&s->lock);
    if (s->finished)
        close(p[1]);
    else {
        s->pipes = realloc(s->pipes, sizeof(int) * (s->pipeCount + 1));
        s->pipes[s->pipeCount++] = p[1];
    }
    pthread_mutex_unlock(&s->lock);
    return p[0];
}

void AVMinkarStreamRelease(AVMinkarStreamRef s) {
    if (s == NULL || atomic_fetch_sub(&s->references, 1) != 1)
        return;
    for (int i = 0; i < s->pipeCount; i++)
        close(s->pipes[i]);
    free(s->pipes);
    close(s->fd);
    unlink(s->path);
    free(s->path);
    pthread_mutex_destroy(&s->lock);
    free(s);
}

/* An asset read from a stream: each session opens the file anew (its own
   offset) with a pipe of its own. */
@interface AVMinkarStreamAsset : AVAsset
- (instancetype)initWithStream:(AVMinkarStreamRef)stream name:(NSString *)name;
@end

@implementation AVMinkarStreamAsset {
    AVMinkarStreamRef _stream;
    NSString *_name;
}

- (instancetype)initWithStream:(AVMinkarStreamRef)stream name:(NSString *)name {
    if ((self = [super init]) != nil) {
        _stream = streamRetain(stream);
        _name = [name copy] ?: @"";
    }
    return self;
}

- (void)dealloc {
    AVMinkarStreamRelease(_stream);
}

- (MDSessionRef)_createSessionReturningError:(NSError **)outError {
    int fd = open(_stream->path, O_RDONLY | O_CLOEXEC);
    int done = streamSessionPipe(_stream);
    int32_t result = kMDErrorCannotOpen;
    MDSessionRef s = NULL;
    if (fd >= 0 && done >= 0)
        s = _MDSessionCreateStreaming(fd, done, _name.lastPathComponent.UTF8String, _stream->expected, &result);
    if (fd >= 0)
        close(fd);
    if (done >= 0)
        close(done);
    if (s == NULL && outError != NULL)
        *outError = _AVErrorForDecoderResult(result, nil);
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

/* ---- Playback ---------------------------------------------------------------- */

@interface AVMinkarPlaybackObject : NSObject
@property (strong) AVAsset *asset;
@property (strong) AVPlaybackEngine *engine;
@property (strong) NSError *error;
@property AVMinkarPlaybackCallback callback;
@property void *context;
@property (strong) dispatch_queue_t queue;
@property BOOL released;
@property BOOL endSaid;
@end

@implementation AVMinkarPlaybackObject
@end

static void say(AVMinkarPlaybackObject *p, int event) {
    if (p.released || p.callback == NULL)
        return;
    p.callback(p.context, event);
}

static AVMinkarPlaybackRef create(AVAsset *asset, AVMinkarPlaybackCallback callback, void *context) {
    AVMinkarPlaybackObject *p = [[AVMinkarPlaybackObject alloc] init];
    p.asset = asset;
    p.callback = callback;
    p.context = context;
    p.queue = dispatch_queue_create("org.minkar.AVMinkarPlayback", DISPATCH_QUEUE_SERIAL);
    dispatch_async(p.queue, ^{
        NSError *error = nil;
        AVPlaybackEngine *engine = [[AVPlaybackEngine alloc] initWithAsset:asset error:&error];
        if (p.released) {
            [engine invalidate];
            return;
        }
        p.engine = engine;
        p.error = error;
        say(p, engine != nil ? kAVMinkarPlaybackReady : kAVMinkarPlaybackFailed);
    });
    return (AVMinkarPlaybackRef)CFBridgingRetain(p);
}

static AVMinkarPlaybackObject *object(AVMinkarPlaybackRef ref) {
    return (__bridge AVMinkarPlaybackObject *)(void *)ref;
}

AVMinkarPlaybackRef AVMinkarPlaybackCreateWithStream(AVMinkarStreamRef stream, const char *name,
                                                     AVMinkarPlaybackCallback callback, void *context) {
    @autoreleasepool {
        AVMinkarStreamAsset *asset = [[AVMinkarStreamAsset alloc] initWithStream:stream
                                                                            name:name != NULL ? @(name) : nil];
        return create(asset, callback, context);
    }
}

AVMinkarPlaybackRef AVMinkarPlaybackCreateWithPath(const char *path, AVMinkarPlaybackCallback callback,
                                                   void *context) {
    @autoreleasepool {
        AVURLAsset *asset = [AVURLAsset URLAssetWithURL:[NSURL fileURLWithPath:@(path)] options:nil];
        return create(asset, callback, context);
    }
}

void AVMinkarPlaybackRelease(AVMinkarPlaybackRef ref) {
    if (ref == NULL)
        return;
    AVMinkarPlaybackObject *p = CFBridgingRelease(ref);
    p.released = true;
    AVPlaybackEngine *engine = p.engine;
    [engine invalidate];
    /* What is under way on its queue ends there, then lets it go. */
    dispatch_async(p.queue, ^{ [p.engine invalidate]; });
}

bool AVMinkarPlaybackHasVideo(AVMinkarPlaybackRef ref) {
    return object(ref).engine.hasVideo;
}

bool AVMinkarPlaybackHasAudio(AVMinkarPlaybackRef ref) {
    return object(ref).engine.hasAudio;
}

void AVMinkarPlaybackGetNaturalSize(AVMinkarPlaybackRef ref, double *width, double *height) {
    CGSize s = object(ref).engine.presentationSize;
    if (width != NULL)
        *width = s.width;
    if (height != NULL)
        *height = s.height;
}

double AVMinkarPlaybackGetDuration(AVMinkarPlaybackRef ref) {
    AVPlaybackEngine *e = object(ref).engine;
    return e != nil && CMTIME_IS_NUMERIC(e.duration) ? CMTimeGetSeconds(e.duration) : NAN;
}

double AVMinkarPlaybackGetCurrentTime(AVMinkarPlaybackRef ref) {
    AVPlaybackEngine *e = object(ref).engine;
    return e != nil ? CMTimeGetSeconds(e.currentTime) : 0;
}

void AVMinkarPlaybackSetRate(AVMinkarPlaybackRef ref, float rate) {
    AVMinkarPlaybackObject *p = object(ref);
    if (rate != 0)
        p.endSaid = NO;
    p.engine.rate = rate;
}

float AVMinkarPlaybackGetRate(AVMinkarPlaybackRef ref) {
    return object(ref).engine.rate;
}

void AVMinkarPlaybackSetVolume(AVMinkarPlaybackRef ref, float volume) {
    object(ref).engine.volume = volume;
}

void AVMinkarPlaybackSeek(AVMinkarPlaybackRef ref, double seconds) {
    AVMinkarPlaybackObject *p = object(ref);
    p.endSaid = NO;
    dispatch_async(p.queue, ^{
        [p.engine seekToTime:CMTimeMakeWithSeconds(seconds, 90000) completionHandler:nil];
        say(p, kAVMinkarPlaybackSeeked);
    });
}

CVPixelBufferRef AVMinkarPlaybackCopyFrame(AVMinkarPlaybackRef ref, double time, double *frameTime) {
    CMTime t = kCMTimeInvalid;
    CVPixelBufferRef pb = [object(ref).engine copyFrameForTime:CMTimeMakeWithSeconds(time, 90000) frameTime:&t];
    if (pb != NULL && frameTime != NULL)
        *frameTime = CMTimeGetSeconds(t);
    return pb;
}

bool AVMinkarPlaybackIsAtEnd(AVMinkarPlaybackRef ref) {
    AVMinkarPlaybackObject *p = object(ref);
    AVPlaybackEngine *e = p.engine;
    if (e == nil || e.rate == 0 || !e.isAtEnd)
        return false;
    if (!p.endSaid) {
        p.endSaid = YES;
        e.rate = 0;
        dispatch_async(p.queue, ^{ say(p, kAVMinkarPlaybackEnded); });
    }
    return true;
}

CFErrorRef AVMinkarPlaybackCopyError(AVMinkarPlaybackRef ref) {
    AVMinkarPlaybackObject *p = object(ref);
    NSError *e = p.error ?: p.engine.error;
    return e != nil ? (CFErrorRef)CFBridgingRetain(e) : NULL;
}

/* ---- Decoding a sound file ---------------------------------------------------- */

bool AVMinkarDecodeAudio(const void *bytes, size_t length, double sampleRate, bool mixToMono, int *channelsOut,
                         size_t *framesOut, float **samplesOut) {
    @autoreleasepool {
        NSString *path = [NSTemporaryDirectory() stringByAppendingPathComponent:
            [NSString stringWithFormat:@"AVMinkarDecode-%@", [NSUUID UUID].UUIDString]];
        if (![[NSData dataWithBytesNoCopy:(void *)bytes length:length freeWhenDone:NO] writeToFile:path atomically:NO])
            return false;
        AVURLAsset *asset = [AVURLAsset URLAssetWithURL:[NSURL fileURLWithPath:path] options:nil];
        AVAssetTrack *track = [asset tracksWithMediaType:AVMediaTypeAudio].firstObject;
        bool ok = false;
        if (track != nil) {
            const AudioStreamBasicDescription *in = CMAudioFormatDescriptionGetStreamBasicDescription(
                (__bridge CMAudioFormatDescriptionRef)track.formatDescriptions.firstObject);
            int channels = mixToMono ? 1 : (in != NULL && in->mChannelsPerFrame > 0 ? (int)in->mChannelsPerFrame : 2);
            AVAssetReader *reader = [AVAssetReader assetReaderWithAsset:asset error:NULL];
            AVAssetReaderTrackOutput *output = [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:track
                outputSettings:@{ AVFormatIDKey: @(kAudioFormatLinearPCM), AVLinearPCMIsFloatKey: @YES,
                                  AVLinearPCMBitDepthKey: @32, AVSampleRateKey: @(sampleRate),
                                  AVNumberOfChannelsKey: @(channels) }];
            [reader addOutput:output];
            if ([reader startReading]) {
                NSMutableData *all = [NSMutableData data];
                CMSampleBufferRef sb;
                while ((sb = [output copyNextSampleBuffer]) != NULL) {
                    CMBlockBufferRef block = CMSampleBufferGetDataBuffer(sb);
                    size_t n = CMBlockBufferGetDataLength(block);
                    size_t at = all.length;
                    all.length = at + n;
                    CMBlockBufferCopyDataBytes(block, 0, n, (char *)all.mutableBytes + at);
                    CFRelease(sb);
                }
                if (reader.status == AVAssetReaderStatusCompleted && all.length > 0) {
                    *samplesOut = malloc(all.length);
                    memcpy(*samplesOut, all.bytes, all.length);
                    *channelsOut = channels;
                    *framesOut = all.length / (sizeof(float) * channels);
                    ok = true;
                }
            }
        }
        [[NSFileManager defaultManager] removeItemAtPath:path error:NULL];
        return ok;
    }
}
