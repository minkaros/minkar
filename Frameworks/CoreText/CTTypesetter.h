/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CTTypesetter: lines made from parts of an attributed string, and where a
 * line of a given width breaks, as CoreText documents it.
 */

#ifndef __CTTYPESETTER__
#define __CTTYPESETTER__

#include <CoreText/CoreTextExport.h>
#include <CoreText/CTLine.h>
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef const struct __CTTypesetter *CTTypesetterRef;

CORETEXT_EXPORT CFTypeID CTTypesetterGetTypeID(void);
CORETEXT_EXPORT CTTypesetterRef CTTypesetterCreateWithAttributedString(CFAttributedStringRef string);
CORETEXT_EXPORT CTTypesetterRef CTTypesetterCreateWithAttributedStringAndOptions(CFAttributedStringRef string,
    CFDictionaryRef options);
/* The line of stringRange; a length of 0 goes to the string's end. */
CORETEXT_EXPORT CTLineRef CTTypesetterCreateLine(CTTypesetterRef typesetter, CFRange stringRange);
CORETEXT_EXPORT CTLineRef CTTypesetterCreateLineWithOffset(CTTypesetterRef typesetter, CFRange stringRange,
    double offset);
/* How many characters from startIndex fit in width: at a word break if
   there is one, otherwise at a cluster break. */
CORETEXT_EXPORT CFIndex CTTypesetterSuggestLineBreak(CTTypesetterRef typesetter, CFIndex startIndex, double width);
CORETEXT_EXPORT CFIndex CTTypesetterSuggestLineBreakWithOffset(CTTypesetterRef typesetter, CFIndex startIndex,
    double width, double offset);
/* How many characters from startIndex fit in width, at a cluster break
   (at least one cluster). */
CORETEXT_EXPORT CFIndex CTTypesetterSuggestClusterBreak(CTTypesetterRef typesetter, CFIndex startIndex, double width);
CORETEXT_EXPORT CFIndex CTTypesetterSuggestClusterBreakWithOffset(CTTypesetterRef typesetter, CFIndex startIndex,
    double width, double offset);

#ifdef __cplusplus
}
#endif

#endif
