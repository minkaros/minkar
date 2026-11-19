/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFKitPlatform.h>

@class PDFPage;

NS_ASSUME_NONNULL_BEGIN

/* A coordinate or zoom a destination leaves to the viewer. */
PDFKIT_EXTERN const CGFloat kPDFDestinationUnspecifiedValue;

PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFDestination : NSObject <NSCopying>
- (instancetype)initWithPage:(PDFPage *)page atPoint:(PDFPoint)point;
@property (nonatomic, weak, readonly, nullable) PDFPage *page;
@property (nonatomic, readonly) PDFPoint point;
@property (nonatomic) CGFloat zoom;
- (NSComparisonResult)compare:(PDFDestination *)destination;
@end

NS_ASSUME_NONNULL_END
