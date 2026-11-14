/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSBundle.h>
#import <Foundation/NSDate.h>
#import <AppKit/AppKitExport.h>

@class NSData, NSPasteboard, NSSound, NSURL;

typedef NSString *NSSoundName;
typedef NSString *NSSoundPlaybackDeviceIdentifier;

APPKIT_EXPORT NSString *const NSSoundPboardType;

@protocol NSSoundDelegate <NSObject>
@optional
- (void)sound:(NSSound *)sound didFinishPlaying:(BOOL)flag;
@end

@interface NSSound : NSObject {
    NSString *_name;
    NSURL *_url;
    float *_samples;            /* interleaved stereo float */
    unsigned long _frames;
    double _rate;
    void *_unit;
    volatile unsigned long _position;
    volatile BOOL _playing, _paused;
    float _volume;
    BOOL _loops;
    id<NSSoundDelegate> _delegate;
    NSString *_device;
    NSArray *_channelMapping;
}

+ (NSArray *)soundUnfilteredFileTypes;
+ (NSArray *)soundUnfilteredTypes;
+ (NSArray *)soundUnfilteredPasteboardTypes;
+ (BOOL)canInitWithPasteboard:(NSPasteboard *)pasteboard;

+ (NSSound *)soundNamed:(NSSoundName)name;

- (id)initWithContentsOfFile:(NSString *)path byReference:(BOOL)byReference;
- (id)initWithContentsOfURL:(NSURL *)url byReference:(BOOL)byReference;
- (id)initWithData:(NSData *)data;
- (id)initWithPasteboard:(NSPasteboard *)pasteboard;
- (void)writeToPasteboard:(NSPasteboard *)pasteboard;

- (BOOL)setName:(NSSoundName)name;
- (NSSoundName)name;

- (BOOL)play;
- (BOOL)pause;
- (BOOL)resume;
- (BOOL)stop;
- (BOOL)isPlaying;

- (id<NSSoundDelegate>)delegate;
- (void)setDelegate:(id<NSSoundDelegate>)delegate;
- (NSTimeInterval)duration;
- (float)volume;
- (void)setVolume:(float)volume;
- (NSTimeInterval)currentTime;
- (void)setCurrentTime:(NSTimeInterval)seconds;
- (BOOL)loops;
- (void)setLoops:(BOOL)loops;
- (NSSoundPlaybackDeviceIdentifier)playbackDeviceIdentifier;
- (void)setPlaybackDeviceIdentifier:(NSSoundPlaybackDeviceIdentifier)identifier;
- (NSArray *)channelMapping;
- (void)setChannelMapping:(NSArray *)mapping;

@end

@interface NSBundle (NSSound)
- (NSString *)pathForSoundResource:(NSString *)sound;
@end
