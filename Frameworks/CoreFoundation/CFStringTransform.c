/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CFStringTransform: transliteration with ICU's transliterators. The
 * transforms are ICU transform IDs, as Foundation's NSStringTransform
 * constants are, and any other ICU ID works too.
 */

#include <CoreFoundation/CFString.h>
#include <stdlib.h>
#include <unicode/utrans.h>

const CFStringRef kCFStringTransformStripCombiningMarks = CFSTR("NFD; [:Mn:] Remove; NFC");
const CFStringRef kCFStringTransformToLatin = CFSTR("Any-Latin");
const CFStringRef kCFStringTransformFullwidthHalfwidth = CFSTR("Fullwidth-Halfwidth");
const CFStringRef kCFStringTransformLatinKatakana = CFSTR("Latin-Katakana");
const CFStringRef kCFStringTransformLatinHiragana = CFSTR("Latin-Hiragana");
const CFStringRef kCFStringTransformHiraganaKatakana = CFSTR("Hiragana-Katakana");
const CFStringRef kCFStringTransformMandarinLatin = CFSTR("Han-Latin");
const CFStringRef kCFStringTransformLatinHangul = CFSTR("Latin-Hangul");
const CFStringRef kCFStringTransformLatinArabic = CFSTR("Latin-Arabic");
const CFStringRef kCFStringTransformLatinHebrew = CFSTR("Latin-Hebrew");
const CFStringRef kCFStringTransformLatinThai = CFSTR("Latin-Thai");
const CFStringRef kCFStringTransformLatinCyrillic = CFSTR("Latin-Cyrillic");
const CFStringRef kCFStringTransformLatinGreek = CFSTR("Latin-Greek");
const CFStringRef kCFStringTransformToXMLHex = CFSTR("Any-Hex/XML");
const CFStringRef kCFStringTransformToUnicodeName = CFSTR("Any-Name");
const CFStringRef kCFStringTransformStripDiacritics = CFSTR("NFD; [:Nonspacing Mark:] Remove; NFC");

/* The range (all of string when NULL) is transformed in place; range is
   set to the transformed text's range. False for an unknown transform or
   one that cannot be reversed. */
Boolean
CFStringTransform(CFMutableStringRef string, CFRange *range, CFStringRef transform, Boolean reverse)
{
    CFIndex length, idLength;
    CFRange whole;
    UniChar *idChars, *text;
    UErrorCode status = U_ZERO_ERROR;
    UTransliterator *t;
    int32_t capacity, textLength, limit;

    if (string == NULL || transform == NULL)
        return false;
    length = CFStringGetLength(string);
    whole = (range != NULL) ? *range : CFRangeMake(0, length);
    if (whole.location < 0 || whole.length < 0 || whole.location + whole.length > length)
        return false;

    idLength = CFStringGetLength(transform);
    idChars = malloc((size_t)(idLength + 1) * sizeof(UniChar));
    if (idChars == NULL)
        return false;
    CFStringGetCharacters(transform, CFRangeMake(0, idLength), idChars);
    t = utrans_openU((const UChar *)idChars, (int32_t)idLength, reverse ? UTRANS_REVERSE : UTRANS_FORWARD,
                     NULL, 0, NULL, &status);
    free(idChars);
    if (U_FAILURE(status) || t == NULL)
        return false;

    /* The whole string is given so that context outside the range counts. */
    capacity = (int32_t)(length * 4 + 16);
    for (;;) {
        text = malloc((size_t)capacity * sizeof(UniChar));
        if (text == NULL) {
            utrans_close(t);
            return false;
        }
        CFStringGetCharacters(string, CFRangeMake(0, length), text);
        textLength = (int32_t)length;
        limit = (int32_t)(whole.location + whole.length);
        status = U_ZERO_ERROR;
        utrans_transUChars(t, (UChar *)text, &textLength, capacity, (int32_t)whole.location, &limit, &status);
        if (status != U_BUFFER_OVERFLOW_ERROR)
            break;
        free(text);
        capacity = textLength + 16;
    }
    utrans_close(t);
    if (U_FAILURE(status)) {
        free(text);
        return false;
    }

    CFStringRef replacement = CFStringCreateWithCharacters(kCFAllocatorDefault,
        text + whole.location, limit - (int32_t)whole.location);
    free(text);
    if (replacement == NULL)
        return false;
    CFStringReplace(string, whole, replacement);
    CFRelease(replacement);
    if (range != NULL)
        range->length = limit - whole.location;
    return true;
}
