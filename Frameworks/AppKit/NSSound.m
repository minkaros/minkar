/* Copyright (c) 2006-2008 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

/* Minkar: a sound is read whole through ExtAudioFile (any format
   AudioToolbox reads) as float stereo at the file's rate, and played by
   its own default output unit, which converts to the device's rate. The
   sound keeps itself alive while it plays; its delegate hears the end on
   the main thread. Named sounds are looked up by name, then in the app,
   ~/Library/Sounds, /Library/Sounds and /System/Library/Sounds. */

#import <AppKit/NSSound.h>
#import <AppKit/NSPasteboard.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <AudioToolbox/AudioToolbox.h>
#include <dispatch/dispatch.h>
#include <stdlib.h>
#include <string.h>

NSString *const NSSoundPboardType = @"NSSoundPboardType";

static NSMutableDictionary *sNamed = nil;   /* name -> NSSound */

#define kMaxSeconds (10 * 60)

/* ---- Reading ------------------------------------------------------------- */

typedef struct {
    const unsigned char *bytes;
    SInt64 length;
} MemoryFile;

static OSStatus memoryRead(void *client, SInt64 position, UInt32 count, void *buffer, UInt32 *actual) {
    MemoryFile *m = client;
    if (position >= m->length) {
        *actual = 0;
        return noErr;
    }
    if (position + count > m->length)
        count = (UInt32)(m->length - position);
    memcpy(buffer, m->bytes + position, count);
    *actual = count;
    return noErr;
}

static SInt64 memorySize(void *client) {
    return ((MemoryFile *)client)->length;
}

/* The whole file as float stereo at its own rate. */
static BOOL readAll(ExtAudioFileRef file, float **outSamples, unsigned long *outFrames, double *outRate) {
    AudioStreamBasicDescription f;
    UInt32 size = sizeof(f);
    float *samples = NULL;
    unsigned long total = 0, capacity = 0;
    SInt64 length = 0;

    if (ExtAudioFileGetProperty(file, kExtAudioFileProperty_FileDataFormat, &size, &f) != noErr)
        return NO;
    *outRate = f.mSampleRate;
    memset(&f, 0, sizeof(f));
    f.mSampleRate = *outRate;
    f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    f.mBytesPerPacket = f.mBytesPerFrame = 2 * sizeof(float);
    f.mFramesPerPacket = 1;
    f.mChannelsPerFrame = 2;
    f.mBitsPerChannel = 32;
    if (ExtAudioFileSetProperty(file, kExtAudioFileProperty_ClientDataFormat, sizeof(f), &f) != noErr)
        return NO;
    size = sizeof(length);
    ExtAudioFileGetProperty(file, kExtAudioFileProperty_FileLengthFrames, &size, &length);
    capacity = length > 0 ? (unsigned long)length + 16 : 65536;
    for (;;) {
        UInt32 n;
        struct { UInt32 count; AudioBuffer buffer; } list;
        if (total == capacity) {
            if (capacity >= (unsigned long)(*outRate * kMaxSeconds))
                break;
            capacity *= 2;
        }
        if (samples == NULL || total == 0 || capacity > total) {
            float *grown = realloc(samples, capacity * 2 * sizeof(float));
            if (grown == NULL) {
                free(samples);
                return NO;
            }
            samples = grown;
        }
        n = (UInt32)(capacity - total < 8192 ? capacity - total : 8192);
        list.count = 1;
        list.buffer.mNumberChannels = 2;
        list.buffer.mDataByteSize = n * 2 * sizeof(float);
        list.buffer.mData = samples + total * 2;
        if (ExtAudioFileRead(file, &n, (AudioBufferList *)&list) != noErr || n == 0)
            break;
        total += n;
    }
    if (total == 0) {
        free(samples);
        return NO;
    }
    *outSamples = samples;
    *outFrames = total;
    return YES;
}

/* ---- Playing (the device's IO thread) ------------------------------------- */

@interface NSSound (Private)
- (OSStatus)_render:(UInt32)frames into:(float *)out;
- (void)_finished:(BOOL)completely;
@end

static OSStatus soundRender(void *refCon, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *time,
                            UInt32 bus, UInt32 frames, AudioBufferList *data) {
    NSSound *sound = refCon;
    return [sound _render:frames into:data->mBuffers[0].mData];
}

@implementation NSSound

+ (NSArray *)soundUnfilteredFileTypes {
    return [NSArray arrayWithObjects:@"aiff", @"aif", @"aifc", @"wav", @"wave", @"caf", @"AIFF", @"WAV", nil];
}

+ (NSArray *)soundUnfilteredTypes {
    return [NSArray arrayWithObjects:@"public.aiff-audio", @"public.aifc-audio", @"com.microsoft.waveform-audio",
                                     @"com.apple.coreaudio-format", nil];
}

+ (NSArray *)soundUnfilteredPasteboardTypes {
    return [NSArray arrayWithObject:NSSoundPboardType];
}

+ (BOOL)canInitWithPasteboard:(NSPasteboard *)pasteboard {
    return [pasteboard availableTypeFromArray:[self soundUnfilteredPasteboardTypes]] != nil;
}

