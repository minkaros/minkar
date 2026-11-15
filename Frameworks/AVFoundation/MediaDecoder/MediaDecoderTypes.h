/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The media decoder (AU5): /usr/libexec/mediadecoder, started by
 * AVFoundation for each file it reads, with a port of the app's as its
 * bootstrap port: it sends its own service port there (an MDHello, with a
 * reply port), the app answers with its own bootstrap port (an MDWelcome)
 * for the decoder to use, and the decoder serves MediaDecoder.defs until
 * the app lets go of the service port. A decoder that crashes takes
 * nothing else with it.
 */

#ifndef _MEDIADECODERTYPES_H_
#define _MEDIADECODERTYPES_H_

#include <mach/message.h>
#include <stdint.h>

typedef char *md_bytes_t;
typedef char *md_data_t;

#define kMDDecoderPath		"/usr/libexec/mediadecoder"
#define kMDHelloID		29790
#define kMDWelcomeID		29791

/* From the decoder: its service port; the reply port is the header's. */
typedef struct {
	mach_msg_header_t header;
	mach_msg_body_t body;
	mach_msg_port_descriptor_t service;
} MDHello;

/* From the app: the bootstrap port the decoder should use. */
typedef struct {
	mach_msg_header_t header;
	mach_msg_body_t body;
	mach_msg_port_descriptor_t bootstrap;
} MDWelcome;

enum {
	kMDSuccess = 0,
	kMDErrorNoService = -1,
	kMDErrorCannotOpen = -2,
	kMDErrorNoSuchTrack = -3,
	kMDErrorDecode = -4,
	kMDErrorBadOptions = -5,
	kMDErrorNoBuffers = -6,		/* every surface still in the app's use */
	kMDErrorNotOpen = -7,
	kMDErrorStalled = -8,		/* a download gave nothing for a minute */
};

enum {
	kMDFlagEndOfTrack = 1 << 0,
	kMDFlagKeyFrame = 1 << 1,
};

/* The file's description. */
#define kMDKeyFormat		"format"	/* the container, FFmpeg's name */
#define kMDKeyDuration		"duration"	/* seconds */
#define kMDKeyTracks		"tracks"	/* an array of dictionaries: */
#define kMDKeyTrackID		"trackID"	/* 1, 2, ... */
#define kMDKeyMediaType		"mediaType"	/* "vide", "soun" */
#define kMDKeyCodec		"codec"		/* FFmpeg's codec name */
#define kMDKeyTimescale		"timescale"
#define kMDKeyTrackDuration	"trackDuration"	/* seconds */
#define kMDKeyWidth		"width"
#define kMDKeyHeight		"height"
#define kMDKeyFrameRate		"frameRate"	/* nominal, frames a second */
#define kMDKeySampleRate	"sampleRate"
#define kMDKeyChannels		"channels"
#define kMDKeyBitRate		"bitRate"
#define kMDKeyLanguage		"language"
#define kMDKeyMetadata		"metadata"	/* the file's tags, or a track's */
#define kMDKeyColorPrimaries	"colorPrimaries"	/* FFmpeg's names */
#define kMDKeyTransfer		"transfer"
#define kMDKeyMatrix		"matrix"
#define kMDKeyFullRange		"fullRange"
#define kMDKeyRotation		"rotation"	/* degrees, from the display matrix */
#define kMDKeySampleAspect	"sampleAspect"	/* [num, den] */

/* A track's output. */
#define kMDKeyOutputPixelFormat	"pixelFormat"	/* 'BGRA' or '420v' */
#define kMDKeyOutputWidth	"width"
#define kMDKeyOutputHeight	"height"
#define kMDKeyOutputSampleRate	"sampleRate"
#define kMDKeyOutputChannels	"channels"
#define kMDKeyOutputFloat	"float"		/* true: 32-bit float, else 16-bit */

#endif
