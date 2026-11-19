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
@end

@interface PDFPage (PDFKitPrivate)
- (instancetype)_initWithDocument:(PDFDocument *)document pageRef:(CGPDFPageRef)pageRef;
- (void)_setDocument:(nullable PDFDocument *)document;
@end

@interface PDFOutline (PDFKitPrivate)
- (void)_setDocument:(nullable PDFDocument *)document;
@end

/* A PDF text string as an NSString, a date string as an NSDate. */
NSString *_Nullable PDFKitString(CGPDFStringRef _Nullable string);
NSDate *_Nullable PDFKitDate(CGPDFStringRef _Nullable string);

NS_ASSUME_NONNULL_END
