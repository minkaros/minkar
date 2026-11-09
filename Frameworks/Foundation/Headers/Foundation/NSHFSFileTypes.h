// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>

@class NSString;

NS_ASSUME_NONNULL_BEGIN

/* An HFS type code as a string ('TEXT', quotes included), and back. */
FOUNDATION_EXPORT NSString *NSFileTypeForHFSTypeCode(OSType hfsFileTypeCode);
FOUNDATION_EXPORT OSType NSHFSTypeCodeFromFileType(NSString *_Nullable fileTypeString);
FOUNDATION_EXPORT NSString *_Nullable NSHFSTypeOfFile(NSString *fullFilePath);

NS_ASSUME_NONNULL_END
