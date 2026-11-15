/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Private to the system: playback for code that has no main dispatch
 * queue nor Objective-C (WebKit's media player, AU6). A stream is a file
 * a download is written into as it comes; a playback plays a file or a
 * stream, on AVFoundation's playback engine (its clock, audio, seeking and
 * frames), and says what happens through a callback, on a thread of its
 * own.
 */

#ifndef _AVMINKARPLAYBACK_H_
#define _AVMINKARPLAYBACK_H_

#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CVPixelBuffer.h>
#include <stdbool.h>
#include <stdint.h>

__BEGIN_DECLS

typedef struct AVMinkarStream *AVMinkarStreamRef;
typedef struct AVMinkarPlayback *AVMinkarPlaybackRef;

/* A download's file. expectedLength: -1 when not known. */
AVMinkarStreamRef AVMinkarStreamCreate(int64_t expectedLength);
bool AVMinkarStreamAppend(AVMinkarStreamRef stream, const void *bytes, size_t length);
/* The download is over (complete or not): reads end where it ends. */
void AVMinkarStreamFinish(AVMinkarStreamRef stream);
int64_t AVMinkarStreamGetLength(AVMinkarStreamRef stream);
int64_t AVMinkarStreamGetExpectedLength(AVMinkarStreamRef stream);
bool AVMinkarStreamIsFinished(AVMinkarStreamRef stream);
void AVMinkarStreamRelease(AVMinkarStreamRef stream);

enum {
    kAVMinkarPlaybackReady = 1,		/* tracks, duration and size known */
    kAVMinkarPlaybackFailed = 2,
    kAVMinkarPlaybackEnded = 3,
    kAVMinkarPlaybackSeeked = 4,
};
typedef void (*AVMinkarPlaybackCallback)(void *context, int event);

/* Loads in the background, then calls back Ready or Failed. name: the
   file's name or URL, for its extension. */
AVMinkarPlaybackRef AVMinkarPlaybackCreateWithStream(AVMinkarStreamRef stream, const char *name,
    AVMinkarPlaybackCallback callback, void *context);
AVMinkarPlaybackRef AVMinkarPlaybackCreateWithPath(const char *path, AVMinkarPlaybackCallback callback,
    void *context);
/* No more callbacks once released. */
void AVMinkarPlaybackRelease(AVMinkarPlaybackRef playback);

bool AVMinkarPlaybackHasVideo(AVMinkarPlaybackRef playback);
bool AVMinkarPlaybackHasAudio(AVMinkarPlaybackRef playback);
void AVMinkarPlaybackGetNaturalSize(AVMinkarPlaybackRef playback, double *width, double *height);
double AVMinkarPlaybackGetDuration(AVMinkarPlaybackRef playback);	/* NAN when not known */
double AVMinkarPlaybackGetCurrentTime(AVMinkarPlaybackRef playback);
void AVMinkarPlaybackSetRate(AVMinkarPlaybackRef playback, float rate);
float AVMinkarPlaybackGetRate(AVMinkarPlaybackRef playback);
void AVMinkarPlaybackSetVolume(AVMinkarPlaybackRef playback, float volume);
/* In the background; calls back Seeked. */
void AVMinkarPlaybackSeek(AVMinkarPlaybackRef playback, double seconds);
/* The frame due at the time (BGRA), or NULL; its time in *frameTime. */
CVPixelBufferRef AVMinkarPlaybackCopyFrame(AVMinkarPlaybackRef playback, double time, double *frameTime)
    CF_RETURNS_RETAINED;
/* While playing: says Ended when the end is reached (checked when the
   time is asked, at the page's pace). */
bool AVMinkarPlaybackIsAtEnd(AVMinkarPlaybackRef playback);
CFErrorRef AVMinkarPlaybackCopyError(AVMinkarPlaybackRef playback) CF_RETURNS_RETAINED;

/* A whole sound file in memory, decoded at the rate, mixed to one channel
   or with its own: interleaved float samples (malloc'd; the caller frees
   them), their channels and frames. */
bool AVMinkarDecodeAudio(const void *bytes, size_t length, double sampleRate, bool mixToMono, int *channels,
    size_t *frames, float **samples);

__END_DECLS

#endif
