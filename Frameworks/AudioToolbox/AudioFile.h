/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio files: reading and writing the samples of a sound file and its
 * properties, by URL or through an app's callbacks.
 */

#ifndef __AUDIOFILE__
#define __AUDIOFILE__

#include <CoreAudio/CoreAudioTypes.h>
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef UInt32 AudioFileTypeID;
typedef struct OpaqueAudioFileID *AudioFileID;
typedef UInt32 AudioFilePropertyID;

enum {
    kAudioFileAIFFType = 'AIFF',
    kAudioFileAIFCType = 'AIFC',
    kAudioFileWAVEType = 'WAVE',
    kAudioFileRF64Type = 'RF64',
    kAudioFileBW64Type = 'BW64',
    kAudioFileWave64Type = 'W64f',
    kAudioFileSoundDesigner2Type = 'Sd2f',
    kAudioFileNextType = 'NeXT',
    kAudioFileMP3Type = 'MPG3',
    kAudioFileMP2Type = 'MPG2',
    kAudioFileMP1Type = 'MPG1',
    kAudioFileAC3Type = 'ac-3',
    kAudioFileAAC_ADTSType = 'adts',
    kAudioFileMPEG4Type = 'mp4f',
    kAudioFileM4AType = 'm4af',
    kAudioFileM4BType = 'm4bf',
    kAudioFileCAFType = 'caff',
    kAudioFile3GPType = '3gpp',
    kAudioFile3GP2Type = '3gp2',
    kAudioFileAMRType = 'amrf',
    kAudioFileFLACType = 'flac',
    kAudioFileLATMInLOASType = 'loas',
};

enum {
    kAudioFileUnspecifiedError = 'wht?',
    kAudioFileUnsupportedFileTypeError = 'typ?',
    kAudioFileUnsupportedDataFormatError = 'fmt?',
    kAudioFileUnsupportedPropertyError = 'pty?',
    kAudioFileBadPropertySizeError = '!siz',
    kAudioFilePermissionsError = 'prm?',
    kAudioFileNotOptimizedError = 'optm',
    kAudioFileInvalidChunkError = 'chk?',
    kAudioFileDoesNotAllow64BitDataSizeError = 'off?',
    kAudioFileInvalidPacketOffsetError = 'pck?',
    kAudioFileInvalidPacketDependencyError = 'dep?',
    kAudioFileInvalidFileError = 'dta?',
    kAudioFileOperationNotSupportedError = 0x6F703F3F,  /* 'op??' */
    kAudioFileNotOpenError = -38,
    kAudioFileEndOfFileError = -39,
    kAudioFilePositionError = -40,
    kAudioFileFileNotFoundError = -43,
};

typedef SInt8 AudioFilePermissions;
enum {
    kAudioFileReadPermission = 0x01,
    kAudioFileWritePermission = 0x02,
    kAudioFileReadWritePermission = 0x03,
};

typedef UInt32 AudioFileFlags;
enum {
    kAudioFileFlags_EraseFile = 1,
    kAudioFileFlags_DontPageAlignAudioData = 2,
};

