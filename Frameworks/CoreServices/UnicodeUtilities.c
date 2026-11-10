/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Unicode collation on ICU. A collation key is laid out as
 *   long options, long length, then the ICU sort key (length bytes and a NUL),
 * so it is compared with strcmp from its third long on; programs that read
 * keys (GLib's g_utf8_collate_key) expect that layout.
 */

#include <stdlib.h>
#include <string.h>

#include <CoreServices/MacErrors.h>
#include <unicode/ucol.h>
#include <unicode/uloc.h>

#include "UnicodeUtilities.h"

struct OpaqueCollatorRef {
    UCollator *ucol;
    UCCollateOptions options;
};

#define KNOWN_OPTIONS (kUCCollateComposeInsensitiveMask | \
    kUCCollateWidthInsensitiveMask | kUCCollateCaseInsensitiveMask | \
    kUCCollateDiacritInsensitiveMask | kUCCollatePunctuationSignificantMask | \
    kUCCollateDigitsOverrideMask | kUCCollateDigitsAsNumberMask)

static UCollator *openCollator(UCCollateOptions options, int noLocale)
{
    UErrorCode err = U_ZERO_ERROR;
    UCollator *ucol = ucol_open(noLocale ? "" : uloc_getDefault(), &err);

    if (U_FAILURE(err))
        return NULL;

    /* Case and diacritics: what each strength ignores. */
    int caseInsensitive = (options & kUCCollateCaseInsensitiveMask) != 0;
    int diacritInsensitive = (options & kUCCollateDiacritInsensitiveMask) != 0;
    if (diacritInsensitive) {
        ucol_setStrength(ucol, UCOL_PRIMARY);
        if (!caseInsensitive)
            ucol_setAttribute(ucol, UCOL_CASE_LEVEL, UCOL_ON, &err);
    } else if (caseInsensitive) {
        ucol_setStrength(ucol, UCOL_SECONDARY);
    } else {
        ucol_setStrength(ucol, UCOL_TERTIARY);
    }

    /* Composed and decomposed forms are the same text. */
    ucol_setAttribute(ucol, UCOL_NORMALIZATION_MODE,
                      (options & kUCCollateComposeInsensitiveMask) ? UCOL_ON : UCOL_OFF,
                      &err);
    /* Punctuation and spaces are ignored unless they are significant. */
    ucol_setAttribute(ucol, UCOL_ALTERNATE_HANDLING,
                      (options & kUCCollatePunctuationSignificantMask) ?
                      UCOL_NON_IGNORABLE : UCOL_SHIFTED, &err);
    /* Runs of digits compare as numbers. */
    ucol_setAttribute(ucol, UCOL_NUMERIC_COLLATION,
                      (options & kUCCollateDigitsAsNumberMask) ? UCOL_ON : UCOL_OFF,
                      &err);

    if (U_FAILURE(err)) {
        ucol_close(ucol);
        return NULL;
    }
    return ucol;
}

OSStatus UCCreateCollator(LocaleRef locale, LocaleOperationVariant opVariant,
                          UCCollateOptions options, CollatorRef *collatorRef)
{
    (void)locale;      /* LocaleRefs are not implemented: the user's locale */
    (void)opVariant;
    if (collatorRef == NULL)
        return paramErr;
    *collatorRef = NULL;
    if (options & ~KNOWN_OPTIONS)
        return kCollateInvalidOptions;

    CollatorRef c = calloc(1, sizeof(*c));
    if (c == NULL)
        return memFullErr;
    c->ucol = openCollator(options, 0);
    if (c->ucol == NULL) {
        free(c);
        return kCollateAttributesNotFoundErr;
    }
    c->options = options;
    *collatorRef = c;
    return noErr;
}

OSStatus UCDisposeCollator(CollatorRef *collatorRef)
{
    if (collatorRef == NULL)
        return paramErr;
    if (*collatorRef != NULL) {
        ucol_close((*collatorRef)->ucol);
        free(*collatorRef);
        *collatorRef = NULL;
    }
    return noErr;
}

static void setResult(UCollationResult r, Boolean *equivalent, SInt32 *order)
{
    SInt32 o = r == UCOL_LESS ? -1 : r == UCOL_GREATER ? 1 : 0;

    if (order != NULL)
        *order = o;
    if (equivalent != NULL)
        *equivalent = o == 0;
}

