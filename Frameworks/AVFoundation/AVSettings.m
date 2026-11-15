/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The audio and video settings' keys: each its own name.
 */

#import <AVFoundation/AVAudioSettings.h>
#import <AVFoundation/AVVideoSettings.h>

#define KEY(name) NSString *const name = @#name;

KEY(AVFormatIDKey)
KEY(AVSampleRateKey)
KEY(AVNumberOfChannelsKey)
KEY(AVLinearPCMBitDepthKey)
KEY(AVLinearPCMIsBigEndianKey)
KEY(AVLinearPCMIsFloatKey)
KEY(AVLinearPCMIsNonInterleaved)
KEY(AVAudioFileTypeKey)
KEY(AVEncoderAudioQualityKey)
KEY(AVEncoderAudioQualityForVBRKey)
KEY(AVEncoderBitRateKey)
KEY(AVEncoderBitRatePerChannelKey)
KEY(AVEncoderBitRateStrategyKey)
KEY(AVEncoderBitDepthHintKey)
KEY(AVSampleRateConverterAlgorithmKey)
KEY(AVSampleRateConverterAudioQualityKey)
KEY(AVChannelLayoutKey)
KEY(AVAudioBitRateStrategy_Constant)
KEY(AVAudioBitRateStrategy_LongTermAverage)
KEY(AVAudioBitRateStrategy_VariableConstrained)
KEY(AVAudioBitRateStrategy_Variable)
KEY(AVSampleRateConverterAlgorithm_Normal)
KEY(AVSampleRateConverterAlgorithm_Mastering)
KEY(AVSampleRateConverterAlgorithm_MinimumPhase)

AVVideoCodecType const AVVideoCodecTypeHEVC = @"hvc1";
AVVideoCodecType const AVVideoCodecTypeH264 = @"avc1";
AVVideoCodecType const AVVideoCodecTypeJPEG = @"jpeg";
AVVideoCodecType const AVVideoCodecTypeAppleProRes4444 = @"ap4h";
AVVideoCodecType const AVVideoCodecTypeAppleProRes422 = @"apcn";
AVVideoCodecType const AVVideoCodecTypeAppleProRes422HQ = @"apch";
AVVideoCodecType const AVVideoCodecTypeAppleProRes422LT = @"apcs";
AVVideoCodecType const AVVideoCodecTypeAppleProRes422Proxy = @"apco";
AVVideoCodecType const AVVideoCodecTypeHEVCWithAlpha = @"muxa";

KEY(AVVideoCodecKey)
KEY(AVVideoWidthKey)
KEY(AVVideoHeightKey)
KEY(AVVideoPixelAspectRatioKey)
KEY(AVVideoPixelAspectRatioHorizontalSpacingKey)
KEY(AVVideoPixelAspectRatioVerticalSpacingKey)
KEY(AVVideoCleanApertureKey)
KEY(AVVideoCleanApertureWidthKey)
KEY(AVVideoCleanApertureHeightKey)
KEY(AVVideoCleanApertureHorizontalOffsetKey)
KEY(AVVideoCleanApertureVerticalOffsetKey)
KEY(AVVideoScalingModeKey)
KEY(AVVideoScalingModeFit)
KEY(AVVideoScalingModeResize)
KEY(AVVideoScalingModeResizeAspect)
KEY(AVVideoScalingModeResizeAspectFill)
KEY(AVVideoColorPropertiesKey)
KEY(AVVideoColorPrimariesKey)
KEY(AVVideoTransferFunctionKey)
KEY(AVVideoYCbCrMatrixKey)
KEY(AVVideoAllowWideColorKey)
KEY(AVVideoCompressionPropertiesKey)
KEY(AVVideoAverageBitRateKey)
KEY(AVVideoQualityKey)
KEY(AVVideoMaxKeyFrameIntervalKey)
KEY(AVVideoMaxKeyFrameIntervalDurationKey)
KEY(AVVideoAllowFrameReorderingKey)
KEY(AVVideoExpectedSourceFrameRateKey)
KEY(AVVideoAverageNonDroppableFrameRateKey)
KEY(AVVideoH264EntropyModeKey)
KEY(AVVideoEncoderSpecificationKey)
KEY(AVVideoProfileLevelKey)
NSString *const AVVideoH264EntropyModeCAVLC = @"CAVLC";
NSString *const AVVideoH264EntropyModeCABAC = @"CABAC";
NSString *const AVVideoProfileLevelH264Baseline30 = @"H264_Baseline_3_0";
NSString *const AVVideoProfileLevelH264Baseline31 = @"H264_Baseline_3_1";
NSString *const AVVideoProfileLevelH264Baseline41 = @"H264_Baseline_4_1";
NSString *const AVVideoProfileLevelH264BaselineAutoLevel = @"H264_Baseline_AutoLevel";
NSString *const AVVideoProfileLevelH264Main30 = @"H264_Main_3_0";
NSString *const AVVideoProfileLevelH264Main31 = @"H264_Main_3_1";
NSString *const AVVideoProfileLevelH264Main32 = @"H264_Main_3_2";
NSString *const AVVideoProfileLevelH264Main41 = @"H264_Main_4_1";
NSString *const AVVideoProfileLevelH264MainAutoLevel = @"H264_Main_AutoLevel";
NSString *const AVVideoProfileLevelH264High40 = @"H264_High_4_0";
NSString *const AVVideoProfileLevelH264High41 = @"H264_High_4_1";
NSString *const AVVideoProfileLevelH264HighAutoLevel = @"H264_High_AutoLevel";
