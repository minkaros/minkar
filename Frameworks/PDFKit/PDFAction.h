/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFKitPlatform.h>

NS_ASSUME_NONNULL_BEGIN

/* What a link or an outline item does: its type is the PDF action's name
   (GoTo, URI as "URL", Named...). */
PDFKIT_CLASS_AVAILABLE(10_5, 11_0)
@interface PDFAction : NSObject <NSCopying>
@property (nonatomic, readonly) NSString *type;
@end

NS_ASSUME_NONNULL_END
