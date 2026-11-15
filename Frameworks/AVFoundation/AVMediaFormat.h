/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>

typedef NSString *AVMediaType;

/* The media types (the same codes as CoreMedia's, as strings). */
FOUNDATION_EXPORT AVMediaType const AVMediaTypeVideo;
FOUNDATION_EXPORT AVMediaType const AVMediaTypeAudio;
FOUNDATION_EXPORT AVMediaType const AVMediaTypeText;
FOUNDATION_EXPORT AVMediaType const AVMediaTypeMuxed;
FOUNDATION_EXPORT AVMediaType const AVMediaTypeClosedCaption;
FOUNDATION_EXPORT AVMediaType const AVMediaTypeSubtitle;
FOUNDATION_EXPORT AVMediaType const AVMediaTypeTimecode;
FOUNDATION_EXPORT AVMediaType const AVMediaTypeMetadata;
FOUNDATION_EXPORT AVMediaType const AVMediaTypeDepthData;

/* What a track's media is for. */
typedef NSString *AVMediaCharacteristic;
FOUNDATION_EXPORT AVMediaCharacteristic const AVMediaCharacteristicVisual;
FOUNDATION_EXPORT AVMediaCharacteristic const AVMediaCharacteristicAudible;
FOUNDATION_EXPORT AVMediaCharacteristic const AVMediaCharacteristicLegible;
FOUNDATION_EXPORT AVMediaCharacteristic const AVMediaCharacteristicFrameBased;
FOUNDATION_EXPORT AVMediaCharacteristic const AVMediaCharacteristicUsesWideGamutColorSpace;
FOUNDATION_EXPORT AVMediaCharacteristic const AVMediaCharacteristicContainsAlphaChannel;
FOUNDATION_EXPORT AVMediaCharacteristic const AVMediaCharacteristicIsMainProgramContent;
FOUNDATION_EXPORT AVMediaCharacteristic const AVMediaCharacteristicIsAuxiliaryContent;

/* File types: their uniform type identifiers. */
typedef NSString *AVFileType;
FOUNDATION_EXPORT AVFileType const AVFileTypeQuickTimeMovie;
FOUNDATION_EXPORT AVFileType const AVFileTypeMPEG4;
FOUNDATION_EXPORT AVFileType const AVFileTypeAppleM4V;
FOUNDATION_EXPORT AVFileType const AVFileTypeAppleM4A;
FOUNDATION_EXPORT AVFileType const AVFileType3GPP;
FOUNDATION_EXPORT AVFileType const AVFileType3GPP2;
FOUNDATION_EXPORT AVFileType const AVFileTypeCoreAudioFormat;
FOUNDATION_EXPORT AVFileType const AVFileTypeWAVE;
FOUNDATION_EXPORT AVFileType const AVFileTypeAIFF;
FOUNDATION_EXPORT AVFileType const AVFileTypeAIFC;
FOUNDATION_EXPORT AVFileType const AVFileTypeAMR;
FOUNDATION_EXPORT AVFileType const AVFileTypeMPEGLayer3;
FOUNDATION_EXPORT AVFileType const AVFileTypeSunAU;
FOUNDATION_EXPORT AVFileType const AVFileTypeAC3;
FOUNDATION_EXPORT AVFileType const AVFileTypeEnhancedAC3;
FOUNDATION_EXPORT AVFileType const AVFileTypeJPEG;
FOUNDATION_EXPORT AVFileType const AVFileTypeDNG;
FOUNDATION_EXPORT AVFileType const AVFileTypeHEIC;
FOUNDATION_EXPORT AVFileType const AVFileTypeAVCI;
FOUNDATION_EXPORT AVFileType const AVFileTypeHEIF;
FOUNDATION_EXPORT AVFileType const AVFileTypeTIFF;

typedef NS_ENUM(NSInteger, AVAuthorizationStatus) {
    AVAuthorizationStatusNotDetermined = 0,
    AVAuthorizationStatusRestricted = 1,
    AVAuthorizationStatusDenied = 2,
    AVAuthorizationStatusAuthorized = 3,
};
