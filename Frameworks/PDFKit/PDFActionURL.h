/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFAction.h>

NS_ASSUME_NONNULL_BEGIN

PDFKIT_CLASS_AVAILABLE(10_5, 11_0)
@interface PDFActionURL : PDFAction <NSCopying>
- (instancetype)initWithURL:(NSURL *)url;
@property (nonatomic, copy, nullable) NSURL *URL;
@end

NS_ASSUME_NONNULL_END