enum {
    kAudioFilePropertyFileFormat = 'ffmt',
    kAudioFilePropertyDataFormat = 'dfmt',
    kAudioFilePropertyIsOptimized = 'optm',
    kAudioFilePropertyMagicCookieData = 'mgic',
    kAudioFilePropertyAudioDataByteCount = 'bcnt',
    kAudioFilePropertyAudioDataPacketCount = 'pcnt',
    kAudioFilePropertyMaximumPacketSize = 'psze',
    kAudioFilePropertyDataOffset = 'doff',
    kAudioFilePropertyChannelLayout = 'cmap',
    kAudioFilePropertyDeferSizeUpdates = 'dszu',
    kAudioFilePropertyDataFormatName = 'fnme',
    kAudioFilePropertyMarkerList = 'mkls',
    kAudioFilePropertyRegionList = 'rgls',
    kAudioFilePropertyPacketToFrame = 'pkfr',
    kAudioFilePropertyFrameToPacket = 'frpk',
    kAudioFilePropertyRestrictsRandomAccess = 'rrap',
    kAudioFilePropertyPacketToRollDistance = 'pkrl',
    kAudioFilePropertyPreviousIndependentPacket = 'pind',
    kAudioFilePropertyNextIndependentPacket = 'nind',
    kAudioFilePropertyPacketToDependencyInfo = 'pkdp',
    kAudioFilePropertyPacketToByte = 'pkby',
    kAudioFilePropertyByteToPacket = 'bypk',
    kAudioFilePropertyChunkIDs = 'chid',
    kAudioFilePropertyInfoDictionary = 'info',
    kAudioFilePropertyPacketTableInfo = 'pnfo',
    kAudioFilePropertyFormatList = 'flst',
    kAudioFilePropertyPacketSizeUpperBound = 'pkub',
    kAudioFilePropertyPacketRangeByteCountUpperBound = 'prub',
    kAudioFilePropertyReserveDuration = 'rsrv',
    kAudioFilePropertyEstimatedDuration = 'edur',
    kAudioFilePropertyBitRate = 'brat',
    kAudioFilePropertyID3Tag = 'id3t',
    kAudioFilePropertyID3TagOffset = 'id3o',
    kAudioFilePropertySourceBitDepth = 'sbtd',
    kAudioFilePropertyAlbumArtwork = 'aart',
    kAudioFilePropertyAudioTrackCount = 'atct',
    kAudioFilePropertyUseAudioTrack = 'uatk',
};

enum {
    kAudioFileGlobalInfo_ReadableTypes = 'afrf',
    kAudioFileGlobalInfo_WritableTypes = 'afwf',
    kAudioFileGlobalInfo_FileTypeName = 'ftnm',
    kAudioFileGlobalInfo_AvailableStreamDescriptionsForFormat = 'sdid',
    kAudioFileGlobalInfo_AvailableFormatIDs = 'fmid',
    kAudioFileGlobalInfo_AllExtensions = 'alxt',
    kAudioFileGlobalInfo_AllHFSTypeCodes = 'ahfs',
    kAudioFileGlobalInfo_AllUTIs = 'auti',
    kAudioFileGlobalInfo_AllMIMETypes = 'amim',
    kAudioFileGlobalInfo_ExtensionsForType = 'fext',
    kAudioFileGlobalInfo_HFSTypeCodesForType = 'fhfs',
    kAudioFileGlobalInfo_UTIsForType = 'futi',
    kAudioFileGlobalInfo_MIMETypesForType = 'fmim',
    kAudioFileGlobalInfo_TypesForMIMEType = 'tmim',
    kAudioFileGlobalInfo_TypesForUTI = 'tuti',
    kAudioFileGlobalInfo_TypesForHFSTypeCode = 'thfs',
    kAudioFileGlobalInfo_TypesForExtension = 'tyex',
};

struct AudioFilePacketTableInfo {
    SInt64 mNumberValidFrames;
    SInt32 mPrimingFrames;
    SInt32 mRemainderFrames;
};
typedef struct AudioFilePacketTableInfo AudioFilePacketTableInfo;

struct AudioFramePacketTranslation {
    SInt64 mFrame;
    SInt64 mPacket;
    UInt32 mFrameOffsetInPacket;
};
typedef struct AudioFramePacketTranslation AudioFramePacketTranslation;

struct AudioBytePacketTranslation {
    SInt64 mByte;
    SInt64 mPacket;
    UInt32 mByteOffsetInPacket;
    UInt32 mFlags;
};
typedef struct AudioBytePacketTranslation AudioBytePacketTranslation;

struct AudioFileTypeAndFormatID {
    AudioFileTypeID mFileType;
    UInt32 mFormatID;
};
typedef struct AudioFileTypeAndFormatID AudioFileTypeAndFormatID;

typedef OSStatus (*AudioFile_ReadProc)(void *inClientData, SInt64 inPosition, UInt32 requestCount, void *buffer,
    UInt32 *actualCount);
typedef OSStatus (*AudioFile_WriteProc)(void *inClientData, SInt64 inPosition, UInt32 requestCount,
    const void *buffer, UInt32 *actualCount);
typedef SInt64 (*AudioFile_GetSizeProc)(void *inClientData);
typedef OSStatus (*AudioFile_SetSizeProc)(void *inClientData, SInt64 inSize);

OSStatus AudioFileCreateWithURL(CFURLRef inFileRef, AudioFileTypeID inFileType,
    const AudioStreamBasicDescription *inFormat, AudioFileFlags inFlags, AudioFileID *outAudioFile);
