/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFKitPlatform.h>

@class PDFDocument, PDFSelection, PDFAnnotation;

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

/* Its annotations: those in its Annots as read, and those added. */
@property (nonatomic, readonly) NSArray<PDFAnnotation *> *annotations;
@property (nonatomic) BOOL displaysAnnotations;
- (void)addAnnotation:(PDFAnnotation *)annotation;
- (void)removeAnnotation:(PDFAnnotation *)annotation;
/* The topmost annotation at point (not a popup), or nil. */
- (nullable PDFAnnotation *)annotationAtPoint:(PDFPoint)point;

- (CGAffineTransform)transformForBox:(PDFDisplayBox)box;
- (void)drawWithBox:(PDFDisplayBox)box toContext:(CGContextRef)context;
- (void)transformContext:(CGContextRef)context forBox:(PDFDisplayBox)box;
- (void)drawWithBox:(PDFDisplayBox)box;
- (NSImage *)thumbnailOfSize:(PDFSize)size forBox:(PDFDisplayBox)box;

/* Its text, as its content shows it: characters in that order, a space
   where a line goes on (a gap in it, or the next line close under it), a
   line feed where it ends; the last line ends with one too. */
@property (nonatomic, readonly, nullable) NSString *string;
@property (nonatomic, readonly, nullable) NSAttributedString *attributedString;
@property (nonatomic, readonly) NSUInteger numberOfCharacters;
/* A character's box on the page: from its baseline's start, as wide as it
   advances, from below the baseline to its font's cap height. */
- (PDFRect)characterBoundsAtIndex:(NSInteger)index;
/* The character whose box holds point, or NSNotFound. */
- (NSInteger)characterIndexAtPoint:(PDFPoint)point;
/* The characters whose boxes rect holds whole. */
- (nullable PDFSelection *)selectionForRect:(PDFRect)rect;
/* The word (letters and digits) at point; nil off one. */
- (nullable PDFSelection *)selectionForWordAtPoint:(PDFPoint)point;
/* The line at point, without its separator. */
- (nullable PDFSelection *)selectionForLineAtPoint:(PDFPoint)point;
/* From one point to the other, in either order: each point falls before or
   after the character it is on, as it is left or right of its middle. */
- (nullable PDFSelection *)selectionFromPoint:(PDFPoint)startPoint toPoint:(PDFPoint)endPoint;
- (nullable PDFSelection *)selectionForRange:(NSRange)range;

@property (nonatomic, readonly, nullable) NSData *dataRepresentation;
@end

NS_ASSUME_NONNULL_END