static NSString *findNamed(NSString *name) {
    NSMutableArray *dirs = [NSMutableArray array];
    NSArray *types = [NSSound soundUnfilteredFileTypes];
    NSString *path = [[NSBundle mainBundle] pathForSoundResource:name];

    if (path != nil)
        return path;
    [dirs addObject:[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Sounds"]];
    [dirs addObject:@"/Library/Sounds"];
    [dirs addObject:@"/System/Library/Sounds"];
    for (NSString *dir in dirs)
        for (NSString *type in types) {
            NSString *candidate = [[dir stringByAppendingPathComponent:name] stringByAppendingPathExtension:type];
            if ([[NSFileManager defaultManager] fileExistsAtPath:candidate])
                return candidate;
        }
    return nil;
}

+ (NSSound *)soundNamed:(NSSoundName)name {
    NSSound *sound;

    if (name == nil)
        return nil;
    @synchronized(self) {
        if (sNamed == nil)
            sNamed = [[NSMutableDictionary alloc] init];
        sound = [sNamed objectForKey:name];
        if (sound == nil) {
            NSString *path = findNamed(name);
            if (path != nil) {
                sound = [[[NSSound alloc] initWithContentsOfFile:path byReference:YES] autorelease];
                if (sound != nil) {
                    sound->_name = [name copy];
                    [sNamed setObject:sound forKey:name];
                }
            }
        }
    }
    return sound;
}

- (id)_initWithFile:(ExtAudioFileRef)file {
    BOOL ok = file != NULL && readAll(file, &_samples, &_frames, &_rate);

    if (file != NULL)
        ExtAudioFileDispose(file);
    if (!ok) {
        [self release];
        return nil;
    }
    _volume = 1;
    return self;
}

- (id)initWithContentsOfURL:(NSURL *)url byReference:(BOOL)byReference {
    ExtAudioFileRef file = NULL;

    self = [super init];
    if (self == nil)
        return nil;
    if (url == nil || ExtAudioFileOpenURL((CFURLRef)url, &file) != noErr)
        file = NULL;
    self = [self _initWithFile:file];
    if (self != nil)
        _url = [url copy];
    return self;
}

- (id)initWithContentsOfFile:(NSString *)path byReference:(BOOL)byReference {
    return [self initWithContentsOfURL:path != nil ? [NSURL fileURLWithPath:path] : nil byReference:byReference];
}

- (id)initWithData:(NSData *)data {
    MemoryFile m = { [data bytes], (SInt64)[data length] };
    AudioFileID audioFile = NULL;
    ExtAudioFileRef file = NULL;

    self = [super init];
    if (self == nil)
        return nil;
    if (data == nil ||
        AudioFileOpenWithCallbacks(&m, memoryRead, NULL, memorySize, NULL, 0, &audioFile) != noErr ||
        ExtAudioFileWrapAudioFileID(audioFile, false, &file) != noErr) {
        if (audioFile != NULL)
            AudioFileClose(audioFile);
        [self release];
        return nil;
    }
    self = [self _initWithFile:file];   /* reads everything before the file goes */
    AudioFileClose(audioFile);
    return self;
}

- (id)initWithPasteboard:(NSPasteboard *)pasteboard {
    NSData *data = [pasteboard dataForType:NSSoundPboardType];

    if (data == nil) {
        [self release];
        return nil;
    }
    return [self initWithData:data];
}

- (void)writeToPasteboard:(NSPasteboard *)pasteboard {
    NSData *data = _url != nil ? [NSData dataWithContentsOfURL:_url] : nil;

    if (data == nil)
        return;
    [pasteboard declareTypes:[NSArray arrayWithObject:NSSoundPboardType] owner:nil];
    [pasteboard setData:data forType:NSSoundPboardType];
}

- (void)dealloc {
    [self stop];
    if (_unit != NULL)
        AudioComponentInstanceDispose(_unit);
    free(_samples);
    [_name release];
    [_url release];
    [_device release];
    [_channelMapping release];
    [super dealloc];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p name: %@ duration: %.3f>", [self class], self, _name, [self duration]];
}

/* ---- Names ---------------------------------------------------------------- */

- (BOOL)setName:(NSSoundName)name {
    @synchronized([NSSound class]) {
        if (sNamed == nil)
            sNamed = [[NSMutableDictionary alloc] init];
        if (name != nil && [sNamed objectForKey:name] != nil && [sNamed objectForKey:name] != self)
            return NO;
        if (_name != nil && [sNamed objectForKey:_name] == self)
            [sNamed removeObjectForKey:_name];
        [_name release];
        _name = [name copy];
        if (name != nil)
            [sNamed setObject:self forKey:name];
    }
    return YES;
}

- (NSSoundName)name {
    return _name;
}

/* ---- Playing --------------------------------------------------------------- */

- (OSStatus)_render:(UInt32)frames into:(float *)out {
    unsigned long at = _position, done = 0;
    float gain = _volume;
    BOOL ended = NO;

    if (_paused || !_playing) {
        memset(out, 0, frames * 2 * sizeof(float));
        return noErr;
    }
    while (done < frames) {
        unsigned long n = _frames - at < frames - done ? _frames - at : frames - done;
        for (unsigned long i = 0; i < n * 2; i++)
            out[done * 2 + i] = _samples[at * 2 + i] * gain;
        at += n;
        done += n;
        if (at >= _frames) {
            if (_loops) {
                at = 0;
            } else {
                ended = YES;
                break;
            }
        }
    }
    if (done < frames)
        memset(out + done * 2, 0, (frames - done) * 2 * sizeof(float));
    _position = at;
    if (ended) {
        _playing = NO;
        dispatch_async(dispatch_get_main_queue(), ^{
            [self _finished:YES];
        });
    }
    return noErr;
}

- (BOOL)_makeUnit {
    AudioComponentDescription d = { kAudioUnitType_Output, kAudioUnitSubType_DefaultOutput,
                                    kAudioUnitManufacturer_Apple, 0, 0 };
    AudioStreamBasicDescription f;
    AURenderCallbackStruct cb = { soundRender, self };
    AudioUnit unit = NULL;

    if (_unit != NULL)
        return YES;
    if (AudioComponentInstanceNew(AudioComponentFindNext(NULL, &d), &unit) != noErr)
        return NO;
    memset(&f, 0, sizeof(f));
    f.mSampleRate = _rate;
    f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    f.mBytesPerPacket = f.mBytesPerFrame = 2 * sizeof(float);
    f.mFramesPerPacket = 1;
    f.mChannelsPerFrame = 2;
    f.mBitsPerChannel = 32;
    if (AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &f, sizeof(f)) != noErr ||
        AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof(cb)) != noErr ||
        AudioUnitInitialize(unit) != noErr) {
        AudioComponentInstanceDispose(unit);
        return NO;
    }
    _unit = unit;
    return YES;
}