OSStatus AudioFileOpenURL(CFURLRef inFileRef, AudioFilePermissions inPermissions, AudioFileTypeID inFileTypeHint,
    AudioFileID *outAudioFile);
OSStatus AudioFileInitializeWithCallbacks(void *inClientData, AudioFile_ReadProc inReadFunc,
    AudioFile_WriteProc inWriteFunc, AudioFile_GetSizeProc inGetSizeFunc, AudioFile_SetSizeProc inSetSizeFunc,
    AudioFileTypeID inFileType, const AudioStreamBasicDescription *inFormat, AudioFileFlags inFlags,
    AudioFileID *outAudioFile);
OSStatus AudioFileOpenWithCallbacks(void *inClientData, AudioFile_ReadProc inReadFunc,
    AudioFile_WriteProc inWriteFunc, AudioFile_GetSizeProc inGetSizeFunc, AudioFile_SetSizeProc inSetSizeFunc,
    AudioFileTypeID inFileTypeHint, AudioFileID *outAudioFile);
OSStatus AudioFileClose(AudioFileID inAudioFile);
OSStatus AudioFileOptimize(AudioFileID inAudioFile);
OSStatus AudioFileReadBytes(AudioFileID inAudioFile, Boolean inUseCache, SInt64 inStartingByte,
    UInt32 *ioNumBytes, void *outBuffer);
OSStatus AudioFileWriteBytes(AudioFileID inAudioFile, Boolean inUseCache, SInt64 inStartingByte,
    UInt32 *ioNumBytes, const void *inBuffer);
OSStatus AudioFileReadPacketData(AudioFileID inAudioFile, Boolean inUseCache, UInt32 *ioNumBytes,
    AudioStreamPacketDescription *outPacketDescriptions, SInt64 inStartingPacket, UInt32 *ioNumPackets,
    void *outBuffer);
OSStatus AudioFileReadPackets(AudioFileID inAudioFile, Boolean inUseCache, UInt32 *outNumBytes,
    AudioStreamPacketDescription *outPacketDescriptions, SInt64 inStartingPacket, UInt32 *ioNumPackets,
    void *outBuffer);
OSStatus AudioFileWritePackets(AudioFileID inAudioFile, Boolean inUseCache, UInt32 inNumBytes,
    const AudioStreamPacketDescription *inPacketDescriptions, SInt64 inStartingPacket, UInt32 *ioNumPackets,
    const void *inBuffer);
OSStatus AudioFileCountUserData(AudioFileID inAudioFile, UInt32 inUserDataID, UInt32 *outNumberItems);
OSStatus AudioFileGetUserDataSize(AudioFileID inAudioFile, UInt32 inUserDataID, UInt32 inIndex,
    UInt32 *outUserDataSize);
OSStatus AudioFileGetUserData(AudioFileID inAudioFile, UInt32 inUserDataID, UInt32 inIndex, UInt32 *ioUserDataSize,
    void *outUserData);
OSStatus AudioFileSetUserData(AudioFileID inAudioFile, UInt32 inUserDataID, UInt32 inIndex, UInt32 inUserDataSize,
    const void *inUserData);
OSStatus AudioFileRemoveUserData(AudioFileID inAudioFile, UInt32 inUserDataID, UInt32 inIndex);
OSStatus AudioFileGetPropertyInfo(AudioFileID inAudioFile, AudioFilePropertyID inPropertyID, UInt32 *outDataSize,
    UInt32 *isWritable);
OSStatus AudioFileGetProperty(AudioFileID inAudioFile, AudioFilePropertyID inPropertyID, UInt32 *ioDataSize,
    void *outPropertyData);
OSStatus AudioFileSetProperty(AudioFileID inAudioFile, AudioFilePropertyID inPropertyID, UInt32 inDataSize,
    const void *inPropertyData);
OSStatus AudioFileGetGlobalInfoSize(AudioFilePropertyID inPropertyID, UInt32 inSpecifierSize, void *inSpecifier,
    UInt32 *outDataSize);
OSStatus AudioFileGetGlobalInfo(AudioFilePropertyID inPropertyID, UInt32 inSpecifierSize, void *inSpecifier,
    UInt32 *ioDataSize, void *outPropertyData);

#ifdef __cplusplus
}
#endif

#endif
