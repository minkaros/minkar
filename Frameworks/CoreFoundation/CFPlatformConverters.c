/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*	CFPlatformConverters.c
	Copyright (c) 1998-2014, Apple Inc. All rights reserved.
	Responsibility: Aki Inoue
*/

#include "CFInternal.h"
#include <CoreFoundation/CFString.h>
#include "CFStringEncodingConverterExt.h"
#include <CoreFoundation/CFStringEncodingExt.h>
#include "CFUniChar.h"
#include "CFUnicodeDecomposition.h"
#include "CFStringEncodingConverterPriv.h"
#include "CFICUConverters.h"


CF_INLINE bool __CFIsPlatformConverterAvailable(int encoding) {

    return false;
}

static const CFStringEncodingConverter __CFICUBootstrap = {
    NULL /* toBytes */, NULL /* toUnicode */, 6 /* maxBytesPerChar */, 4 /* maxDecomposedCharLen */,
    kCFStringEncodingConverterICU /* encodingClass */,
    NULL /* toBytesLen */, NULL /* toUnicodeLen */, NULL /* toBytesFallback */,
    NULL /* toUnicodeFallback */, NULL /* toBytesPrecompose */, NULL, /* isValidCombiningChar */
};

static const CFStringEncodingConverter __CFPlatformBootstrap = {
    NULL /* toBytes */, NULL /* toUnicode */, 6 /* maxBytesPerChar */, 4 /* maxDecomposedCharLen */,
    kCFStringEncodingConverterPlatformSpecific /* encodingClass */,
    NULL /* toBytesLen */, NULL /* toUnicodeLen */, NULL /* toBytesFallback */,
    NULL /* toUnicodeFallback */, NULL /* toBytesPrecompose */, NULL, /* isValidCombiningChar */
};

CF_PRIVATE const CFStringEncodingConverter *__CFStringEncodingGetExternalConverter(uint32_t encoding) {

    // we prefer Text Encoding Converter ICU since it's more reliable
    if (__CFIsPlatformConverterAvailable(encoding)) {
        return &__CFPlatformBootstrap;
    } else {
        if (__CFStringEncodingGetICUName(encoding)) {
            return &__CFICUBootstrap;
        }
        return NULL;
    }
}

CF_PRIVATE CFStringEncoding *__CFStringEncodingCreateListOfAvailablePlatformConverters(CFAllocatorRef allocator, CFIndex *numberOfConverters) {

    return NULL;
}

CF_PRIVATE CFIndex __CFStringEncodingPlatformUnicodeToBytes(uint32_t encoding, uint32_t flags, const UniChar *characters, CFIndex numChars, CFIndex *usedCharLen, uint8_t *bytes, CFIndex maxByteLen, CFIndex *usedByteLen) {

    return kCFStringEncodingConverterUnavailable;
}

CF_PRIVATE CFIndex __CFStringEncodingPlatformBytesToUnicode(uint32_t encoding, uint32_t flags, const uint8_t *bytes, CFIndex numBytes, CFIndex *usedByteLen, UniChar *characters, CFIndex maxCharLen, CFIndex *usedCharLen) {

    return kCFStringEncodingConverterUnavailable;
}

CF_PRIVATE CFIndex __CFStringEncodingPlatformCharLengthForBytes(uint32_t encoding, uint32_t flags, const uint8_t *bytes, CFIndex numBytes) {
    CFIndex usedCharLen;
    return (__CFStringEncodingPlatformBytesToUnicode(encoding, flags, bytes, numBytes, NULL, NULL, 0, &usedCharLen) == kCFStringEncodingConversionSuccess ? usedCharLen : 0);
}

CF_PRIVATE CFIndex __CFStringEncodingPlatformByteLengthForCharacters(uint32_t encoding, uint32_t flags, const UniChar *characters, CFIndex numChars) {
    CFIndex usedByteLen;
    return (__CFStringEncodingPlatformUnicodeToBytes(encoding, flags, characters, numChars, NULL, NULL, 0, &usedByteLen) == kCFStringEncodingConversionSuccess ? usedByteLen : 0);
}

#undef __CFCarbonCore_GetTextEncodingBase0

