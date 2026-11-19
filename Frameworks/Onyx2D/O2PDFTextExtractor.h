/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A PDF page's text as shown: each character's Unicode value, where it
 * is on the page and how wide, in the order the content shows it (for
 * PDFKit's text).
 */

#import <Onyx2D/O2Geometry.h>
#import <Foundation/NSData.h>

@class O2PDFPage;

typedef struct {
    unichar unicode;
    O2Point origin;       // its baseline's start, in the page's default space
    O2Float advance;      // how far the next one starts, along the baseline
    O2Float fontSize;     // in the page's space (the size times the scaling)
    O2Float ascent;       // above and below the baseline, in the page's space
    O2Float descent;      // (negative below)
} O2PDFTextCharacter;

/* The page's characters, as an array of O2PDFTextCharacter. */
NSData *O2PDFPageCopyTextCharacters(O2PDFPage *page);
