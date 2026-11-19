/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFKitPlatform.h>
#import <PDFKit/PDFPage.h>

@class PDFDocument;

NS_ASSUME_NONNULL_BEGIN

/* Text selected in a document: ranges of characters on its pages. */
PDFKIT_CLASS_AVAILABLE(10_4, 11_0)
@interface PDFSelection : NSObject <NSCopying>
- (instancetype)initWithDocument:(PDFDocument *)document NS_DESIGNATED_INITIALIZER;

@property (nonatomic, readonly) NSArray<PDFPage *> *pages;
@property (nonatomic, copy, nullable) NSColor *color;
/* The ranges' text: those on a page joined with a carriage return, the
   pages' with a line feed. */
@property (nonatomic, readonly, nullable) NSString *string;
@property (nonatomic, readonly, nullable) NSAttributedString *attributedString;
- (PDFRect)boundsForPage:(PDFPage *)page;
- (NSUInteger)numberOfTextRangesOnPage:(PDFPage *)page;
- (NSRange)rangeAtIndex:(NSUInteger)index onPage:(PDFPage *)page;
- (NSArray<PDFSelection *> *)selectionsByLine;
- (void)addSelection:(PDFSelection *)selection;
- (void)addSelections:(NSArray<PDFSelection *> *)selections;
- (void)extendSelectionAtEnd:(NSInteger)succeed;
- (void)extendSelectionAtStart:(NSInteger)precede;
- (void)drawForPage:(PDFPage *)page active:(BOOL)active;
- (void)drawForPage:(PDFPage *)page withBox:(PDFDisplayBox)box active:(BOOL)active;
@end

NS_ASSUME_NONNULL_END
