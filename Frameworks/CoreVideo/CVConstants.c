/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A key's value is its name after kCV and without "Key"; a value's is the
 * part of its name after the key's.
 */

#include <CoreVideo/CVPixelBuffer.h>

#define KEY(name, value) const CFStringRef name = CFSTR(value)

KEY(kCVImageBufferCGColorSpaceKey, "CVImageBufferCGColorSpace");
KEY(kCVImageBufferColorPrimariesKey, "CVImageBufferColorPrimaries");
KEY(kCVImageBufferColorPrimaries_ITU_R_709_2, "ITU_R_709_2");
KEY(kCVImageBufferColorPrimaries_EBU_3213, "EBU_3213");
KEY(kCVImageBufferColorPrimaries_SMPTE_C, "SMPTE_C");
KEY(kCVImageBufferColorPrimaries_P22, "P22");
KEY(kCVImageBufferColorPrimaries_DCI_P3, "DCI_P3");
KEY(kCVImageBufferColorPrimaries_P3_D65, "P3_D65");
KEY(kCVImageBufferColorPrimaries_ITU_R_2020, "ITU_R_2020");
KEY(kCVImageBufferTransferFunctionKey, "CVImageBufferTransferFunction");
KEY(kCVImageBufferTransferFunction_ITU_R_709_2, "ITU_R_709_2");
KEY(kCVImageBufferTransferFunction_SMPTE_240M_1995, "SMPTE_240M_1995");
KEY(kCVImageBufferTransferFunction_UseGamma, "UseGamma");
KEY(kCVImageBufferTransferFunction_sRGB, "sRGB");
KEY(kCVImageBufferTransferFunction_Linear, "Linear");
KEY(kCVImageBufferTransferFunction_ITU_R_2020, "ITU_R_2020");
KEY(kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ, "SMPTE_ST_2084_PQ");
KEY(kCVImageBufferTransferFunction_ITU_R_2100_HLG, "ITU_R_2100_HLG");
KEY(kCVImageBufferYCbCrMatrixKey, "CVImageBufferYCbCrMatrix");
KEY(kCVImageBufferYCbCrMatrix_ITU_R_709_2, "ITU_R_709_2");
KEY(kCVImageBufferYCbCrMatrix_ITU_R_601_4, "ITU_R_601_4");
KEY(kCVImageBufferYCbCrMatrix_SMPTE_240M_1995, "SMPTE_240M_1995");
KEY(kCVImageBufferYCbCrMatrix_ITU_R_2020, "ITU_R_2020");
KEY(kCVImageBufferChromaLocationTopFieldKey, "CVImageBufferChromaLocationTopField");
KEY(kCVImageBufferChromaLocationBottomFieldKey, "CVImageBufferChromaLocationBottomField");
KEY(kCVImageBufferChromaLocation_Left, "Left");
KEY(kCVImageBufferChromaLocation_Center, "Center");
KEY(kCVImageBufferChromaLocation_TopLeft, "TopLeft");
KEY(kCVImageBufferChromaLocation_Top, "Top");
KEY(kCVImageBufferChromaLocation_BottomLeft, "BottomLeft");
KEY(kCVImageBufferChromaLocation_Bottom, "Bottom");
KEY(kCVImageBufferMasteringDisplayColorVolumeKey, "CVImageBufferMasteringDisplayColorVolume");
KEY(kCVImageBufferContentLightLevelInfoKey, "CVImageBufferContentLightLevelInfo");

KEY(kCVPixelBufferPixelFormatTypeKey, "CVPixelBufferPixelFormatType");
KEY(kCVPixelBufferWidthKey, "CVPixelBufferWidth");
KEY(kCVPixelBufferHeightKey, "CVPixelBufferHeight");
KEY(kCVPixelBufferBytesPerRowAlignmentKey, "CVPixelBufferBytesPerRowAlignment");
KEY(kCVPixelBufferIOSurfacePropertiesKey, "CVPixelBufferIOSurfaceProperties");
KEY(kCVPixelBufferOpenGLCompatibilityKey, "CVPixelBufferOpenGLCompatibility");
KEY(kCVPixelBufferMetalCompatibilityKey, "CVPixelBufferMetalCompatibility");
KEY(kCVPixelBufferCGImageCompatibilityKey, "CVPixelBufferCGImageCompatibility");
KEY(kCVPixelBufferCGBitmapContextCompatibilityKey, "CVPixelBufferCGBitmapContextCompatibility");

