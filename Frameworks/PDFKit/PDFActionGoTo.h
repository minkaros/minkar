/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFAction.h>

@class PDFDestination;

NS_ASSUME_NONNULL_BEGIN

PDFKIT_CLASS_AVAILABLE(10_5, 11_0)
@interface PDFActionGoTo : PDFAction <NSCopying>
- (instancetype)initWithDestination:(PDFDestination *)destination;
@property (nonatomic, strong) PDFDestination *destination;
@end

NS_ASSUME_NONNULL_END