- (BOOL)play {
    if (_playing || _samples == NULL || ![self _makeUnit])
        return NO;
    [self retain];  /* until it finishes or stops */
    _position = 0;
    _paused = NO;
    _playing = YES;
    if (AudioOutputUnitStart(_unit) != noErr) {
        _playing = NO;
        [self release];
        return NO;
    }
    return YES;
}

- (BOOL)pause {
    if (!_playing || _paused)
        return NO;
    _paused = YES;
    return YES;
}

- (BOOL)resume {
    if (!_playing || !_paused)
        return NO;
    _paused = NO;
    return YES;
}

- (void)_finished:(BOOL)completely {
    if (_unit != NULL)
        AudioOutputUnitStop(_unit);
    _paused = NO;
    if ([_delegate respondsToSelector:@selector(sound:didFinishPlaying:)])
        [_delegate sound:self didFinishPlaying:completely];
    [self release];
}

- (BOOL)stop {
    if (!_playing)
        return NO;
    _playing = NO;
    [self _finished:NO];
    return YES;
}

- (BOOL)isPlaying {
    return _playing;
}

/* ---- Properties ------------------------------------------------------------ */

- (id<NSSoundDelegate>)delegate {
    return _delegate;
}

- (void)setDelegate:(id<NSSoundDelegate>)delegate {
    _delegate = delegate;
}

- (NSTimeInterval)duration {
    return _rate > 0 ? _frames / _rate : 0;
}

- (float)volume {
    return _volume;
}

- (void)setVolume:(float)volume {
    _volume = volume < 0 ? 0 : (volume > 1 ? 1 : volume);
}

- (NSTimeInterval)currentTime {
    return _rate > 0 ? _position / _rate : 0;
}

- (void)setCurrentTime:(NSTimeInterval)seconds {
    unsigned long at = seconds <= 0 ? 0 : (unsigned long)(seconds * _rate);
    _position = at < _frames ? at : _frames;
}

- (BOOL)loops {
    return _loops;
}

- (void)setLoops:(BOOL)loops {
    _loops = loops;
}

- (NSSoundPlaybackDeviceIdentifier)playbackDeviceIdentifier {
    return _device;
}

- (void)setPlaybackDeviceIdentifier:(NSSoundPlaybackDeviceIdentifier)identifier {
    if (identifier != _device) {
        [_device release];
        _device = [identifier copy];
    }
}

- (NSArray *)channelMapping {
    return _channelMapping;
}

- (void)setChannelMapping:(NSArray *)mapping {
    if (mapping != _channelMapping) {
        [_channelMapping release];
        _channelMapping = [mapping copy];
    }
}

@end

@implementation NSBundle (NSSound)

- (NSString *)pathForSoundResource:(NSString *)name {
    NSArray *types = [NSSound soundUnfilteredFileTypes];

    if ([[name pathExtension] length] > 0) {
        NSString *path = [self pathForResource:[name stringByDeletingPathExtension] ofType:[name pathExtension]];
        if (path != nil)
            return path;
    }
    for (NSString *type in types) {
        NSString *path = [self pathForResource:name ofType:type];
        if (path != nil)
            return path;
    }
    return nil;
}

@end
