/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A key's value is its name after its group's prefix ("..._Key_" or
 * "..._Extension_").
 */

#include "CoreMedia.h"

#define KEY(name, value) const CFStringRef name = CFSTR(value)

KEY(kCMSampleAttachmentKey_NotSync, "NotSync");
KEY(kCMSampleAttachmentKey_PartialSync, "PartialSync");
KEY(kCMSampleAttachmentKey_HasRedundantCoding, "HasRedundantCoding");
KEY(kCMSampleAttachmentKey_IsDependedOnByOthers, "IsDependedOnByOthers");
KEY(kCMSampleAttachmentKey_DependsOnOthers, "DependsOnOthers");
KEY(kCMSampleAttachmentKey_EarlierDisplayTimesAllowed, "EarlierDisplayTimesAllowed");
KEY(kCMSampleAttachmentKey_DisplayImmediately, "DisplayImmediately");
KEY(kCMSampleAttachmentKey_DoNotDisplay, "DoNotDisplay");

KEY(kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, "SampleDescriptionExtensionAtoms");
KEY(kCMFormatDescriptionExtension_ColorPrimaries, "CVImageBufferColorPrimaries");
KEY(kCMFormatDescriptionExtension_TransferFunction, "CVImageBufferTransferFunction");
KEY(kCMFormatDescriptionExtension_YCbCrMatrix, "CVImageBufferYCbCrMatrix");
KEY(kCMFormatDescriptionExtension_FullRangeVideo, "FullRangeVideo");
