/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Unicode utilities: collation (comparing text and building sort keys the
 * way the user's language orders it), on ICU's collator.
 */

#ifndef __UNICODEUTILITIES__
#define __UNICODEUTILITIES__

#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueCollatorRef *CollatorRef;
typedef struct OpaqueLocaleRef *LocaleRef;
typedef UInt32 LocaleOperationVariant;
typedef UInt32 UCCollateOptions;
typedef UInt32 UCCollationValue;

/* Collation options. */
enum {
    kUCCollateComposeInsensitiveMask = 1UL << 1,
    kUCCollateWidthInsensitiveMask = 1UL << 2,
    kUCCollateCaseInsensitiveMask = 1UL << 3,
    kUCCollateDiacritInsensitiveMask = 1UL << 4,
    kUCCollatePunctuationSignificantMask = 1UL << 15,
    kUCCollateDigitsOverrideMask = 1UL << 16,
    kUCCollateDigitsAsNumberMask = 1UL << 17
};

enum {
    kUCCollateStandardOptions = kUCCollateComposeInsensitiveMask |
                                kUCCollateWidthInsensitiveMask
};

/* Collation errors. */
enum {
    kCollateAttributesNotFoundErr = -29500,
    kCollateInvalidOptions = -29501,
    kCollateMissingUnicodeTableErr = -29502,
    kCollateUnicodeConvertFailedErr = -29506,
    kCollatePatternNotFoundErr = -29507,
    kCollateInvalidChar = -29508,
    kCollateBufferTooSmall = -29509,
    kCollateInvalidCollationRef = -29510
};

/* A collator for a locale (NULL: the user's) and options. */
OSStatus UCCreateCollator(LocaleRef locale, LocaleOperationVariant opVariant,
                          UCCollateOptions options, CollatorRef *collatorRef);
OSStatus UCDisposeCollator(CollatorRef *collatorRef);

/* Compare two texts: order is -1, 0 or 1; equivalent when it is 0. Either
 * result pointer may be NULL. */
OSStatus UCCompareText(CollatorRef collatorRef, const UniChar *text1Ptr,
                       UniCharCount text1Length, const UniChar *text2Ptr,
                       UniCharCount text2Length, Boolean *equivalent,
                       SInt32 *order);
OSStatus UCCompareTextDefault(UCCollateOptions options, const UniChar *text1Ptr,
                              UniCharCount text1Length, const UniChar *text2Ptr,
                              UniCharCount text2Length, Boolean *equivalent,
                              SInt32 *order);
OSStatus UCCompareTextNoLocale(UCCollateOptions options, const UniChar *text1Ptr,
                               UniCharCount text1Length, const UniChar *text2Ptr,
                               UniCharCount text2Length, Boolean *equivalent,
                               SInt32 *order);

/* A sort key for a text, maxKeySize UCCollationValues at most; two keys
 * compare as their texts do. kCollateBufferTooSmall when it does not fit. */
OSStatus UCGetCollationKey(CollatorRef collatorRef, const UniChar *textPtr,
                           UniCharCount textLength, ItemCount maxKeySize,
                           ItemCount *actualKeySize,
                           UCCollationValue collationKey[]);
OSStatus UCCompareCollationKeys(const UCCollationValue *key1Ptr,
                                ItemCount key1Length,
                                const UCCollationValue *key2Ptr,
                                ItemCount key2Length, Boolean *equivalent,
                                SInt32 *order);

/* Keyboard layouts (kTISPropertyUnicodeKeyLayoutData) and translating a
   key to the characters it types with them. */
typedef struct UCKeyboardLayout UCKeyboardLayout;
typedef unsigned long UniCharCount;

enum {
    kUCKeyActionDown = 0,
    kUCKeyActionUp = 1,
    kUCKeyActionAutoKey = 2,
    kUCKeyActionDisplay = 3
};

enum {
    kUCKeyTranslateNoDeadKeysBit = 0,
    kUCKeyTranslateNoDeadKeysMask = 1UL << kUCKeyTranslateNoDeadKeysBit
};

enum {
    kUCOutputBufferTooSmall = -25340,
    kUCTextBreakLocatorMissingType = -25341
};

OSStatus UCKeyTranslate(const UCKeyboardLayout *keyLayoutPtr, UInt16 virtualKeyCode, UInt16 keyAction,
    UInt32 modifierKeyState, UInt32 keyboardType, OptionBits keyTranslateOptions, UInt32 *deadKeyState,
    UniCharCount maxStringLength, UniCharCount *actualStringLength, UniChar unicodeString[]);

/* Minkar: the layout data the text input sources give. It names the
   window server's keyboard layout in use, which translates the keys. */
#define kMinkarKeyboardLayoutMagic 0x4d6b4b4cU	/* 'MkKL' */
typedef struct {
    UInt32 magic;
    UInt32 version;
    char layout[32];
    char variant[32];
} MinkarKeyboardLayoutData;

#ifdef __cplusplus
}
#endif

#endif /* __UNICODEUTILITIES__ */
