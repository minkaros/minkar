/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The keys of audio output and encoding settings.
 */

#import <Foundation/Foundation.h>

FOUNDATION_EXPORT NSString *const AVFormatIDKey;
FOUNDATION_EXPORT NSString *const AVSampleRateKey;
FOUNDATION_EXPORT NSString *const AVNumberOfChannelsKey;
FOUNDATION_EXPORT NSString *const AVLinearPCMBitDepthKey;
FOUNDATION_EXPORT NSString *const AVLinearPCMIsBigEndianKey;
FOUNDATION_EXPORT NSString *const AVLinearPCMIsFloatKey;
FOUNDATION_EXPORT NSString *const AVLinearPCMIsNonInterleaved;
FOUNDATION_EXPORT NSString *const AVAudioFileTypeKey;
FOUNDATION_EXPORT NSString *const AVEncoderAudioQualityKey;
FOUNDATION_EXPORT NSString *const AVEncoderAudioQualityForVBRKey;
FOUNDATION_EXPORT NSString *const AVEncoderBitRateKey;
FOUNDATION_EXPORT NSString *const AVEncoderBitRatePerChannelKey;
FOUNDATION_EXPORT NSString *const AVEncoderBitRateStrategyKey;
FOUNDATION_EXPORT NSString *const AVEncoderBitDepthHintKey;
FOUNDATION_EXPORT NSString *const AVSampleRateConverterAlgorithmKey;
FOUNDATION_EXPORT NSString *const AVSampleRateConverterAudioQualityKey;
FOUNDATION_EXPORT NSString *const AVChannelLayoutKey;

FOUNDATION_EXPORT NSString *const AVAudioBitRateStrategy_Constant;
FOUNDATION_EXPORT NSString *const AVAudioBitRateStrategy_LongTermAverage;
FOUNDATION_EXPORT NSString *const AVAudioBitRateStrategy_VariableConstrained;
FOUNDATION_EXPORT NSString *const AVAudioBitRateStrategy_Variable;
FOUNDATION_EXPORT NSString *const AVSampleRateConverterAlgorithm_Normal;
FOUNDATION_EXPORT NSString *const AVSampleRateConverterAlgorithm_Mastering;
FOUNDATION_EXPORT NSString *const AVSampleRateConverterAlgorithm_MinimumPhase;

typedef NS_ENUM(NSInteger, AVAudioQuality) {
    AVAudioQualityMin = 0,
    AVAudioQualityLow = 0x20,
    AVAudioQualityMedium = 0x40,
    AVAudioQualityHigh = 0x60,
    AVAudioQualityMax = 0x7F,
};
