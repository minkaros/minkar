/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/*
 * Lets a web browser list the platform passkeys saved for a relying
 * party (for autofill), once the user has allowed it.
 */

#import <Foundation/Foundation.h>
#include <os/availability.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationState) {
    ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationStateAuthorized,
    ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationStateDenied,
    ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationStateNotDetermined,
} API_AVAILABLE(macos(13.3));

/* A saved platform passkey, as the browser sees it. */
API_AVAILABLE(macos(13.3))
@interface ASAuthorizationWebBrowserPlatformPublicKeyCredential : NSObject
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property (nonatomic, readonly, copy) NSString *name;
@property (nonatomic, readonly, copy, nullable) NSString *customTitle;
@property (nonatomic, readonly, copy) NSString *relyingParty;
@property (nonatomic, readonly, copy) NSData *credentialID;
@property (nonatomic, readonly, copy) NSData *userHandle;
@property (nonatomic, readonly, copy, nullable) NSString *providerName;
@end

API_AVAILABLE(macos(13.3))
@interface ASAuthorizationWebBrowserPublicKeyCredentialManager : NSObject
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (void)requestAuthorizationForPublicKeyCredentials:(void (^)(ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationState authorizationState))completionHandler;
- (void)platformCredentialsForRelyingParty:(NSString *)relyingParty
                         completionHandler:(void (^)(NSArray<ASAuthorizationWebBrowserPlatformPublicKeyCredential *> *credentials))completionHandler;
@property (nonatomic, readonly) ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationState authorizationStateForPlatformCredentials;
@end

NS_ASSUME_NONNULL_END
