/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A document's objects as written, references not looked through: for
 * appending an incremental update (PDFKit's saving).
 */

#import <Onyx2D/O2PDFObject.h>
#import <Foundation/NSData.h>
#import <Foundation/NSArray.h>

@class O2PDFDocument, O2PDFDictionary;

/* The object as PDF text, a reference written as one ("n g R"). */
void O2PDFAppendRawObject(NSMutableData *data, O2PDFObject *object);
/* A dictionary's entries as PDF text, but those keys. */
NSData *O2PDFDictionaryCopyRawEncoding(O2PDFDictionary *dictionary, NSArray *excludedKeys);
/* A page's object number and generation (pages from 0). */
BOOL O2PDFDocumentGetPageReference(O2PDFDocument *document, unsigned pageIndex, int *number, int *generation);
/* A page dictionary's Annots as written: for each, @[number, generation]
   or NSNull for one written inline. */
NSArray *O2PDFDictionaryCopyAnnotationReferences(O2PDFDictionary *page);
/* The document's last trailer (or cross-reference stream's dictionary). */
O2PDFDictionary *O2PDFDocumentGetTrailer(O2PDFDocument *document);
