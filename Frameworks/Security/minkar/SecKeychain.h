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

#ifdef __cplusplus
}
#endif

#endif
