/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio formats: what is known about a format and about channel layouts,
 * asked by property.
 */

#ifndef __AUDIOFORMAT__
#define __AUDIOFORMAT__

#include <CoreAudio/CoreAudioTypes.h>
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef UInt32 AudioFormatPropertyID;

struct AudioFormatInfo {
    AudioStreamBasicDescription mASBD;
    const void *mMagicCookie;
    UInt32 mMagicCookieSize;
};
typedef struct AudioFormatInfo AudioFormatInfo;

struct AudioFormatListItem {
    AudioStreamBasicDescription mASBD;
    AudioChannelLayoutTag mChannelLayoutTag;
};
typedef struct AudioFormatListItem AudioFormatListItem;

struct AudioPanningInfo {
    UInt32 mPanningMode;
    AudioChannelFlags mCoordinateFlags;
    Float32 mCoordinates[3];
    Float32 mGainScale;
    const AudioChannelLayout *mOutputChannelMap;
};
typedef struct AudioPanningInfo AudioPanningInfo;

enum {
    kAudioFormatProperty_FormatInfo = 'fmti',
    kAudioFormatProperty_FormatName = 'fnam',
    kAudioFormatProperty_EncodeFormatIDs = 'acof',
    kAudioFormatProperty_DecodeFormatIDs = 'acif',
    kAudioFormatProperty_FormatList = 'flst',
    kAudioFormatProperty_ASBDFromESDS = 'essc',
    kAudioFormatProperty_ChannelLayoutFromESDS = 'cfes',
    kAudioFormatProperty_OutputFormatList = 'ofls',
    kAudioFormatProperty_FirstPlayableFormatFromList = 'fpfl',
    kAudioFormatProperty_FormatIsVBR = 'fvbr',
    kAudioFormatProperty_FormatIsExternallyFramed = 'fexf',
    kAudioFormatProperty_FormatEmploysDependentPackets = 'fdep',
    kAudioFormatProperty_FormatIsEncrypted = 'cryp',
    kAudioFormatProperty_Encoders = 'aven',
    kAudioFormatProperty_Decoders = 'avde',
    kAudioFormatProperty_AvailableEncodeBitRates = 'aebr',
    kAudioFormatProperty_AvailableEncodeSampleRates = 'aesr',
    kAudioFormatProperty_AvailableEncodeChannelLayoutTags = 'aecl',
    kAudioFormatProperty_AvailableEncodeNumberChannels = 'avnc',
    kAudioFormatProperty_AvailableDecodeNumberChannels = 'adnc',
    kAudioFormatProperty_ASBDFromMPEGPacket = 'admp',
    kAudioFormatProperty_BitmapForLayoutTag = 'bmtg',
    kAudioFormatProperty_MatrixMixMap = 'mmap',
    kAudioFormatProperty_ChannelMap = 'chmp',
    kAudioFormatProperty_NumberOfChannelsForLayout = 'nchm',
    kAudioFormatProperty_AreChannelLayoutsEquivalent = 'cheq',
    kAudioFormatProperty_ChannelLayoutHash = 'chha',
    kAudioFormatProperty_ValidateChannelLayout = 'vacl',
    kAudioFormatProperty_ChannelLayoutForTag = 'cmpl',
    kAudioFormatProperty_TagForChannelLayout = 'cmpt',
    kAudioFormatProperty_ChannelLayoutName = 'lonm',
    kAudioFormatProperty_ChannelLayoutSimpleName = 'lsnm',
    kAudioFormatProperty_ChannelLayoutForBitmap = 'cmpb',
    kAudioFormatProperty_ChannelName = 'cnam',
    kAudioFormatProperty_ChannelShortName = 'csnm',
    kAudioFormatProperty_TagsForNumberOfChannels = 'tagc',
    kAudioFormatProperty_PanningMatrix = 'panm',
    kAudioFormatProperty_BalanceFade = 'balf',
    kAudioFormatProperty_ID3TagSize = 'id3s',
    kAudioFormatProperty_ID3TagToDictionary = 'id3d',
};

enum {
    kAudioFormatUnspecifiedError = 'what',
    kAudioFormatUnsupportedPropertyError = 'prop',
    kAudioFormatBadPropertySizeError = '!siz',
    kAudioFormatBadSpecifierSizeError = '!spc',
    kAudioFormatUnsupportedDataFormatError = 'fmt?',
    kAudioFormatUnknownFormatError = '!fmt',
};

OSStatus AudioFormatGetPropertyInfo(AudioFormatPropertyID inPropertyID, UInt32 inSpecifierSize,
    const void *inSpecifier, UInt32 *outPropertyDataSize);
OSStatus AudioFormatGetProperty(AudioFormatPropertyID inPropertyID, UInt32 inSpecifierSize,
    const void *inSpecifier, UInt32 *ioPropertyDataSize, void *outPropertyData);

#ifdef __cplusplus
}
#endif

#endif
