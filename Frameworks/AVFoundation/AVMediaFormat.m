/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AVFoundation/AVMediaFormat.h>

AVMediaType const AVMediaTypeVideo = @"vide";
AVMediaType const AVMediaTypeAudio = @"soun";
AVMediaType const AVMediaTypeText = @"text";
AVMediaType const AVMediaTypeMuxed = @"muxx";
AVMediaType const AVMediaTypeClosedCaption = @"clcp";
AVMediaType const AVMediaTypeSubtitle = @"sbtl";
AVMediaType const AVMediaTypeTimecode = @"tmcd";
AVMediaType const AVMediaTypeMetadata = @"meta";
AVMediaType const AVMediaTypeDepthData = @"dpth";

AVMediaCharacteristic const AVMediaCharacteristicVisual = @"AVMediaCharacteristicVisual";
AVMediaCharacteristic const AVMediaCharacteristicAudible = @"AVMediaCharacteristicAudible";
AVMediaCharacteristic const AVMediaCharacteristicLegible = @"AVMediaCharacteristicLegible";
AVMediaCharacteristic const AVMediaCharacteristicFrameBased = @"AVMediaCharacteristicFrameBased";
AVMediaCharacteristic const AVMediaCharacteristicUsesWideGamutColorSpace =
    @"AVMediaCharacteristicUsesWideGamutColorSpace";
AVMediaCharacteristic const AVMediaCharacteristicContainsAlphaChannel = @"AVMediaCharacteristicContainsAlphaChannel";
AVMediaCharacteristic const AVMediaCharacteristicIsMainProgramContent = @"public.main-program-content";
AVMediaCharacteristic const AVMediaCharacteristicIsAuxiliaryContent = @"public.auxiliary-content";

AVFileType const AVFileTypeQuickTimeMovie = @"com.apple.quicktime-movie";
AVFileType const AVFileTypeMPEG4 = @"public.mpeg-4";
AVFileType const AVFileTypeAppleM4V = @"com.apple.m4v-video";
AVFileType const AVFileTypeAppleM4A = @"com.apple.m4a-audio";
AVFileType const AVFileType3GPP = @"public.3gpp";
AVFileType const AVFileType3GPP2 = @"public.3gpp2";
AVFileType const AVFileTypeCoreAudioFormat = @"com.apple.coreaudio-format";
AVFileType const AVFileTypeWAVE = @"com.microsoft.waveform-audio";
AVFileType const AVFileTypeAIFF = @"public.aiff-audio";
AVFileType const AVFileTypeAIFC = @"public.aifc-audio";
AVFileType const AVFileTypeAMR = @"org.3gpp.adaptive-multi-rate-audio";
AVFileType const AVFileTypeMPEGLayer3 = @"public.mp3";
AVFileType const AVFileTypeSunAU = @"public.au-audio";
AVFileType const AVFileTypeAC3 = @"public.ac3-audio";
AVFileType const AVFileTypeEnhancedAC3 = @"public.enhanced-ac3-audio";
AVFileType const AVFileTypeJPEG = @"public.jpeg";
AVFileType const AVFileTypeDNG = @"com.adobe.raw-image";
AVFileType const AVFileTypeHEIC = @"public.heic";
AVFileType const AVFileTypeAVCI = @"public.avci";
AVFileType const AVFileTypeHEIF = @"public.heif";
AVFileType const AVFileTypeTIFF = @"public.tiff";
