/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * The Security.framework types SystemConfiguration and configd name.
 * Minkar has no Security.framework yet: SystemConfiguration reaches its
 * functions through dlopen (dy_framework.c), which fails cleanly, so only
 * the types and the few prototypes they use are declared here.
 */

#ifndef _MINKAR_COMPAT_SECURITY_H
#define _MINKAR_COMPAT_SECURITY_H

#include <sys/types.h>
#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

enum {
	errSecSuccess		= 0,
	errSecUnimplemented	= -4,
	errSecParam		= -50,
	errSecAllocate		= -108,
	errSecNotAvailable	= -25291,
	errSecDuplicateItem	= -25299,
	errSecItemNotFound	= -25300,
};

/* Authorization */
typedef const struct AuthorizationOpaqueRef *AuthorizationRef;
typedef UInt32 AuthorizationFlags;
enum {
	errAuthorizationSuccess		= 0,
	errAuthorizationInvalidRef	= -60002,
	kAuthorizationFlagDefaults	= 0,
};
#define kAuthorizationExternalFormLength 32
typedef struct {
	char	bytes[kAuthorizationExternalFormLength];
} AuthorizationExternalForm;

typedef struct {
	const char	*name;
	size_t		valueLength;
	void		*value;
	UInt32		flags;
} AuthorizationItem;
typedef struct {
	UInt32			count;
	AuthorizationItem	*items;
} AuthorizationItemSet;
typedef AuthorizationItemSet AuthorizationRights;
typedef AuthorizationItemSet AuthorizationEnvironment;
#define kAuthorizationEmptyEnvironment	NULL
enum {
	kAuthorizationFlagInteractionAllowed	= (1 << 0),
	kAuthorizationFlagExtendRights		= (1 << 1),
	kAuthorizationFlagPartialRights		= (1 << 2),
	kAuthorizationFlagDestroyRights		= (1 << 3),
	kAuthorizationFlagPreAuthorize		= (1 << 4),
};

OSStatus AuthorizationCreate(const AuthorizationRights *rights,
    const AuthorizationEnvironment *environment, AuthorizationFlags flags,
    AuthorizationRef *authorization);
OSStatus AuthorizationCreateFromExternalForm(const AuthorizationExternalForm *extForm,
    AuthorizationRef *authorization);
OSStatus AuthorizationMakeExternalForm(AuthorizationRef authorization,
    AuthorizationExternalForm *extForm);
OSStatus AuthorizationFree(AuthorizationRef authorization, AuthorizationFlags flags);

/* Keychains, their items and access */
typedef struct OpaqueSecKeychainRef *SecKeychainRef;
typedef struct OpaqueSecKeychainItemRef *SecKeychainItemRef;
typedef struct OpaqueSecAccessRef *SecAccessRef;
typedef struct OpaqueSecTrustedApplicationRef *SecTrustedApplicationRef;
typedef struct __SecCertificate *SecCertificateRef;
typedef struct __SecTask *SecTaskRef;

typedef FourCharCode SecItemClass;
enum {
	kSecGenericPasswordItemClass	= 'genp',
};

typedef FourCharCode SecKeychainAttrType;
typedef struct SecKeychainAttribute {
	SecKeychainAttrType	tag;
	UInt32			length;
	void			*data;
} SecKeychainAttribute;
typedef struct SecKeychainAttributeList {
	UInt32			count;
	SecKeychainAttribute	*attr;
} SecKeychainAttributeList;
enum {
	kSecCreationDateItemAttr	= 'cdat',
	kSecModDateItemAttr		= 'mdat',
	kSecDescriptionItemAttr		= 'desc',
	kSecCommentItemAttr		= 'icmt',
	kSecLabelItemAttr		= 'labl',
	kSecAccountItemAttr		= 'acct',
	kSecServiceItemAttr		= 'svce',
	kSecGenericItemAttr		= 'gena',
};

typedef enum {
	kSecPreferencesDomainUser,
	kSecPreferencesDomainSystem,
	kSecPreferencesDomainCommon,
	kSecPreferencesDomainDynamic,
} SecPreferencesDomain;

typedef UInt32 SecAccessOwnerType;
enum {
	kSecUseOnlyUID	= 1,
	kSecUseOnlyGID	= 2,
	kSecHonorRoot	= 0x100,
	kSecMatchBits	= (kSecUseOnlyUID | kSecUseOnlyGID),
};

SecTaskRef SecTaskCreateFromSelf(CFAllocatorRef allocator);
CFTypeRef SecTaskCopyValueForEntitlement(SecTaskRef task, CFStringRef entitlement,
    CFErrorRef *error);

/* Reached through dlopen in SystemConfiguration (dy_framework.c). */
extern const CFStringRef kSecAttrService;
extern const CFStringRef kSecClass;
extern const CFStringRef kSecClassGenericPassword;
extern const CFStringRef kSecMatchLimit;
extern const CFStringRef kSecMatchLimitAll;
extern const CFStringRef kSecMatchSearchList;
extern const CFStringRef kSecReturnRef;
extern const CFStringRef kSecGuestAttributePid;
extern const CFStringRef kSecCodeInfoIdentifier;
extern const CFStringRef kSecCodeInfoUnique;
extern const CFStringRef kSecPropertyKeyValue;
extern const CFStringRef kSecPropertyKeyLabel;

OSStatus SecAccessCreate(CFStringRef descriptor, CFArrayRef trustedlist,
    SecAccessRef *accessRef);
SecAccessRef SecAccessCreateWithOwnerAndACL(uid_t userId, gid_t groupId,
    SecAccessOwnerType ownerType, CFArrayRef acls, CFErrorRef *error);
OSStatus SecItemCopyMatching(CFDictionaryRef query, CFTypeRef *result);
OSStatus SecKeychainCopyDomainDefault(SecPreferencesDomain domain,
    SecKeychainRef *keychain);
OSStatus SecKeychainOpen(const char *pathName, SecKeychainRef *keychain);
OSStatus SecKeychainSetDomainDefault(SecPreferencesDomain domain,
    SecKeychainRef keychain);
OSStatus SecKeychainItemCopyContent(SecKeychainItemRef itemRef,
    SecItemClass *itemClass, SecKeychainAttributeList *attrList,
    UInt32 *length, void **outData);
OSStatus SecKeychainItemCreateFromContent(SecItemClass itemClass,
    SecKeychainAttributeList *attrList, UInt32 length, const void *data,
    SecKeychainRef keychainRef, SecAccessRef initialAccess,
    SecKeychainItemRef *itemRef);
OSStatus SecKeychainItemDelete(SecKeychainItemRef itemRef);
OSStatus SecKeychainItemFreeContent(SecKeychainAttributeList *attrList, void *data);
OSStatus SecKeychainItemModifyContent(SecKeychainItemRef itemRef,
    const SecKeychainAttributeList *attrList, UInt32 length, const void *data);
OSStatus SecTrustedApplicationCreateFromPath(const char *path,
    SecTrustedApplicationRef *app);
CFArrayRef SecCertificateCopyProperties(SecCertificateRef certRef);
SecCertificateRef SecCertificateCreateWithData(CFAllocatorRef allocator, CFDataRef data);

__END_DECLS

#endif /* !_MINKAR_COMPAT_SECURITY_H */
