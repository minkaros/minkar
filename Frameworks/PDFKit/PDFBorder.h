/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFKitPlatform.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, PDFBorderStyle) {
    kPDFBorderStyleSolid = 0,
    kPDFBorderStyleDashed = 1,
    kPDFBorderStyleBeveled = 2,
    kPDFBorderStyleInset = 3,
    kPDFBorderStyleUnderline = 4
};

typedef NSString *PDFBorderKey NS_STRING_ENUM;
PDFKIT_EXTERN PDFBorderKey const PDFBorderKeyLineWidth;
PDFKIT_EXTERN PDFBorderKey const PDFBorderKeyStyle;
PDFKIT_EXTERN PDFBorderKey const PDFBorderKeyDashPattern;

/* An annotation's border: its width, style and dash pattern. */
PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFBorder : NSObject <NSCopying, NSCoding>
@property (nonatomic) PDFBorderStyle style;
@property (nonatomic) CGFloat lineWidth;
@property (nonatomic, copy, nullable) NSArray *dashPattern;
@property (nonatomic, readonly, copy) NSDictionary *borderKeyValues;
- (void)drawInRect:(PDFRect)rect;
@end

NS_ASSUME_NONNULL_END
