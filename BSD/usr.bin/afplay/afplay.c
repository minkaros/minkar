/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * afplay: plays a sound file. The file is read through ExtAudioFile as
 * float at its own rate and channels, and played by an output audio
 * queue (which converts to the device's rate and channels).
 */

#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define kBuffers        3
#define kBufferSeconds  0.25

static ExtAudioFileRef file;
static AudioStreamBasicDescription client;
static UInt32 bufferFrames;
static SInt64 limitFrames = -1, playedFrames;
static float gain = 1;
static Boolean ended, stopped, started;
static volatile sig_atomic_t interrupted;

static void usage(void) {
    fprintf(stderr,
            "usage: afplay [-v volume] [-t time] [-r rate] [-q quality] [-d] [-h] audio_file\n"
            "  -v volume   playback volume (1 is the file's own level; above 1 amplifies)\n"
            "  -t time     play only this many seconds\n"
            "  -r rate     playback rate (only 1 for now)\n"
            "  -q quality  rate conversion quality, 0 (low) or 1 (high)\n"
            "  -d          show the file's format and length\n"
            "  -h          show this help\n");
}

static void fill(AudioQueueRef queue, AudioQueueBufferRef buffer) {
    struct { UInt32 count; AudioBuffer b; } list;
    UInt32 frames = bufferFrames;
    float *samples = buffer->mAudioData;

    if (ended)
        return;
    if (limitFrames >= 0 && playedFrames + frames > limitFrames)
        frames = (UInt32)(limitFrames - playedFrames);
    list.count = 1;
    list.b.mNumberChannels = client.mChannelsPerFrame;
    list.b.mDataByteSize = frames * client.mBytesPerFrame;
    list.b.mData = samples;
    if (frames == 0 || ExtAudioFileRead(file, &frames, (AudioBufferList *)&list) != noErr || frames == 0) {
        ended = true;
        if (started)
            AudioQueueStop(queue, false);   /* what is queued plays out */
        return;
    }
    if (gain != 1)
        for (UInt32 i = 0; i < frames * client.mChannelsPerFrame; i++)
            samples[i] *= gain;
    playedFrames += frames;
    buffer->mAudioDataByteSize = frames * client.mBytesPerFrame;
    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

static void output(void *data, AudioQueueRef queue, AudioQueueBufferRef buffer) {
    if (interrupted) {
        if (!ended) {
            ended = true;
            AudioQueueStop(queue, true);
        }
        return;
    }
    fill(queue, buffer);
}

static void running(void *data, AudioQueueRef queue, AudioQueuePropertyID id) {
    UInt32 v = 1, size = sizeof(v);

    AudioQueueGetProperty(queue, kAudioQueueProperty_IsRunning, &v, &size);
    if (!v) {
        stopped = true;
        CFRunLoopStop(CFRunLoopGetCurrent());
    }
}

static void onSignal(int sig) {
    interrupted = 1;
}

static const char *typeName(UInt32 type) {
    static char buf[8];
    buf[0] = (char)(type >> 24);
    buf[1] = (char)(type >> 16);
    buf[2] = (char)(type >> 8);
    buf[3] = (char)type;
    buf[4] = 0;
    return buf;
}

int main(int argc, char **argv) {
    AudioStreamBasicDescription f;
    AudioQueueRef queue;
    AudioQueueBufferRef buffers[kBuffers];
    CFURLRef url;
    AudioFileID audioFile = NULL;
    UInt32 size, type = 0;
    SInt64 length = 0;
    double seconds = -1, rate = 1, volume = 1;
    int quality = 1, debug = 0, ch;
    OSStatus status;

    while ((ch = getopt(argc, argv, "v:t:r:q:dh")) != -1) {
        switch (ch) {
        case 'v':
            volume = atof(optarg);
            break;
        case 't':
            seconds = atof(optarg);
            break;
        case 'r':
            rate = atof(optarg);
            break;
        case 'q':
            quality = atoi(optarg);
            break;
        case 'd':
            debug = 1;
            break;
        case 'h':
            usage();
            return 0;
        default:
            usage();
            return 1;
        }
    }
    argc -= optind;
    argv += optind;
    if (argc != 1) {
        usage();
        return 1;
    }
    if (rate != 1) {
        fprintf(stderr, "afplay: a rate other than 1 is not supported yet\n");
        return 1;
    }
    if (volume < 0) {
        fprintf(stderr, "afplay: the volume cannot be negative\n");
        return 1;
    }

    url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)argv[0], (CFIndex)strlen(argv[0]), false);
    status = ExtAudioFileOpenURL(url, &file);
    CFRelease(url);
    if (status != noErr) {
        fprintf(stderr, "afplay: %s: cannot open the file (%d)\n", argv[0], (int)status);
        return 1;
    }
    size = sizeof(f);
    ExtAudioFileGetProperty(file, kExtAudioFileProperty_FileDataFormat, &size, &f);
    size = sizeof(length);
    ExtAudioFileGetProperty(file, kExtAudioFileProperty_FileLengthFrames, &size, &length);
    size = sizeof(audioFile);
    ExtAudioFileGetProperty(file, kExtAudioFileProperty_AudioFile, &size, &audioFile);
    size = sizeof(type);
    if (audioFile != NULL)
        AudioFileGetProperty(audioFile, kAudioFilePropertyFileFormat, &size, &type);

    if (debug) {
        CFStringRef name = NULL;
        char buf[160] = "";
        size = sizeof(name);
        AudioFormatGetProperty(kAudioFormatProperty_FormatName, sizeof(f), &f, &size, &name);
        if (name != NULL) {
            CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8);
            CFRelease(name);
        }
        printf("Playing file: %s\n", argv[0]);
        printf("File type: %s\n", typeName(type));
        printf("Data format: %s, %u channels, %.0f Hz\n", buf, (unsigned)f.mChannelsPerFrame, f.mSampleRate);
        printf("Length: %lld frames, %.3f seconds\n", (long long)length, f.mSampleRate > 0 ? length / f.mSampleRate : 0);
    }

    memset(&client, 0, sizeof(client));
    client.mSampleRate = f.mSampleRate;
    client.mFormatID = kAudioFormatLinearPCM;
    client.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    client.mChannelsPerFrame = f.mChannelsPerFrame;
    client.mBitsPerChannel = 32;
    client.mFramesPerPacket = 1;
    client.mBytesPerFrame = client.mBytesPerPacket = 4 * f.mChannelsPerFrame;
    status = ExtAudioFileSetProperty(file, kExtAudioFileProperty_ClientDataFormat, sizeof(client), &client);
    if (status != noErr) {
        fprintf(stderr, "afplay: %s: its format cannot be played (%d)\n", argv[0], (int)status);
        return 1;
    }
    if (seconds >= 0)
        limitFrames = (SInt64)(seconds * f.mSampleRate);
    bufferFrames = (UInt32)(f.mSampleRate * kBufferSeconds);
    /* The queue's volume goes up to 1; louder is applied to the samples. */
    gain = volume > 1 ? (float)volume : 1;

    status = AudioQueueNewOutput(&client, output, NULL, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode, 0, &queue);
    if (status != noErr) {
        fprintf(stderr, "afplay: no audio output (%d)\n", (int)status);
        return 1;
    }
    AudioQueueSetParameter(queue, kAudioQueueParam_Volume, volume < 1 ? (float)volume : 1);
    if (quality == 0) {
        AudioConverterRef converter = NULL;
        UInt32 q = kAudioConverterQuality_Low;
        size = sizeof(converter);
        if (ExtAudioFileGetProperty(file, kExtAudioFileProperty_AudioConverter, &size, &converter) == noErr &&
            converter != NULL)
            AudioConverterSetProperty(converter, kAudioConverterSampleRateConverterQuality, sizeof(q), &q);
    }
    AudioQueueAddPropertyListener(queue, kAudioQueueProperty_IsRunning, running, NULL);
    for (int i = 0; i < kBuffers; i++) {
        if (AudioQueueAllocateBuffer(queue, bufferFrames * client.mBytesPerFrame, &buffers[i]) != noErr)
            break;
        fill(queue, buffers[i]);
    }
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);
    if (playedFrames == 0) {
        fprintf(stderr, "afplay: %s: there is nothing to play\n", argv[0]);
        return 1;
    }
    status = AudioQueueStart(queue, NULL);
    if (status != noErr) {
        fprintf(stderr, "afplay: the queue cannot start (%d)\n", (int)status);
        return 1;
    }
    started = true;
    if (ended)      /* all of it is already queued */
        AudioQueueStop(queue, false);
    while (!stopped) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.25, false);
        if (interrupted && !stopped) {
            AudioQueueStop(queue, true);
            break;
        }
    }
    AudioQueueDispose(queue, true);
    ExtAudioFileDispose(file);
    return interrupted ? 130 : 0;
}
