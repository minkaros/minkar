/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * AVFoundation's side of the media decoder (AU5): a session is one
 * mediadecoder process reading one file, its samples given back as
 * CMSampleBuffers (video frames as IOSurface-backed pixel buffers, audio
 * as linear PCM). Private to the system's frameworks.
 */

#ifndef _MDCLIENT_H_
#define _MDCLIENT_H_

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CMSampleBuffer.h>
#include <sys/types.h>

__BEGIN_DECLS

typedef struct MDSession *MDSessionRef;

/* The file (read through its descriptor, which the session dups) and its
   description (MediaDecoderTypes.h's kMDKey...), or NULL and the error. */
MDSessionRef _MDSessionCreate(int fd, const char *name, int32_t *error);
/* A file still being downloaded into: reads past its end wait for more
   until doneFd (the read end of a pipe whose write end the downloader
   closes when it is over) says so, or the expected length (-1: not
   known) is reached. */
MDSessionRef _MDSessionCreateStreaming(int fd, int doneFd, const char *name, int64_t expectedLength,
    int32_t *error);
CFDictionaryRef _MDSessionGetInfo(MDSessionRef session);
pid_t _MDSessionGetProcessID(MDSessionRef session);

/* A track's output: kMDKeyOutput... options. */
int32_t _MDSessionConfigure(MDSessionRef session, int32_t trackID, CFDictionaryRef options);
int32_t _MDSessionSeek(MDSessionRef session, CMTime time);

/* The track's next sample, or NULL with *ended at its end, or NULL and
   the error; kMDErrorNoService once the decoder is gone. */
int32_t _MDSessionCopyNextSample(MDSessionRef session, int32_t trackID, CMSampleBufferRef *sample, Boolean *ended);

/* The decoder ends when its session does. */
void _MDSessionRelease(MDSessionRef session);

__END_DECLS

#endif
