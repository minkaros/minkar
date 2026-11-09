/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Keychain items by dictionary: generic and internet passwords (other
 * classes are not kept yet). SHORTCUT(19.6.0): the user's items are kept
 * in ~/Library/Keychains/items.plist, readable by the user only (mode
 * 0600), not encrypted: there is no keychain password or securityd yet.
 */

#ifndef _SECURITY_SECITEM_H_
#define _SECURITY_SECITEM_H_

#include "SecBase.h"

#ifdef __cplusplus
extern "C" {
#endif

CF_EXPORT const CFStringRef kSecClass;
CF_EXPORT const CFStringRef kSecClassGenericPassword;
CF_EXPORT const CFStringRef kSecClassInternetPassword;
CF_EXPORT const CFStringRef kSecClassCertificate;
CF_EXPORT const CFStringRef kSecClassKey;
CF_EXPORT const CFStringRef kSecClassIdentity;

CF_EXPORT const CFStringRef kSecAttrAccount;
CF_EXPORT const CFStringRef kSecAttrService;
CF_EXPORT const CFStringRef kSecAttrLabel;
CF_EXPORT const CFStringRef kSecAttrDescription;
CF_EXPORT const CFStringRef kSecAttrComment;
CF_EXPORT const CFStringRef kSecAttrGeneric;
CF_EXPORT const CFStringRef kSecAttrServer;
CF_EXPORT const CFStringRef kSecAttrProtocol;
CF_EXPORT const CFStringRef kSecAttrPort;
CF_EXPORT const CFStringRef kSecAttrPath;
CF_EXPORT const CFStringRef kSecAttrSecurityDomain;
CF_EXPORT const CFStringRef kSecAttrCreationDate;
CF_EXPORT const CFStringRef kSecAttrModificationDate;
CF_EXPORT const CFStringRef kSecAttrCreator;
CF_EXPORT const CFStringRef kSecAttrType;
CF_EXPORT const CFStringRef kSecAttrIsInvisible;
CF_EXPORT const CFStringRef kSecAttrSynchronizable;
CF_EXPORT const CFStringRef kSecAttrAccessible;
CF_EXPORT const CFStringRef kSecAttrAccessibleWhenUnlocked;
CF_EXPORT const CFStringRef kSecAttrAccessibleAfterFirstUnlock;
CF_EXPORT const CFStringRef kSecAttrAccessGroup;

CF_EXPORT const CFStringRef kSecMatchLimit;
CF_EXPORT const CFStringRef kSecMatchLimitOne;
CF_EXPORT const CFStringRef kSecMatchLimitAll;
CF_EXPORT const CFStringRef kSecMatchSearchList;
CF_EXPORT const CFStringRef kSecMatchCaseInsensitive;

CF_EXPORT const CFStringRef kSecReturnData;
CF_EXPORT const CFStringRef kSecReturnAttributes;
CF_EXPORT const CFStringRef kSecReturnRef;
CF_EXPORT const CFStringRef kSecReturnPersistentRef;
CF_EXPORT const CFStringRef kSecValueData;
CF_EXPORT const CFStringRef kSecValueRef;
CF_EXPORT const CFStringRef kSecValuePersistentRef;
CF_EXPORT const CFStringRef kSecUseKeychain;
CF_EXPORT const CFStringRef kSecUseDataProtectionKeychain;

OSStatus SecItemAdd(CFDictionaryRef attributes, CFTypeRef *result);
OSStatus SecItemCopyMatching(CFDictionaryRef query, CFTypeRef *result);
OSStatus SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate);
OSStatus SecItemDelete(CFDictionaryRef query);

#ifdef __cplusplus
}
#endif

#endif
