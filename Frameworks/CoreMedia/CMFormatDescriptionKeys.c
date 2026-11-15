/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Format description extensions and their values. The ones an image
 * buffer has too are the same strings as CoreVideo's keys, so the
 * extensions and a buffer's attachments can be compared.
 */

#include "CMFormatDescription.h"

#define KEY(name, value) const CFStringRef name = CFSTR(value)

KEY(kCMFormatDescriptionExtension_OriginalCompressionSettings, "OriginalCompressionSettings");
KEY(kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, "SampleDescriptionExtensionAtoms");
KEY(kCMFormatDescriptionExtension_VerbatimSampleDescription, "VerbatimSampleDescription");
KEY(kCMFormatDescriptionExtension_VerbatimISOSampleEntry, "VerbatimISOSampleEntry");
KEY(kCMFormatDescriptionExtension_FormatName, "FormatName");
KEY(kCMFormatDescriptionExtension_Depth, "Depth");
KEY(kCMFormatDescriptionExtension_CleanAperture, "CVCleanAperture");
KEY(kCMFormatDescriptionKey_CleanApertureWidth, "Width");
KEY(kCMFormatDescriptionKey_CleanApertureHeight, "Height");
KEY(kCMFormatDescriptionKey_CleanApertureHorizontalOffset, "HorizontalOffset");
KEY(kCMFormatDescriptionKey_CleanApertureVerticalOffset, "VerticalOffset");
KEY(kCMFormatDescriptionKey_CleanApertureWidthRational, "CleanApertureWidthRational");
KEY(kCMFormatDescriptionKey_CleanApertureHeightRational, "CleanApertureHeightRational");
KEY(kCMFormatDescriptionKey_CleanApertureHorizontalOffsetRational, "CleanApertureHorizontalOffsetRational");
KEY(kCMFormatDescriptionKey_CleanApertureVerticalOffsetRational, "CleanApertureVerticalOffsetRational");
KEY(kCMFormatDescriptionExtension_FieldCount, "CVFieldCount");
KEY(kCMFormatDescriptionExtension_FieldDetail, "CVFieldDetail");
KEY(kCMFormatDescriptionFieldDetail_TemporalTopFirst, "TemporalTopFirst");
KEY(kCMFormatDescriptionFieldDetail_TemporalBottomFirst, "TemporalBottomFirst");
KEY(kCMFormatDescriptionFieldDetail_SpatialFirstLineEarly, "SpatialFirstLineEarly");
KEY(kCMFormatDescriptionFieldDetail_SpatialFirstLineLate, "SpatialFirstLineLate");
KEY(kCMFormatDescriptionExtension_PixelAspectRatio, "CVPixelAspectRatio");
KEY(kCMFormatDescriptionKey_PixelAspectRatioHorizontalSpacing, "HorizontalSpacing");
KEY(kCMFormatDescriptionKey_PixelAspectRatioVerticalSpacing, "VerticalSpacing");
KEY(kCMFormatDescriptionExtension_ColorPrimaries, "CVImageBufferColorPrimaries");
KEY(kCMFormatDescriptionColorPrimaries_ITU_R_709_2, "ITU_R_709_2");
KEY(kCMFormatDescriptionColorPrimaries_EBU_3213, "EBU_3213");
KEY(kCMFormatDescriptionColorPrimaries_SMPTE_C, "SMPTE_C");
KEY(kCMFormatDescriptionColorPrimaries_DCI_P3, "DCI_P3");
KEY(kCMFormatDescriptionColorPrimaries_P3_D65, "P3_D65");
KEY(kCMFormatDescriptionColorPrimaries_ITU_R_2020, "ITU_R_2020");
KEY(kCMFormatDescriptionColorPrimaries_P22, "P22");
KEY(kCMFormatDescriptionExtension_TransferFunction, "CVImageBufferTransferFunction");
KEY(kCMFormatDescriptionTransferFunction_ITU_R_709_2, "ITU_R_709_2");
KEY(kCMFormatDescriptionTransferFunction_SMPTE_240M_1995, "SMPTE_240M_1995");
KEY(kCMFormatDescriptionTransferFunction_UseGamma, "UseGamma");
KEY(kCMFormatDescriptionTransferFunction_ITU_R_2020, "ITU_R_2020");
KEY(kCMFormatDescriptionTransferFunction_SMPTE_ST_428_1, "SMPTE_ST_428_1");
KEY(kCMFormatDescriptionTransferFunction_SMPTE_ST_2084_PQ, "SMPTE_ST_2084_PQ");
KEY(kCMFormatDescriptionTransferFunction_ITU_R_2100_HLG, "ITU_R_2100_HLG");
KEY(kCMFormatDescriptionTransferFunction_Linear, "Linear");
KEY(kCMFormatDescriptionTransferFunction_sRGB, "sRGB");
KEY(kCMFormatDescriptionExtension_GammaLevel, "CVImageBufferGammaLevel");
KEY(kCMFormatDescriptionExtension_YCbCrMatrix, "CVImageBufferYCbCrMatrix");
KEY(kCMFormatDescriptionYCbCrMatrix_ITU_R_709_2, "ITU_R_709_2");
KEY(kCMFormatDescriptionYCbCrMatrix_ITU_R_601_4, "ITU_R_601_4");
KEY(kCMFormatDescriptionYCbCrMatrix_SMPTE_240M_1995, "SMPTE_240M_1995");
KEY(kCMFormatDescriptionYCbCrMatrix_ITU_R_2020, "ITU_R_2020");
KEY(kCMFormatDescriptionExtension_FullRangeVideo, "FullRangeVideo");
KEY(kCMFormatDescriptionExtension_ICCProfile, "CVImageBufferICCProfile");
KEY(kCMFormatDescriptionExtension_BytesPerRow, "CVBytesPerRow");
KEY(kCMFormatDescriptionExtension_ChromaLocationTopField, "CVImageBufferChromaLocationTopField");
KEY(kCMFormatDescriptionExtension_ChromaLocationBottomField, "CVImageBufferChromaLocationBottomField");
KEY(kCMFormatDescriptionChromaLocation_Left, "Left");
KEY(kCMFormatDescriptionChromaLocation_Center, "Center");
KEY(kCMFormatDescriptionChromaLocation_TopLeft, "TopLeft");
KEY(kCMFormatDescriptionChromaLocation_Top, "Top");
KEY(kCMFormatDescriptionChromaLocation_BottomLeft, "BottomLeft");
KEY(kCMFormatDescriptionChromaLocation_Bottom, "Bottom");
KEY(kCMFormatDescriptionChromaLocation_DV420, "DV 4:2:0");
KEY(kCMFormatDescriptionConformsToMPEG2VideoProfile, "ConformsToMPEG2VideoProfile");
KEY(kCMFormatDescriptionExtension_TemporalQuality, "TemporalQuality");
KEY(kCMFormatDescriptionExtension_SpatialQuality, "SpatialQuality");
KEY(kCMFormatDescriptionExtension_VerbatimImageDescription, "VerbatimImageDescription");
KEY(kCMFormatDescriptionExtension_Version, "Version");
KEY(kCMFormatDescriptionExtension_RevisionLevel, "RevisionLevel");
KEY(kCMFormatDescriptionExtension_Vendor, "Vendor");
KEY(kCMFormatDescriptionVendor_Apple, "appl");
KEY(kCMFormatDescriptionExtension_MasteringDisplayColorVolume, "CVImageBufferMasteringDisplayColorVolume");
KEY(kCMFormatDescriptionExtension_ContentLightLevelInfo, "CVImageBufferContentLightLevelInfo");
KEY(kCMFormatDescriptionExtension_AlternativeTransferCharacteristics, "CVImageBufferAlternativeTransferCharacteristics");