OSStatus UCCompareText(CollatorRef collatorRef, const UniChar *text1Ptr,
                       UniCharCount text1Length, const UniChar *text2Ptr,
                       UniCharCount text2Length, Boolean *equivalent,
                       SInt32 *order)
{
    if (collatorRef == NULL || collatorRef->ucol == NULL)
        return kCollateInvalidCollationRef;
    if ((text1Ptr == NULL && text1Length != 0) ||
        (text2Ptr == NULL && text2Length != 0))
        return paramErr;
    setResult(ucol_strcoll(collatorRef->ucol, (const UChar *)text1Ptr,
                           (int32_t)text1Length, (const UChar *)text2Ptr,
                           (int32_t)text2Length), equivalent, order);
    return noErr;
}

static OSStatus compareWith(UCCollateOptions options, int noLocale,
                            const UniChar *text1Ptr, UniCharCount text1Length,
                            const UniChar *text2Ptr, UniCharCount text2Length,
                            Boolean *equivalent, SInt32 *order)
{
    if (options & ~KNOWN_OPTIONS)
        return kCollateInvalidOptions;

    struct OpaqueCollatorRef c = { openCollator(options, noLocale), options };
    if (c.ucol == NULL)
        return kCollateAttributesNotFoundErr;
    OSStatus st = UCCompareText(&c, text1Ptr, text1Length, text2Ptr,
                                text2Length, equivalent, order);
    ucol_close(c.ucol);
    return st;
}

OSStatus UCCompareTextDefault(UCCollateOptions options, const UniChar *text1Ptr,
                              UniCharCount text1Length, const UniChar *text2Ptr,
                              UniCharCount text2Length, Boolean *equivalent,
                              SInt32 *order)
{
    return compareWith(options, 0, text1Ptr, text1Length, text2Ptr,
                       text2Length, equivalent, order);
}

OSStatus UCCompareTextNoLocale(UCCollateOptions options, const UniChar *text1Ptr,
                               UniCharCount text1Length, const UniChar *text2Ptr,
                               UniCharCount text2Length, Boolean *equivalent,
                               SInt32 *order)
{
    return compareWith(options, 1, text1Ptr, text1Length, text2Ptr,
                       text2Length, equivalent, order);
}

OSStatus UCGetCollationKey(CollatorRef collatorRef, const UniChar *textPtr,
                           UniCharCount textLength, ItemCount maxKeySize,
                           ItemCount *actualKeySize,
                           UCCollationValue collationKey[])
{
    if (collatorRef == NULL || collatorRef->ucol == NULL)
        return kCollateInvalidCollationRef;
    if ((textPtr == NULL && textLength != 0) || actualKeySize == NULL ||
        collationKey == NULL)
        return paramErr;

    /* The sort key's length, its NUL included. */
    int32_t sortLen = ucol_getSortKey(collatorRef->ucol, (const UChar *)textPtr,
                                      (int32_t)textLength, NULL, 0);
    size_t bytes = 2 * sizeof(long) + (size_t)sortLen;
    ItemCount values = (bytes + sizeof(UCCollationValue) - 1) /
                       sizeof(UCCollationValue);

    *actualKeySize = values;
    if (values > maxKeySize)
        return kCollateBufferTooSmall;

    memset(collationKey, 0, values * sizeof(UCCollationValue));
    long header[2] = { (long)collatorRef->options, (long)(sortLen - 1) };
    memcpy(collationKey, header, sizeof(header));
    ucol_getSortKey(collatorRef->ucol, (const UChar *)textPtr,
                    (int32_t)textLength,
                    (uint8_t *)collationKey + sizeof(header), sortLen);
    return noErr;
}

OSStatus UCCompareCollationKeys(const UCCollationValue *key1Ptr,
                                ItemCount key1Length,
                                const UCCollationValue *key2Ptr,
                                ItemCount key2Length, Boolean *equivalent,
                                SInt32 *order)
{
    size_t minLen = 2 * sizeof(long) / sizeof(UCCollationValue);

    if (key1Ptr == NULL || key2Ptr == NULL || key1Length < minLen ||
        key2Length < minLen)
        return paramErr;

    const long *h1 = (const long *)key1Ptr;
    const long *h2 = (const long *)key2Ptr;
    if (h1[0] != h2[0])
        return kCollateInvalidOptions;   /* keys from different options */

    int r = strcmp((const char *)&h1[2], (const char *)&h2[2]);
    setResult(r < 0 ? UCOL_LESS : r > 0 ? UCOL_GREATER : UCOL_EQUAL,
              equivalent, order);
    return noErr;
}
