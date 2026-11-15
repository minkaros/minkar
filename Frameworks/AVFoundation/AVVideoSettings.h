/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The keys and values of video output and encoding settings.
 */

#import <Foundation/Foundation.h>

typedef NSString *AVVideoCodecType;
FOUNDATION_EXPORT AVVideoCodecType const AVVideoCodecTypeHEVC;
FOUNDATION_EXPORT AVVideoCodecType const AVVideoCodecTypeH264;
FOUNDATION_EXPORT AVVideoCodecType const AVVideoCodecTypeJPEG;
FOUNDATION_EXPORT AVVideoCodecType const AVVideoCodecTypeAppleProRes4444;
FOUNDATION_EXPORT AVVideoCodecType const AVVideoCodecTypeAppleProRes422;
FOUNDATION_EXPORT AVVideoCodecType const AVVideoCodecTypeAppleProRes422HQ;
FOUNDATION_EXPORT AVVideoCodecType const AVVideoCodecTypeAppleProRes422LT;
FOUNDATION_EXPORT AVVideoCodecType const AVVideoCodecTypeAppleProRes422Proxy;
FOUNDATION_EXPORT AVVideoCodecType const AVVideoCodecTypeHEVCWithAlpha;

FOUNDATION_EXPORT NSString *const AVVideoCodecKey;
FOUNDATION_EXPORT NSString *const AVVideoWidthKey;
FOUNDATION_EXPORT NSString *const AVVideoHeightKey;
FOUNDATION_EXPORT NSString *const AVVideoPixelAspectRatioKey;
FOUNDATION_EXPORT NSString *const AVVideoPixelAspectRatioHorizontalSpacingKey;
FOUNDATION_EXPORT NSString *const AVVideoPixelAspectRatioVerticalSpacingKey;
FOUNDATION_EXPORT NSString *const AVVideoCleanApertureKey;
FOUNDATION_EXPORT NSString *const AVVideoCleanApertureWidthKey;
FOUNDATION_EXPORT NSString *const AVVideoCleanApertureHeightKey;
FOUNDATION_EXPORT NSString *const AVVideoCleanApertureHorizontalOffsetKey;
FOUNDATION_EXPORT NSString *const AVVideoCleanApertureVerticalOffsetKey;
FOUNDATION_EXPORT NSString *const AVVideoScalingModeKey;
FOUNDATION_EXPORT NSString *const AVVideoScalingModeFit;
FOUNDATION_EXPORT NSString *const AVVideoScalingModeResize;
FOUNDATION_EXPORT NSString *const AVVideoScalingModeResizeAspect;
FOUNDATION_EXPORT NSString *const AVVideoScalingModeResizeAspectFill;
FOUNDATION_EXPORT NSString *const AVVideoColorPropertiesKey;
FOUNDATION_EXPORT NSString *const AVVideoColorPrimariesKey;
FOUNDATION_EXPORT NSString *const AVVideoTransferFunctionKey;
FOUNDATION_EXPORT NSString *const AVVideoYCbCrMatrixKey;
FOUNDATION_EXPORT NSString *const AVVideoAllowWideColorKey;
FOUNDATION_EXPORT NSString *const AVVideoCompressionPropertiesKey;
FOUNDATION_EXPORT NSString *const AVVideoAverageBitRateKey;
FOUNDATION_EXPORT NSString *const AVVideoQualityKey;
FOUNDATION_EXPORT NSString *const AVVideoMaxKeyFrameIntervalKey;
FOUNDATION_EXPORT NSString *const AVVideoMaxKeyFrameIntervalDurationKey;
FOUNDATION_EXPORT NSString *const AVVideoAllowFrameReorderingKey;
FOUNDATION_EXPORT NSString *const AVVideoExpectedSourceFrameRateKey;
FOUNDATION_EXPORT NSString *const AVVideoAverageNonDroppableFrameRateKey;
FOUNDATION_EXPORT NSString *const AVVideoH264EntropyModeKey;
FOUNDATION_EXPORT NSString *const AVVideoH264EntropyModeCAVLC;
FOUNDATION_EXPORT NSString *const AVVideoH264EntropyModeCABAC;
FOUNDATION_EXPORT NSString *const AVVideoProfileLevelKey;
FOUNDATION_EXPORT NSString *const AVVideoProfileLevelH264Baseline30;
FOUNDATION_EXPORT NSString *const AVVideoProfileLevelH264Baseline31;
FOUNDATION_EXPORT NSString *const AVVideoProfileLevelH264Baseline41;
FOUNDATION_EXPORT NSString *const AVVideoProfileLevelH264BaselineAutoLevel;
FOUNDATION_EXPORT NSString *const AVVideoProfileLevelH264Main30;
FOUNDATION_EXPORT NSString *const AVVideoProfileLevelH264Main31;
FOUNDATION_EXPORT NSString *const AVVideoProfileLevelH264Main32;
FOUNDATION_EXPORT NSString *const AVVideoProfileLevelH264Main41;
FOUNDATION_EXPORT NSString *const AVVideoProfileLevelH264MainAutoLevel;
FOUNDATION_EXPORT NSString *const AVVideoProfileLevelH264High40;
FOUNDATION_EXPORT NSString *const AVVideoProfileLevelH264High41;
FOUNDATION_EXPORT NSString *const AVVideoProfileLevelH264HighAutoLevel;
FOUNDATION_EXPORT NSString *const AVVideoEncoderSpecificationKey;
