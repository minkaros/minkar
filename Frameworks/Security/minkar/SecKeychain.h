/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The older keychain calls, over the same items as SecItem.h.
 */

#ifndef _SECURITY_SECKEYCHAIN_H_
#define _SECURITY_SECKEYCHAIN_H_

#include "SecBase.h"

#ifdef __cplusplus
extern "C" {
#endif

CF_IMPLICIT_BRIDGING_ENABLED

enum {
    kSecInternetPasswordItemClass = 'inet',
    kSecGenericPasswordItemClass = 'genp',
    kSecCertificateItemClass = 0x80001000
};

enum {
    kSecCreationDateItemAttr = 'cdat',
    kSecModDateItemAttr = 'mdat',
    kSecDescriptionItemAttr = 'desc',
    kSecCommentItemAttr = 'icmt',
    kSecCreatorItemAttr = 'crtr',
    kSecTypeItemAttr = 'type',
    kSecLabelItemAttr = 'labl',
    kSecInvisibleItemAttr = 'invi',
    kSecAccountItemAttr = 'acct',
    kSecServiceItemAttr = 'svce',
    kSecGenericItemAttr = 'gena',
    kSecSecurityDomainItemAttr = 'sdmn',
    kSecServerItemAttr = 'srvr',
    kSecPathItemAttr = 'path',
    kSecPortItemAttr = 'port',
    kSecProtocolItemAttr = 'ptcl'
};

CFTypeID SecKeychainGetTypeID(void);
CFTypeID SecKeychainItemGetTypeID(void);
CFTypeID SecKeychainSearchGetTypeID(void);

OSStatus SecKeychainCopyDefault(SecKeychainRef *keychain);
OSStatus SecKeychainSearchCreateFromAttributes(CFTypeRef keychainOrArray, SecItemClass itemClass,
                                               const SecKeychainAttributeList *attrList, SecKeychainSearchRef *searchRef);
OSStatus SecKeychainSearchCopyNext(SecKeychainSearchRef searchRef, SecKeychainItemRef *itemRef);
OSStatus SecKeychainItemCopyContent(SecKeychainItemRef itemRef, SecItemClass *itemClass, SecKeychainAttributeList *attrList,
                                    UInt32 *length, void **outData);
OSStatus SecKeychainItemFreeContent(SecKeychainAttributeList *attrList, void *data);
OSStatus SecKeychainItemCopyAttributesAndData(SecKeychainItemRef itemRef, SecKeychainAttributeInfo *info, SecItemClass *itemClass,
                                              SecKeychainAttributeList **attrList, UInt32 *length, void **outData);
OSStatus SecKeychainItemFreeAttributesAndData(SecKeychainAttributeList *attrList, void *data);
OSStatus SecKeychainItemModifyAttributesAndData(SecKeychainItemRef itemRef, const SecKeychainAttributeList *attrList,
                                                UInt32 length, const void *data);
OSStatus SecKeychainItemDelete(SecKeychainItemRef itemRef);
OSStatus SecKeychainAddGenericPassword(SecKeychainRef keychain, UInt32 serviceNameLength, const char *serviceName,
                                       UInt32 accountNameLength, const char *accountName, UInt32 passwordLength,
                                       const void *passwordData, SecKeychainItemRef *itemRef);
OSStatus SecKeychainFindGenericPassword(CFTypeRef keychainOrArray, UInt32 serviceNameLength, const char *serviceName,
                                        UInt32 accountNameLength, const char *accountName, UInt32 *passwordLength,
                                        void **passwordData, SecKeychainItemRef *itemRef);

/* The Keychain Manager's older names of the errors (the same values). */
enum {
    errKCNotAvailable = -25291, errKCReadOnly = -25292, errKCAuthFailed = -25293,
    errKCNoSuchKeychain = -25294, errKCInvalidKeychain = -25295, errKCDuplicateKeychain = -25296,
    errKCDuplicateCallback = -25297, errKCInvalidCallback = -25298, errKCDuplicateItem = -25299,
    errKCItemNotFound = -25300, errKCBufferTooSmall = -25301, errKCDataTooLarge = -25302,
    errKCNoSuchAttr = -25303, errKCInvalidItemRef = -25304, errKCInvalidSearchRef = -25305,
    errKCNoSuchClass = -25306, errKCNoDefaultKeychain = -25307, errKCInteractionNotAllowed = -25308,
};

/* An attribute's format in SecKeychainAttributeInfo (CDSA's). */
#ifndef CSSM_DB_ATTRIBUTE_FORMAT_STRING
typedef uint32_t CSSM_DB_ATTRIBUTE_FORMAT;
enum {
    CSSM_DB_ATTRIBUTE_FORMAT_STRING = 0, CSSM_DB_ATTRIBUTE_FORMAT_SINT32 = 1,
    CSSM_DB_ATTRIBUTE_FORMAT_UINT32 = 2, CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM = 3,
    CSSM_DB_ATTRIBUTE_FORMAT_REAL = 4, CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE = 5,
    CSSM_DB_ATTRIBUTE_FORMAT_BLOB = 6, CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32 = 7,
    CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX = 8,
};
#endif


CF_IMPLICIT_BRIDGING_DISABLED

#ifdef __cplusplus
}
#endif

#endif
