/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFKitPlatform.h>

@class PDFDocument;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, PDFDisplayBox) {
    kPDFDisplayBoxMediaBox = 0,
    kPDFDisplayBoxCropBox = 1,
    kPDFDisplayBoxBleedBox = 2,
    kPDFDisplayBoxTrimBox = 3,
    kPDFDisplayBoxArtBox = 4
};

/* A page of a document (its CGPDFPage), or a page made in code (blank, or
   an image's). */
PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFPage : NSObject <NSCopying>
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithImage:(NSImage *)image;

@property (nonatomic, readonly, weak, nullable) PDFDocument *document;
@property (nonatomic, readonly, nullable) CGPDFPageRef pageRef;
/* Its page label (the document's PageLabels, else its number), as
   the page it was in the document as read; a page made in code has none. */
@property (nonatomic, readonly, nullable) NSString *label;

- (PDFRect)boundsForBox:(PDFDisplayBox)box;
- (void)setBounds:(PDFRect)bounds forBox:(PDFDisplayBox)box;
/* Degrees, a multiple of 90. */
@property (nonatomic) NSInteger rotation;

- (CGAffineTransform)transformForBox:(PDFDisplayBox)box;
- (void)drawWithBox:(PDFDisplayBox)box toContext:(CGContextRef)context;
- (void)transformContext:(CGContextRef)context forBox:(PDFDisplayBox)box;
- (void)drawWithBox:(PDFDisplayBox)box;
- (NSImage *)thumbnailOfSize:(PDFSize)size forBox:(PDFDisplayBox)box;

@property (nonatomic, readonly, nullable) NSData *dataRepresentation;
@end

NS_ASSUME_NONNULL_END
