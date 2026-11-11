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

CF_IMPLICIT_BRIDGING_ENABLED

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
/* kSecAttrProtocol's values. */
CF_EXPORT const CFStringRef kSecAttrProtocolFTP;
CF_EXPORT const CFStringRef kSecAttrProtocolFTPAccount;
CF_EXPORT const CFStringRef kSecAttrProtocolHTTP;
CF_EXPORT const CFStringRef kSecAttrProtocolIRC;
CF_EXPORT const CFStringRef kSecAttrProtocolNNTP;
CF_EXPORT const CFStringRef kSecAttrProtocolPOP3;
CF_EXPORT const CFStringRef kSecAttrProtocolSMTP;
CF_EXPORT const CFStringRef kSecAttrProtocolSOCKS;
CF_EXPORT const CFStringRef kSecAttrProtocolIMAP;
CF_EXPORT const CFStringRef kSecAttrProtocolLDAP;
CF_EXPORT const CFStringRef kSecAttrProtocolAppleTalk;
CF_EXPORT const CFStringRef kSecAttrProtocolAFP;
CF_EXPORT const CFStringRef kSecAttrProtocolTelnet;
CF_EXPORT const CFStringRef kSecAttrProtocolSSH;
CF_EXPORT const CFStringRef kSecAttrProtocolFTPS;
CF_EXPORT const CFStringRef kSecAttrProtocolHTTPS;
CF_EXPORT const CFStringRef kSecAttrProtocolHTTPProxy;
CF_EXPORT const CFStringRef kSecAttrProtocolHTTPSProxy;
CF_EXPORT const CFStringRef kSecAttrProtocolFTPProxy;
CF_EXPORT const CFStringRef kSecAttrProtocolSMB;
CF_EXPORT const CFStringRef kSecAttrProtocolRTSP;
CF_EXPORT const CFStringRef kSecAttrProtocolRTSPProxy;
CF_EXPORT const CFStringRef kSecAttrProtocolDAAP;
CF_EXPORT const CFStringRef kSecAttrProtocolEPPC;
CF_EXPORT const CFStringRef kSecAttrProtocolIPP;
CF_EXPORT const CFStringRef kSecAttrProtocolNNTPS;
CF_EXPORT const CFStringRef kSecAttrProtocolLDAPS;
CF_EXPORT const CFStringRef kSecAttrProtocolTelnetS;
CF_EXPORT const CFStringRef kSecAttrProtocolIMAPS;
CF_EXPORT const CFStringRef kSecAttrProtocolIRCS;
CF_EXPORT const CFStringRef kSecAttrProtocolPOP3S;
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


CF_IMPLICIT_BRIDGING_DISABLED

#ifdef __cplusplus
}
#endif

#endif
