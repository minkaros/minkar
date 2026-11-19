/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * PDFKit's own: objects made from a document's PDF objects.
 */

#import <PDFKit/PDFKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface PDFDocument (PDFKitPrivate)
/* The page a page dictionary is (by identity), or nil. */
- (nullable PDFPage *)_pageForDictionary:(CGPDFDictionaryRef)dictionary;
/* A destination: an explicit array, or a name to look up. */
- (nullable PDFDestination *)_destinationForObject:(CGPDFObjectRef)object;
- (nullable PDFAction *)_actionForDictionary:(CGPDFDictionaryRef)dictionary;
/* A page's annotations were added or removed (the document has changed). */
- (void)_annotationsChanged;
@end

@interface PDFPage (PDFKitPrivate)
- (instancetype)_initWithDocument:(PDFDocument *)document pageRef:(CGPDFPageRef)pageRef;
- (void)_setDocument:(nullable PDFDocument *)document;
/* Where a point falls between characters, or NSNotFound off the text. */
- (NSUInteger)_boundaryAtPoint:(PDFPoint)point;
@end

@interface PDFSelection (PDFKitPrivate)
/* Characters on a page added (clipped to its text). */
- (void)_addRange:(NSRange)range onPage:(PDFPage *)page;
@end

@interface PDFAnnotation (PDFKitWriting)
- (nullable NSArray *)_sourceReference;
- (void)_setSourceReference:(nullable NSArray *)reference;
- (BOOL)_isModified;
/* As a PDF dictionary, its page and destinations by their objects
   (a page's object number for its index). */
- (NSData *)_pdfDataWithPageReference:(NSString *)page pageReferences:(NSString *_Nullable (^)(PDFPage *page))references;
@end

@interface PDFPage (PDFKitWriting)
- (void)_annotationChanged;
- (BOOL)_annotationsChangedSinceRead;
- (nullable NSArray<PDFAnnotation *> *)_readAnnotations;
@end

@interface PDFAnnotation (PDFKitReading)
+ (nullable PDFAnnotation *)_annotationWithDictionary:(CGPDFDictionaryRef)dictionary page:(PDFPage *)page;
@end

@interface PDFOutline (PDFKitPrivate)
- (void)_setDocument:(nullable PDFDocument *)document;
@end

/* A PDF text string as an NSString, a date string as an NSDate. */
NSString *_Nullable PDFKitString(CGPDFStringRef _Nullable string);
NSDate *_Nullable PDFKitDate(CGPDFStringRef _Nullable string);

NS_ASSUME_NONNULL_END