KEY(kCVImageBufferTransferFunction_SMPTE_ST_428_1, "SMPTE_ST_428_1");
KEY(kCVImageBufferChromaLocation_DV420, "DV 4:2:0");
KEY(kCVImageBufferCleanApertureKey, "CVCleanAperture");
KEY(kCVImageBufferCleanApertureWidthKey, "Width");
KEY(kCVImageBufferCleanApertureHeightKey, "Height");
KEY(kCVImageBufferCleanApertureHorizontalOffsetKey, "HorizontalOffset");
KEY(kCVImageBufferCleanApertureVerticalOffsetKey, "VerticalOffset");
KEY(kCVImageBufferPreferredCleanApertureKey, "CVPreferredCleanAperture");
KEY(kCVImageBufferFieldCountKey, "CVFieldCount");
KEY(kCVImageBufferFieldDetailKey, "CVFieldDetail");
KEY(kCVImageBufferFieldDetailTemporalTopFirst, "TemporalTopFirst");
KEY(kCVImageBufferFieldDetailTemporalBottomFirst, "TemporalBottomFirst");
KEY(kCVImageBufferFieldDetailSpatialFirstLineEarly, "SpatialFirstLineEarly");
KEY(kCVImageBufferFieldDetailSpatialFirstLineLate, "SpatialFirstLineLate");
KEY(kCVImageBufferPixelAspectRatioKey, "CVPixelAspectRatio");
KEY(kCVImageBufferPixelAspectRatioHorizontalSpacingKey, "HorizontalSpacing");
KEY(kCVImageBufferPixelAspectRatioVerticalSpacingKey, "VerticalSpacing");
KEY(kCVImageBufferDisplayDimensionsKey, "CVDisplayDimensions");
KEY(kCVImageBufferDisplayWidthKey, "Width");
KEY(kCVImageBufferDisplayHeightKey, "Height");
KEY(kCVImageBufferGammaLevelKey, "CVImageBufferGammaLevel");
KEY(kCVImageBufferICCProfileKey, "CVImageBufferICCProfile");
KEY(kCVImageBufferChromaSubsamplingKey, "CVImageBufferChromaSubsampling");
KEY(kCVImageBufferChromaSubsampling_420, "4:2:0");
KEY(kCVImageBufferChromaSubsampling_422, "4:2:2");
KEY(kCVImageBufferChromaSubsampling_411, "4:1:1");
KEY(kCVImageBufferAlphaChannelIsOpaque, "CVImageBufferAlphaChannelIsOpaque");
KEY(kCVBufferMovieTimeKey, "QTMovieTime");
KEY(kCVBufferTimeValueKey, "TimeValue");
KEY(kCVBufferTimeScaleKey, "TimeScale");
KEY(kCVBufferPropagatedAttachmentsKey, "PropagatedAttachments");
KEY(kCVBufferNonPropagatedAttachmentsKey, "NonPropagatedAttachments");
KEY(kCVPixelBufferMemoryAllocatorKey, "MemoryAllocator");
KEY(kCVPixelBufferExtendedPixelsLeftKey, "ExtendedPixelsLeft");
KEY(kCVPixelBufferExtendedPixelsTopKey, "ExtendedPixelsTop");
KEY(kCVPixelBufferExtendedPixelsRightKey, "ExtendedPixelsRight");
KEY(kCVPixelBufferExtendedPixelsBottomKey, "ExtendedPixelsBottom");
KEY(kCVPixelBufferPlaneAlignmentKey, "PlaneAlignment");
